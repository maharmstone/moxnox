// SPDX-License-Identifier: GPL-2.0-only
module;

#include <shared_mutex>
#include <span>
#include <optional>
#include <string_view>
#include <limits.h>
#include <mutex>
#include <atomic>
#include <memory>
#include <stddef.h>
#include <stdint.h>
#include <signal.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include "assert.h"
#include "trace.h"

module vfs;

import print;
import bsod;
import cpio;
import config;
import unicode;
import guid;
import cmdline;
import thread;
import visit;
import handle;
import crt;
import chardev;
import moxnox_ioctls;
import vec;
import net;
import rangelock;
import trace;
import disable_interrupts;
import kernel_time_page;
import pipe;
import process;
import event;
import scheduler;
import interrupts;

using namespace std;

class vfsctl : public chardev {
public:
    vfsctl() { }

    nt_status read(span<const span<uint8_t>>, uint64_t&) override {
        return nt_status::NOT_IMPLEMENTED;
    }

    nt_status write(span<const span<const uint8_t>>, uint64_t&) override {
        return nt_status::NOT_IMPLEMENTED;
    }

    nt_status ioctl(uint32_t code, span<const uint8_t> inbuf,
                    span<uint8_t> outbuf, uintptr_t& written) override;
};

struct mount_point {
    mount_point(dentry& d) : d(d) { }

    ~mount_point() {
        if (vol)
            vol->release();
    }

    void acquire() {
        refcount++;
    }

    void release() {
        if (--refcount == 0)
            delete this;
    }

    list_entry le;
    uint64_t num;
    filesystem_recognizer* type;
    volume* vol = nullptr;
    dentry& d;
    atomic<unsigned int> refcount = 1;
};

struct posix_lock {
    list_entry le;
    bool write;
    uint64_t start;
    uint64_t length;
};

struct posix_lock_proc {
    posix_lock_proc(process& proc) : proc(proc) {
        proc.acquire();
    }

    ~posix_lock_proc() {
        while (!is_list_empty(locks)) {
            auto& pl = containing_record<posix_lock>(locks.next);

            remove_entry_list(pl.le);
            delete &pl;
        }

        proc.release();
    }

    list_entry le;
    process& proc;
    list_entry locks = { &locks, &locks };
};

static object_ref vfsctl_obj;

static rwsem mount_points_lock;
static list_entry mount_points { &mount_points, &mount_points };
static atomic<uint64_t> mount_point_num;

static ::filesystem* do_mount_root(string_view cmdline_root,
                                   filesystem_recognizer*& type,
                                   volume*& volptr) {
    ::filesystem* ret = nullptr;

    if (cmdline_root.empty())
        return nullptr;

    static const char prefix[] = "/dev/";

    if (cmdline_root.size() <= sizeof(prefix) - 1 ||
        string_view(cmdline_root.data(), sizeof(prefix) - 1) != prefix) {
        bsod_format("malformed root option {}", cmdline_root);
    }

    cmdline_root = string_view(cmdline_root.data() + sizeof(prefix) - 1,
                                cmdline_root.size() - sizeof(prefix) + 1);

    object_ref ref;

    auto status = open_object(cmdline_root, ref);
    if (!success(status))
        bsod_format("Error {} while trying to open root volume /dev/{}", status, cmdline_root);

    if (ref.get().type != object_type::volume) {
        bsod_format("root volume /dev/{} is not a block device",
                    cmdline_root);
    }

    auto& vol = static_cast<volume&>(ref.get());

    {
        lock_guard lock(vol.mount_lock);

        if (!vol.filesystem) {
            for (auto le = filesystem_types.next; le != &filesystem_types; le = le->next) {
                auto& r = containing_record<filesystem_recognizer>(le);

                if (!r.try_mount_volume)
                    continue;

                auto status = r.try_mount_volume(vol, 0);
                if (success(status)) {
                    type = &r;
                    break;
                }

                if (status == nt_status::UNRECOGNIZED_VOLUME)
                    continue;

                bsod_format("probe function for {} returned {} for /dev/{}\n",
                            r.name, status, cmdline_root);
            }
        }

        if (!vol.filesystem) {
            bsod_format("no recognized filesystem found on /dev/{}",
                        cmdline_root);
        }

        ret = &vol.filesystem->get();

        vol.acquire();
        volptr = &vol;

        return ret;
    }

    return nullptr;
}

static uint64_t unix_to_system_time(uint64_t t) {
    return (t * 10000000) + 116444736000000000;
}

static nt_status extract_initrd_file(string_view path, uint64_t mode,
                                     span<const uint8_t> contents,
                                     uint64_t mtime) {
    if (!S_ISDIR(mode) && !S_ISREG(mode) && !S_ISLNK(mode))
        return nt_status::SUCCESS;

    // ignore root directory
    if (S_ISDIR(mode) && path == ".")
        return nt_status::SUCCESS;

    object_ref inorw;
    optional<reference_wrapper<dentry>> drw;
    unsigned int create_result;
    unique_ptr<char[]> rdbbuf;

    auto status = open_inode(path, *root_dentry, inorw, drw, FILE_CREATE,
                             S_ISDIR(mode) ? FILE_DIRECTORY_FILE : 0,
                             create_result, false);
    if (!success(status))
        return status;

    auto& ino = static_cast<inode&>(inorw.get());
    FILE_BASIC_INFORMATION fbi;

    memset(&fbi, 0, sizeof(fbi));

    fbi.LastWriteTime.QuadPart = (int64_t)unix_to_system_time(mtime);

    status = ino.set_information(span((const uint8_t*)&fbi, sizeof(fbi)), drw->get(),
                                 FILE_INFORMATION_CLASS::BasicInformation);

    if (drw)
        drw->get().release();

    if (!success(status))
        return status;

    if (S_ISREG(mode)) {
        uint64_t offset = 0, written;

        while (!contents.empty()) {
            status = ino.write(span(&contents, 1), offset, written);
            if (!success(status))
                return status;

            contents = span(contents.data() + written, contents.size() - written);
            offset += written;
        }
    } else if (S_ISLNK(mode)) {
        auto u16targetlen = utf8_to_utf16_len(string_view((char*)contents.data(), contents.size()));
        size_t rdblen = offsetof(REPARSE_DATA_BUFFER, SymbolicLinkReparseBuffer.PathBuffer) + (u16targetlen * sizeof(char16_t));

        rdbbuf.reset(new char[rdblen]);
        if (!rdbbuf) {
            print("malloc failed\n");
            return nt_status::INSUFFICIENT_RESOURCES;
        }

        auto& rdb = *(REPARSE_DATA_BUFFER*)rdbbuf.get();

        rdb.ReparseTag = IO_REPARSE_TAG_SYMLINK;
        rdb.ReparseDataLength = 0;
        rdb.Reserved = 0;
        rdb.SymbolicLinkReparseBuffer.SubstituteNameOffset = 0;
        rdb.SymbolicLinkReparseBuffer.SubstituteNameLength = (uint16_t)(u16targetlen * sizeof(char16_t));
        rdb.SymbolicLinkReparseBuffer.PrintNameOffset = 0;
        rdb.SymbolicLinkReparseBuffer.PrintNameLength = 0;
        rdb.SymbolicLinkReparseBuffer.Flags = 0;

        utf8_to_utf16(span(rdb.SymbolicLinkReparseBuffer.PathBuffer, u16targetlen),
                      string_view((char*)contents.data(), contents.size()));

        uint32_t retlen;

        status = ino.fs_control(FSCTL_SET_REPARSE_POINT, span((uint8_t*)&rdb, rdblen),
                                span<uint8_t>(), retlen, nullptr);
        if (!success(status)) {
            print("FSCTL_SET_REPARSE_POINT returned {}\n", status);
            return status;
        }
    }

    return nt_status::SUCCESS;
}

static bool extract_initrd() {
    filesystem_recognizer* tmpfs = nullptr;

    // FIXME - spinlock for this once we have module system
    for (auto le = filesystem_types.next; le != &filesystem_types; le = le->next) {
        auto& r = containing_record<filesystem_recognizer>(le);

        if (r.name == "tmpfs") {
            tmpfs = &r;
            break;
        }
    }

    if (!tmpfs) {
        print("could not find tmpfs filesystem recognizer\n");
        return false;
    }

    auto fs = tmpfs->mount_without_volume(0);
    if (!fs) {
        print("tmpfs->mount_without_volume failed\n");
        return false;
    }

    root_dentry = new dentry;
    if (!root_dentry) {
        print("new failed\n");
        fs->release();
        return false;
    }

    root_dentry->inode = fs->root_inode;
    root_dentry->name_len = 0;
    root_dentry->parent = nullptr;

    auto mp = new mount_point(*root_dentry);
    if (!mp) {
        print("new failed\n");
        delete root_dentry;
        fs->release();
        return false;
    }

    root_dentry->mount_point = true;
    mp->type = tmpfs;

    {
        lock_guard lg(mount_points_lock);
        insert_tail_list(mount_points, mp->le);
    }

    root_dentry->inode->acquire();

    process_cpio(initrd, [&](string_view path, uint64_t mode, span<const uint8_t> contents, uint64_t mtime) {
        auto status = extract_initrd_file(path, mode, contents, mtime);

        if (!success(status))
            bsod_format("extract_initrd_file returned {} for {}", status, path);
    });

    auto num_pages = bytes_to_pages(initrd.size());

    for (unsigned int i = 0; i < num_pages; i++) {
        auto va = (uintptr_t)(initrd.data() + (i << PAGE_SHIFT));
        auto pa = va_to_pa_lookup(va);

        unmap_page(va);
        free_folio(pa);
    }

    initrd = span<uint8_t>();

    return true;
}

bool mount_root() {
    string_view cmdline_root;

    if (!initrd.empty())
        return extract_initrd();

    loop_through_cmdline([&](string_view s) {
        static const char root_option[] = "root=";

        if (s.size() < sizeof(root_option) - 1)
            return true;

        if (string_view(s.data(), sizeof(root_option) - 1) != root_option)
            return true;

        cmdline_root = string_view(s.data() + sizeof(root_option) - 1,
                                   s.size() - sizeof(root_option) + 1);

        return false;
    });

    filesystem_recognizer* type = nullptr;
    volume* vol = nullptr;

    auto root_fs = do_mount_root(cmdline_root, type, vol);
    if (!root_fs)
        return false;

    root_dentry = new dentry;
    if (!root_dentry) {
        print("new failed\n");

        if (vol)
            vol->release();

        return false;
    }

    root_dentry->inode = root_fs->root_inode;
    root_dentry->name_len = 0;
    root_dentry->parent = nullptr;

    auto mp = new mount_point(*root_dentry);
    if (!mp) {
        print("new failed\n");

        if (vol)
            vol->release();

        return false;
    }

    root_dentry->mount_point = true;
    mp->type = type;
    mp->vol = vol;

    {
        lock_guard lg(mount_points_lock);
        insert_tail_list(mount_points, mp->le);
    }

    root_dentry->inode->acquire();

    return true;
}

static bool add_to_dentry_cache(dentry& dir, string_view name, inode& ino,
                                optional<reference_wrapper<dentry>>& drw) {
    auto p = malloc(sizeof(dentry));
    if (!p)
        return false;

    auto& d = *new (p) dentry;

    d.name_len = name.size();

    if (!name.empty()) {
        d.name = (char*)malloc(name.size());
        if (!d.name) {
            d.~dentry();
            free(p);
            return false;
        }

        memcpy(d.name, name.data(), name.size());
    }

    d.inode = &ino;
    ino.acquire();

    insert_tail_list(dir.children, d.le);

    d.parent = &dir;
    dir.acquire();

    drw = d;

    return true;
}

void search_dentry_cache(dentry& dir, string_view name,
                         optional<reference_wrapper<dentry>>& drw) {
    for (auto le = dir.children.next; le != &dir.children; le = le->next) {
        auto& d = containing_record<dentry>(le);
        auto sv = string_view(d.name, d.name_len);

        if (sv == name) {
            drw = d;
            d.acquire();
            return;
        }
    }
};

static nt_status open_inode2(string_view& fn, dentry& root_dir,
                             object_ref& obj_ref, optional<reference_wrapper<dentry>>& dentry,
                             uint32_t create_disposition, uint32_t create_options,
                             unsigned int& create_result, bool parent) {
    bool root = false;

    while (!fn.empty() && fn.front() == '/') {
        root = true;
        fn = string_view(fn.data() + 1, fn.size() - 1);
    }

    reference_wrapper<::dentry> dirrw{root ? *root_dentry : root_dir};
    dirrw.get().acquire();

    // FIXME - die if anything in middle not directory

    while (true) {
        auto sl = fn.find("/");
        auto& dir = dirrw.get();
        bool last_bit = sl == string_view::npos;
        optional<reference_wrapper<::dentry>> drw;
        auto name = last_bit ? fn : string_view(fn.data(), sl);
        auto dispo = last_bit ? create_disposition : FILE_OPEN;
        nt_status status;
        object_ref obj_ref2;

        if (parent && last_bit) {
            dir.inode->acquire();
            obj_ref = *dir.inode;
            dentry = dir;

            return nt_status::SUCCESS;
        }

        // FIXME - mount points
        // FIXME - negative dentries?
        // FIXME - dentry LRU

        if (name.empty() || name == ".") {
            if (dir.inode->inode_type != inode_type::directory) {
                dir.release();
                return nt_status::OBJECT_PATH_SYNTAX_BAD;
            }

            drw = dir;
            dir.acquire();
        } else if (name == "..") {
            if (dir.inode->inode_type != inode_type::directory) {
                dir.release();
                return nt_status::OBJECT_PATH_SYNTAX_BAD;
            }

            ::dentry* parent;

            {
                lock_guard lg(dir.lock);

                if (!dir.parent)
                    parent = &dir;
                else
                    parent = dir.parent;
            }

            parent->acquire();

            drw = *parent;
        } else {
            shared_lock lg(dir.child_lock);

            search_dentry_cache(dir, name, drw);
        }

        if (drw.has_value()) {
            if (dispo == FILE_CREATE) {
                dir.release();
                drw->get().release();
                return nt_status::OBJECT_NAME_COLLISION;
            } else if (last_bit)
                create_result = FILE_OPENED;
        } else {
            lock_guard lg(dir.child_lock);

            search_dentry_cache(dir, name, drw);

            if (drw.has_value()) {
                if (dispo == FILE_CREATE) {
                    dir.release();
                    drw->get().release();
                    return nt_status::OBJECT_NAME_COLLISION;
                } else if (last_bit)
                    create_result = FILE_OPENED;
            } else {
                status = dir.inode->open_child(name, obj_ref2);

                if (success(status)) {
                    if (dispo == FILE_CREATE) {
                        dir.release();
                        return nt_status::OBJECT_NAME_COLLISION;
                    } else if (last_bit)
                        create_result = FILE_OPENED;
                }

                if (status == nt_status::OBJECT_NAME_NOT_FOUND &&
                    (dispo == FILE_CREATE || dispo == FILE_OPEN_IF || dispo == FILE_OVERWRITE_IF || dispo == FILE_SUPERSEDE)) {
                    optional<reference_wrapper<inode>> inorw;

                    status = dir.inode->create_child(name, create_options,
                                                     inorw);
                    if (success(status)) {
                        create_result = FILE_CREATED;

                        obj_ref2 = inorw->get();
                    }
                }

                if (success(status)) {
                    // FIXME - should non-inodes also go in dentry cache?

                    if (obj_ref2.get().type == object_type::inode) {
                        auto& ino = static_cast<inode&>(obj_ref2.get());

                        if (!add_to_dentry_cache(dir, name, ino, drw)) {
                            dir.release();
                            return nt_status::INSUFFICIENT_RESOURCES;
                        }

                        drw->get().acquire();
                    }
                }
            }
        }

        if (!drw.has_value()) {
            dir.release();

            // non-inodes don't get put in dentry cache
            if (last_bit && success(status)) {
                swap(obj_ref, obj_ref2);
                return status;
            }

            if (!last_bit && status == nt_status::OBJECT_NAME_NOT_FOUND)
                status = nt_status::OBJECT_PATH_NOT_FOUND;

            return status;
        }

        if (last_bit && create_result == FILE_OPENED && (dispo == FILE_OVERWRITE || dispo == FILE_OVERWRITE_IF || dispo == FILE_SUPERSEDE)) {
            auto& ino = *drw->get().inode;

            // FIXME - SUPERSEDE subtly different (attributes?)

            if (create_options & FILE_DIRECTORY_FILE && ino.inode_type != inode_type::directory) {
                drw->get().release();
                dir.release();
                return nt_status::NOT_A_DIRECTORY;
            } else if (!(create_options & FILE_DIRECTORY_FILE) && ino.inode_type == inode_type::directory) {
                drw->get().release();
                dir.release();
                return nt_status::FILE_IS_A_DIRECTORY;
            }

            if (dispo == FILE_SUPERSEDE)
                create_result = FILE_SUPERSEDED;
            else
                create_result = FILE_OVERWRITTEN;
        }

        auto& d = drw->get();

        dir.release();

        if (d.inode->type == object_type::inode && (!last_bit || !(create_options & FILE_OPEN_REPARSE_POINT))) {
            auto& ino = static_cast<inode&>(*d.inode);

            if (ino.inode_type == inode_type::symlink) {
                if (last_bit)
                    fn = string_view();
                else
                    fn = string_view(fn.data() + sl + 1, fn.size() - sl - 1);

                d.inode->acquire();
                obj_ref = *d.inode;
                dentry = d;

                return nt_status::REPARSE;
            }
        }

        if (last_bit) {
            d.inode->acquire();
            obj_ref = *d.inode;
            dentry = d;

            return nt_status::SUCCESS;
        }

        fn = string_view(fn.data() + sl + 1, fn.size() - sl - 1);
        dirrw = d;
    }
}

nt_status open_inode(string_view fn, dentry& root_dir,
                     object_ref& obj_ref, optional<reference_wrapper<dentry>>& dentry,
                     uint32_t create_disposition, uint32_t create_options,
                     unsigned int& create_result, bool parent) {
    struct rdb_buf {
        REPARSE_DATA_BUFFER header;
        char16_t buffer[PATH_MAX - 1];
    };

    static const unsigned int MAX_SYMLINK_LOOPS = 40;

    nt_status status;
    unique_ptr<rdb_buf> buf;
    unique_ptr<char[]> fnbuf;
    reference_wrapper<::dentry> rwd = root_dir;
    uint32_t retlen;

    root_dir.acquire();

    for (unsigned int i = 0; i < MAX_SYMLINK_LOOPS; i++) {
        status = open_inode2(fn, rwd, obj_ref, dentry, create_disposition,
                             create_options, create_result, parent);
        if (status != nt_status::REPARSE) {
            rwd.get().release();
            return status;
        }

        if (!obj_ref || obj_ref.get().type != object_type::inode) {
            rwd.get().release();
            obj_ref.reset();

            if (dentry.has_value()) {
                dentry->get().release();
                dentry = nullopt;
            }

            return nt_status::INTERNAL_ERROR;
        }

        auto& ino = static_cast<inode&>(obj_ref.get());

        if (!buf) {
            buf.reset(new rdb_buf);
            if (!buf) {
                print("new failed\n");
                rwd.get().release();
                obj_ref.reset();

                dentry->get().release();
                dentry = nullopt;

                return nt_status::INSUFFICIENT_RESOURCES;
            }
        }

        status = ino.fs_control(FSCTL_GET_REPARSE_POINT, span<uint8_t>(),
                                span((uint8_t*)buf.get(), sizeof(*buf)), retlen,
                                nullptr);
        if (!success(status)) {
            rwd.get().release();
            obj_ref.reset();

            dentry->get().release();
            dentry = nullopt;

            return status;
        }

        if (buf->header.ReparseTag != IO_REPARSE_TAG_SYMLINK) {
            rwd.get().release();
            obj_ref.reset();

            dentry->get().release();
            dentry = nullopt;

            return nt_status::REPARSE_POINT_NOT_RESOLVED;
        }

        auto u16sv = u16string_view(buf->header.SymbolicLinkReparseBuffer.PathBuffer + (buf->header.SymbolicLinkReparseBuffer.SubstituteNameOffset / sizeof(char16_t)),
                                    buf->header.SymbolicLinkReparseBuffer.SubstituteNameLength / sizeof(char16_t));

        if (u16sv.empty()) {
            rwd.get().release();
            obj_ref.reset();

            dentry->get().release();
            dentry = nullopt;

            return nt_status::REPARSE_POINT_NOT_RESOLVED;
        }

        auto u8len = utf16_to_utf8_len(u16sv);

        if (fn.empty()) {
            fnbuf.reset(new char[u8len]);
            if (!fnbuf) {
                print("new failed\n");

                rwd.get().release();
                obj_ref.reset();

                dentry->get().release();
                dentry = nullopt;

                return nt_status::INSUFFICIENT_RESOURCES;
            }

            utf16_to_utf8(span(fnbuf.get(), u8len), u16sv);

            fn = string_view(fnbuf.get(), u8len);
        } else {
            auto fnbuf2 = new char[u8len + 1 + fn.size()];
            if (!fnbuf2) {
                print("new failed\n");

                rwd.get().release();
                obj_ref.reset();

                dentry->get().release();
                dentry = nullopt;

                return nt_status::INSUFFICIENT_RESOURCES;
            }

            utf16_to_utf8(span(fnbuf2, u8len), u16sv);
            fnbuf2[u8len] = '/';
            memcpy(fnbuf2 + u8len + 1, fn.data(), fn.size());

            fnbuf.reset(fnbuf2);

            fn = string_view(fnbuf.get(), u8len + 1 + fn.size());
        }

        rwd.get().release();

        rwd = *dentry->get().parent;
        rwd.get().acquire();

        dentry->get().release();
        dentry = nullopt;

        obj_ref.reset();
    }

    rwd.get().release();

    return nt_status::REPARSE_POINT_NOT_RESOLVED;
}

extern "C"
void trace_NtReadFile(uintptr_t* args) {
    auto file_handle = (HANDLE)args[0];
    auto vec_size = (size_t)args[1];
    auto byte_offset = (const LARGE_INTEGER*)args[2];

    start_deferred_print();

    print("NtReadFile: file_handle = ");
    print_handle(file_handle);
    print(", length = {}, byte_offset = {}\n", vec_size, byte_offset);

    end_deferred_print();
}

nt_status vec_NtReadFile(HANDLE file_handle, HANDLE, PIO_APC_ROUTINE, void*,
                         IO_STATUS_BLOCK* io_status_block,
                         span<const span<uint8_t>> v,
                         const LARGE_INTEGER* byte_offset, const uint32_t*) {
    auto vec_size = get_vec_size(v);

    TRACE3("NtReadFile", file_handle, vec_size, byte_offset);

    if (byte_offset && byte_offset->QuadPart < 0)
        return nt_status::INVALID_PARAMETER;

    auto objrw = deref_handle(file_handle);
    if (!objrw)
        return nt_status::INVALID_HANDLE;

    // FIXME - async (event)
    // FIXME - async (APC)

    auto& obj = objrw.get();

    NTSTATUS status;
    size_t read;

    if (byte_offset) {
        status = object_read_with_offset(obj, v, (uint64_t)byte_offset->QuadPart,
                                         read);
    } else
        status = object_read_without_offset(obj, v, read);

    if (!success(status) || status == nt_status::USER_APC) {
        io_status_block->Status = status;
        io_status_block->Information = 0;
        return status;
    }

    assert(read <= vec_size);

    io_status_block->Status = nt_status::SUCCESS;
    io_status_block->Information = read;

    return nt_status::SUCCESS;
}

void print_file_access_mask(ACCESS_MASK access_mask) {
    static constexpr pair<ACCESS_MASK, string_view> values[] = {
        { FILE_READ_DATA, "FILE_READ_DATA" },
        { FILE_LIST_DIRECTORY, "FILE_LIST_DIRECTORY" },
        { FILE_WRITE_DATA, "FILE_WRITE_DATA" },
        { FILE_ADD_FILE, "FILE_ADD_FILE" },
        { FILE_APPEND_DATA, "FILE_APPEND_DATA" },
        { FILE_ADD_SUBDIRECTORY, "FILE_ADD_SUBDIRECTORY" },
        { FILE_CREATE_PIPE_INSTANCE, "FILE_CREATE_PIPE_INSTANCE" },
        { FILE_READ_EA, "FILE_READ_EA" },
        { FILE_WRITE_EA, "FILE_WRITE_EA" },
        { FILE_EXECUTE, "FILE_EXECUTE" },
        { FILE_TRAVERSE, "FILE_TRAVERSE" },
        { FILE_DELETE_CHILD, "FILE_DELETE_CHILD" },
        { FILE_READ_ATTRIBUTES, "FILE_READ_ATTRIBUTES" },
        { FILE_WRITE_ATTRIBUTES, "FILE_WRITE_ATTRIBUTES" },
        { DELETE, "DELETE" },
        { READ_CONTROL, "READ_CONTROL" },
        { WRITE_DAC, "WRITE_DAC" },
        { WRITE_OWNER, "WRITE_OWNER" },
        { SYNCHRONIZE, "SYNCHRONIZE" },
        { ACCESS_SYSTEM_SECURITY, "ACCESS_SYSTEM_SECURITY" },
        { MAXIMUM_ALLOWED, "MAXIMUM_ALLOWED" },
        { GENERIC_ALL, "GENERIC_ALL" },
        { GENERIC_EXECUTE, "GENERIC_EXECUTE" },
        { GENERIC_WRITE, "GENERIC_WRITE" },
        { GENERIC_READ, "GENERIC_READ" },
    };

    bool first = true;

    for (auto& v : values) {
        if (access_mask & v.first) {
            access_mask &= ~v.first;

            if (!first)
                print(" | ");

            print("{}", v.second);
            first = false;
        }
    }

    if (access_mask != 0) {
        if (!first)
            print(" | ");

        print("{:x}", access_mask);
    }

    if (first && access_mask == 0)
        print("0");
}

static void print_create_options(uint32_t create_options) {
    static constexpr pair<uint32_t, string_view> values[] = {
        { FILE_DIRECTORY_FILE, "FILE_DIRECTORY_FILE" },
        { FILE_WRITE_THROUGH, "FILE_WRITE_THROUGH" },
        { FILE_SEQUENTIAL_ONLY, "FILE_SEQUENTIAL_ONLY" },
        { FILE_NO_INTERMEDIATE_BUFFERING, "FILE_NO_INTERMEDIATE_BUFFERING" },
        { FILE_SYNCHRONOUS_IO_ALERT, "FILE_SYNCHRONOUS_IO_ALERT" },
        { FILE_SYNCHRONOUS_IO_NONALERT, "FILE_SYNCHRONOUS_IO_NONALERT" },
        { FILE_NON_DIRECTORY_FILE, "FILE_NON_DIRECTORY_FILE" },
        { FILE_CREATE_TREE_CONNECTION, "FILE_CREATE_TREE_CONNECTION" },
        { FILE_COMPLETE_IF_OPLOCKED, "FILE_COMPLETE_IF_OPLOCKED" },
        { FILE_NO_EA_KNOWLEDGE, "FILE_NO_EA_KNOWLEDGE" },
        { FILE_OPEN_REMOTE_INSTANCE, "FILE_OPEN_REMOTE_INSTANCE" },
        { FILE_RANDOM_ACCESS, "FILE_RANDOM_ACCESS" },
        { FILE_DELETE_ON_CLOSE, "FILE_DELETE_ON_CLOSE" },
        { FILE_OPEN_BY_FILE_ID, "FILE_OPEN_BY_FILE_ID" },
        { FILE_OPEN_FOR_BACKUP_INTENT, "FILE_OPEN_FOR_BACKUP_INTENT" },
        { FILE_NO_COMPRESSION, "FILE_NO_COMPRESSION" },
        { FILE_OPEN_REQUIRING_OPLOCK, "FILE_OPEN_REQUIRING_OPLOCK" },
        { FILE_DISALLOW_EXCLUSIVE, "FILE_DISALLOW_EXCLUSIVE" },
        { FILE_RESERVE_OPFILTER, "FILE_RESERVE_OPFILTER" },
        { FILE_OPEN_REPARSE_POINT, "FILE_OPEN_REPARSE_POINT" },
        { FILE_OPEN_NO_RECALL, "FILE_OPEN_NO_RECALL" },
        { FILE_OPEN_FOR_FREE_SPACE_QUERY, "FILE_OPEN_FOR_FREE_SPACE_QUERY" },
        { MOXNOX_FILE_OPEN_NONBLOCK, "MOXNOX_FILE_OPEN_NONBLOCK" },
    };

    bool first = true;

    for (auto& v : values) {
        if (create_options & v.first) {
            create_options &= ~v.first;

            if (!first)
                print(" | ");

            print("{}", v.second);
            first = false;
        }
    }

    if (create_options != 0) {
        if (!first)
            print(" | ");

        print("{:x}", create_options);
    }

    if (first && create_options == 0)
        print("0");
}

void print_create_disposition(uint32_t create_disposition) {
    switch (create_disposition) {
        case FILE_SUPERSEDE:
            print("FILE_SUPERSEDE");
            break;

        case FILE_CREATE:
            print("FILE_CREATE");
            break;

        case FILE_OPEN:
            print("FILE_OPEN");
            break;

        case FILE_OPEN_IF:
            print("FILE_OPEN_IF");
            break;

        case FILE_OVERWRITE:
            print("FILE_OVERWRITE");
            break;

        case FILE_OVERWRITE_IF:
            print("FILE_OVERWRITE_IF");
            break;

        default:
            print("{:x}", create_disposition);
            break;
    }
}

extern "C"
void trace_NtCreateFile(uintptr_t* args) {
    auto access_mask = (ACCESS_MASK)args[0];
    auto& object_attributes = *(const OBJECT_ATTRIBUTES*)args[1];
    auto create_disposition = (uint32_t)args[2];
    auto create_options = (uint32_t)args[3];

    start_deferred_print();

    print("NtCreateFile: access_mask = ");
    print_file_access_mask(access_mask);
    print(", object_attributes = {}, create_disposition = ", object_attributes);
    print_create_disposition(create_disposition);
    print(", create_options = ");
    print_create_options(create_options);
    print("\n");

    end_deferred_print();
}

extern "C"
NTSTATUS NtCreateFile(HANDLE* file_handle, ACCESS_MASK access_mask,
                      const OBJECT_ATTRIBUTES* object_attributes, IO_STATUS_BLOCK* io_status_block,
                      const LARGE_INTEGER*, uint32_t, uint32_t, uint32_t create_disposition,
                      uint32_t create_options, void*, uint32_t) {
    NTSTATUS status;
    auto t = get_current_thread();
    auto p = t->proc;
    optional<reference_wrapper<dentry>> root_dir;
    bool shared_okay = true;

    TRACE4("NtCreateFile", access_mask, object_attributes,
           create_disposition, create_options);

    if (!p)
        return nt_status::INVALID_PARAMETER;

    if (create_options & FILE_NON_DIRECTORY_FILE && create_options & FILE_DIRECTORY_FILE)
        return nt_status::INVALID_PARAMETER;

    switch (create_disposition) {
        case FILE_SUPERSEDE:
        case FILE_OPEN:
        case FILE_CREATE:
        case FILE_OPEN_IF:
        case FILE_OVERWRITE:
        case FILE_OVERWRITE_IF:
            break;

        default:
            return nt_status::INVALID_PARAMETER;
    }

    if (!object_attributes->ObjectName) // FIXME - is this allowed if RootDirectory is set?
        return nt_status::INVALID_PARAMETER;

    if (object_attributes->RootDirectory) {
        auto root_dir_file = deref_handle(object_attributes->RootDirectory);

        if (!root_dir_file)
            return nt_status::INVALID_HANDLE;

        if (root_dir_file.get().type != object_type::file)
            return nt_status::INVALID_HANDLE;

        auto& f = static_cast<file&>(root_dir_file.get());

        if (f.ino.type != object_type::inode)
            return nt_status::INVALID_HANDLE;

        if (!f.dentry) // shouldn't happen
            return nt_status::INTERNAL_ERROR;

        root_dir = *f.dentry;
    } else
        root_dir = *root_dentry;

    auto& rd = root_dir->get();

    rd.acquire();

    // FIXME - check permissions (whether valid, and against SD)
    // FIXME - allocation_size
    // FIXME - file_attributes
    // FIXME - share_access
    // FIXME - EAs

    auto u16name = u16string_view(object_attributes->ObjectName->Buffer, object_attributes->ObjectName->Length / sizeof(char16_t));

    if (u16name.empty()) {
        rd.release();
        return nt_status::INVALID_PARAMETER;
    }

    auto namelen = utf16_to_utf8_len(u16name);

    if (namelen == 0) {
        rd.release();
        return nt_status::INVALID_PARAMETER;
    }

    auto name = (char*)malloc(namelen);
    if (!name) {
        rd.release();
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    utf16_to_utf8(span(name, namelen), u16name);

    for (unsigned int i = 0; i < namelen; i++) {
        if (name[i] == '\\')
            name[i] = '/';
    }

    object_ref ino_ref;
    optional<reference_wrapper<dentry>> drw;
    unsigned int create_result;

    status = open_inode(string_view(name, namelen), rd,
                        ino_ref, drw, create_disposition, create_options,
                        create_result, false);
    if (!success(status)) {
        free(name);
        rd.release();
        return status;
    }

    free(name);
    rd.release();

    shared_okay = !object_is_exclusive(ino_ref.get());

    handle_t h;

    if (ino_ref.get().type == object_type::inode) {
        auto& ino = static_cast<inode&>(ino_ref.get());

        if (create_options & FILE_NON_DIRECTORY_FILE && ino.inode_type == inode_type::directory) {
            if (drw)
                drw->get().release();

            return nt_status::FILE_IS_A_DIRECTORY;
        } else if (create_options & FILE_DIRECTORY_FILE && ino.inode_type != inode_type::directory) {
            if (drw)
                drw->get().release();

            return nt_status::NOT_A_DIRECTORY;
        }

        if (access_mask & GENERIC_READ) {
            access_mask &= ~GENERIC_READ;
            access_mask |= FILE_GENERIC_READ;
        }

        if (access_mask & GENERIC_WRITE) {
            access_mask &= ~GENERIC_WRITE;
            access_mask |= FILE_GENERIC_WRITE;
        }

        if (access_mask & GENERIC_EXECUTE) {
            access_mask &= ~GENERIC_EXECUTE;
            access_mask |= FILE_GENERIC_EXECUTE;
        }

        auto f = new file(ino, create_options, access_mask);

        if (!f) {
            if (drw)
                drw->get().release();

            return nt_status::INSUFFICIENT_RESOURCES;
        }

        if (drw)
            f->dentry = &drw->get();

        if (ino.fs.readonly) {
            if (access_mask & (FILE_WRITE_DATA | FILE_WRITE_ATTRIBUTES | FILE_WRITE_EA | FILE_APPEND_DATA | DELETE)) {
                delete f;
                return nt_status::MEDIA_WRITE_PROTECTED;
            }
        }

        auto ho = (handle_obj*)malloc(sizeof(handle_obj));
        if (!ho) {
            print("malloc failed\n");
            delete f;
            return nt_status::INSUFFICIENT_RESOURCES;
        }

        if (ino.inode_type == inode_type::fifo && access_mask & (FILE_READ_DATA | FILE_WRITE_DATA | FILE_APPEND_DATA)) {
            fifo_object* fifo = nullptr;

            {
                sync_lock sl;
                lock_guard lg(sl);

                if (ino.fifo) {
                    ino.fifo->acquire();
                    fifo = ino.fifo;
                }
            }

            if (!fifo) {
                fifo_object* old_fifo = nullptr;

                fifo = new fifo_object;
                if (!fifo) {
                    print("new failed\n");
                    delete f;
                    free(ho);
                    return nt_status::INSUFFICIENT_RESOURCES;
                }

                if (!fifo->init) {
                    delete f;
                    delete fifo;
                    free(ho);
                    return nt_status::INSUFFICIENT_RESOURCES;
                }

                {
                    sync_lock sl;
                    lock_guard lg(sl);

                    if (ino.fifo) {
                        ino.fifo->acquire();
                        old_fifo = fifo;
                        fifo = ino.fifo;
                    } else {
                        ino.fifo = fifo;
                        fifo->acquire();
                    }
                }

                if (old_fifo)
                    old_fifo->release();
            }

            {
                sync_lock sl;
                lock_guard lock1(sl);
                lock_guard lock2(fifo->lock);

                if (access_mask & FILE_READ_DATA) {
                    fifo->num_readers++;
                    fifo->have_readers.set_holding_sync_lock();

                    if (fifo->buf_unread == fifo_object::BUF_PAGES << PAGE_SHIFT) {
                        fifo->not_full_event.signalled = false;
                        fifo->write_signalled = false;
                    }
                }

                if (access_mask & (FILE_WRITE_DATA | FILE_APPEND_DATA)) {
                    fifo->num_writers++;
                    fifo->have_writers.set_holding_sync_lock();

                    if (fifo->signalled && fifo->buf_unread == 0)
                        fifo->signalled = false;
                }
            }

            if (access_mask & FILE_READ_DATA && !(access_mask & (FILE_WRITE_DATA | FILE_APPEND_DATA)) && !(create_options & MOXNOX_FILE_OPEN_NONBLOCK))
                status = wait_for_object(fifo->have_writers, true);
            else if (access_mask & (FILE_WRITE_DATA | FILE_APPEND_DATA) && !(access_mask & FILE_READ_DATA)) {
                if (create_options & MOXNOX_FILE_OPEN_NONBLOCK) {
                    status = wait_for_object(fifo->have_readers, true, 0);

                    if (status == nt_status::TIMEOUT)
                        status = nt_status::PIPE_DISCONNECTED;
                } else
                    status = wait_for_object(fifo->have_readers, true);
            } else
                status = nt_status::SUCCESS;

            if (!success(status) || status == nt_status::USER_APC) {
                {
                    sync_lock sl;
                    lock_guard lock1(sl);
                    lock_guard lock2(fifo->lock);

                    if (access_mask & FILE_READ_DATA) {
                        fifo->num_readers--;

                        if (fifo->num_readers == 0) {
                            fifo->have_readers.signalled = false;
                            fifo->not_full_event.set_holding_sync_lock();

                            fifo->write_signalled = true;
                            wake_up_waiters(fifo->waiters, WAIT_TYPE_WRITING);
                        }
                    }

                    if (access_mask & (FILE_WRITE_DATA | FILE_APPEND_DATA)) {
                        fifo->num_writers--;

                        if (fifo->num_writers == 0) {
                            fifo->have_writers.signalled = false;

                            if (!fifo->signalled) {
                                fifo->signalled = true;
                                wake_up_waiters(fifo->waiters, WAIT_TYPE_READING);
                            }
                        }
                    }
                }

                delete f;
                fifo->release();
                free(ho);

                return status;
            }

            fifo->release();
        }

        f->handle_count++;
        p->add_handle_no_alloc(*f, access_mask, h, ho);

        if (f->dentry)
            f->dentry->open_file_count++;

        f->release();

        if ((create_result == FILE_OVERWRITTEN || create_result == FILE_SUPERSEDED) && ino.inode_type == inode_type::file) {
            auto now = (uint64_t)kernel_time_page.system_time + kernel_time_page.system_time_offset;

            {
                lock_guard lock(ino.inode_lock);

                ino.file_size = 0;
                ino.mtime = now;
                ino.ctime = now;
            }

            ino.cache.mark_dirty();
        }
    } else {
        if (drw) // FIXME - shouldn't happen, as only inodes get dentries at the moment
            drw->get().release();

        auto f = new file(ino_ref.get(), 0, 0);

        if (!f)
            return nt_status::INSUFFICIENT_RESOURCES;

        object_ref ref{*f};

        // FIXME - GENERIC_READ, GENERIC_WRITE, GENERIC_EXECUTE (different for each object type)

        status = p->add_handle(*f, access_mask, shared_okay, h);
        if (!success(status))
            return status;
    }

    *file_handle = h;

    io_status_block->Information = create_result;

    return nt_status::SUCCESS;
}

extern "C"
NTSTATUS NtOpenFile(HANDLE* file_handle, ACCESS_MASK desired_access,
                    const OBJECT_ATTRIBUTES* object_attributes,
                    IO_STATUS_BLOCK* iosb, uint32_t share_access,
                    uint32_t open_options) {
    return NtCreateFile(file_handle, desired_access, object_attributes, iosb,
                        nullptr, 0, share_access, FILE_OPEN, open_options,
                        nullptr, 0);
}

extern "C"
void trace_NtWriteFile(uintptr_t* args) {
    auto file_handle = (HANDLE)args[0];
    auto vec_size = (size_t)args[1];
    auto byte_offset = (const LARGE_INTEGER*)args[2];

    start_deferred_print();

    print("NtWriteFile: file_handle = ");
    print_handle(file_handle);
    print(", length = {}, byte_offset = {}\n", vec_size, byte_offset);

    end_deferred_print();
}

nt_status vec_NtWriteFile(HANDLE file_handle, HANDLE, PIO_APC_ROUTINE, void*,
                          IO_STATUS_BLOCK* io_status_block,
                          span<const span<const uint8_t>> v,
                          const LARGE_INTEGER* byte_offset,
                          const uint32_t*) {
    ACCESS_MASK access_mask;
    auto vec_size = get_vec_size(v);

    TRACE3("NtWriteFile", file_handle, vec_size, byte_offset);

    static const uint64_t WRITE_TO_END_OF_FILE = 0xffffffffffffffff;

    auto objrw = deref_handle(file_handle, access_mask);
    if (!objrw)
        return nt_status::INVALID_HANDLE;

    if (!(access_mask & (FILE_WRITE_DATA | FILE_APPEND_DATA)))
        return nt_status::ACCESS_DENIED;

    // FIXME - async (event)
    // FIXME - async (APC)

    auto& obj = objrw.get();
    optional<uint64_t> offset;

    if (byte_offset)
        offset = (uint64_t)byte_offset->QuadPart;

    if ((access_mask & FILE_APPEND_DATA) && !(access_mask & FILE_WRITE_DATA))
        offset = WRITE_TO_END_OF_FILE;

    size_t written;
    nt_status status;

    if (offset.has_value() && *offset == WRITE_TO_END_OF_FILE) {
        if (obj.type != object_type::file)
            status = nt_status::INVALID_PARAMETER;
        else {
            auto& f = static_cast<file&>(obj);

            if (f.ino.type != object_type::inode)
                status = nt_status::INVALID_PARAMETER;
            else {
                auto& ino = static_cast<inode&>(f.ino);

                {
                    lock_guard lock(ino.inode_lock);
                    offset = ino.file_size;
                }

                status = f.write(v, *offset, written);
            }
        }
    } else if (offset.has_value())
        status = object_write_with_offset(obj, v, *offset, written);
    else
        status = object_write_without_offset(obj, v, written);

    if (status == nt_status::PIPE_BROKEN) {
        auto mask = ((sigset_t)1 << (SIGPIPE - 1));
        auto& t = *get_current_thread();

        {
            sync_lock sl;
            lock_guard lock(sl);

            if (t.blocked_signals & mask)
                t.pending_signals |= mask;
            else {
                auto p = new pending_apc;

                if (p) { // if we OOM, just drop the signal
                    p->routine = (PS_APC_ROUTINE)(t.proc->ntdll_addr + mn_posix_signal);
                    p->arg1 = (uintptr_t)SIGPIPE;
                    p->arg2 = 0;
                    p->arg3 = 0;

                    {
                        lock_guard lock(t.apc_lock);

                        insert_tail_list(t.pending_apcs, p->le);
                    }
                }
            }
        }
    }

    if (!success(status) || status == nt_status::USER_APC) {
        io_status_block->Status = status;
        io_status_block->Information = 0;
        return status;
    }

    assert(written <= vec_size);

    io_status_block->Status = nt_status::SUCCESS;
    io_status_block->Information = written;

    return nt_status::SUCCESS;
}

nt_status flush_filesystems(bool make_readonly) {
    lock_guard lock(mount_points_lock);

    // FIXME - what should we do if there's a failure - carry on?

    for (auto le = mount_points.next; le != &mount_points; le = le->next) {
        auto& mp = containing_record<::mount_point>(le);

        if (!mp.vol || !mp.vol->filesystem.has_value())
            continue;

        auto status = mp.vol->filesystem->get().flush(make_readonly);
        if (!success(status))
            return status;
    }

    return nt_status::SUCCESS;
}

inode::~inode() {
    fs.release();

    while (!is_list_empty(posix_lock_procs)) {
        auto& plp = containing_record<posix_lock_proc>(posix_lock_procs.next);

        remove_entry_list(plp.le);
        delete &plp;
    }
}

nt_status inode::read(span<const span<uint8_t>> v, uint64_t offset,
                      uint64_t& read_bytes) {
    uint64_t size;
    size_t vec_size = get_vec_size(v);
    fifo_object* f = nullptr;

    {
        shared_lock lock1(inode_lock);

        switch (inode_type) {
            case inode_type::file:
                size = file_size;
                break;

            case inode_type::fifo: {
                sync_lock sl;
                lock_guard lock2(sl);

                if (!fifo)
                    return nt_status::INTERNAL_ERROR;

                f = fifo;
                f->acquire();

                break;
            }

            default:
                return nt_status::INVALID_PARAMETER;
        }
    }

    if (f) {
        auto ret = f->read(v, read_bytes);

        f->release();

        return ret;
    }

    if (offset >= size)
        return nt_status::END_OF_FILE;

    if (vec_size == 0) {
        read_bytes = 0;
        return nt_status::SUCCESS;
    }

    auto status = cache.read(v, offset, size - offset);
    if (!success(status))
        return status;

    read_bytes = min((uint64_t)vec_size, size - offset);
    return nt_status::SUCCESS;
}

nt_status inode::read_nonblock(span<const span<uint8_t>> v, uint64_t& read_bytes) {
    fifo_object* f = nullptr;

    {
        shared_lock lock(inode_lock);

        if (inode_type != inode_type::fifo)
            return nt_status::NOT_SUPPORTED;

        {
            sync_lock sl;
            lock_guard lock2(sl);

            if (!fifo)
                return nt_status::INTERNAL_ERROR;

            f = fifo;
            f->acquire();
        }
    }

    auto ret = f->read_nonblock(v, read_bytes);

    f->release();

    return ret;
}

nt_status inode::write(span<const span<const uint8_t>> v, uint64_t offset,
                       uint64_t& written_bytes) {
    // FIXME - don't allow if FS readonly

    size_t vec_size = get_vec_size(v);

    if (vec_size == 0) {
        written_bytes = 0;
        return nt_status::SUCCESS;
    }

    fifo_object* f = nullptr;

    {
        lock_guard lock1(inode_lock);

        switch (inode_type) {
            case inode_type::file: {
                auto now = (uint64_t)kernel_time_page.system_time + kernel_time_page.system_time_offset;

                if (file_size < offset + vec_size)
                    file_size = offset + vec_size;

                mtime = now;
                ctime = now;

                break;
            }

            case inode_type::fifo: {
                sync_lock sl;
                lock_guard lock2(sl);

                if (!fifo)
                    return nt_status::INTERNAL_ERROR;

                f = fifo;
                f->acquire();

                break;
            }

            default:
                return nt_status::INVALID_PARAMETER;
        }
    }

    if (f) {
        auto ret = f->write(v, written_bytes);

        f->release();

        return ret;
    }

    written_bytes = cache.write_vec(v, offset);

    return nt_status::SUCCESS;
}

nt_status inode::write_nonblock(span<const span<const uint8_t>> v,
                                uint64_t& written_bytes) {
    fifo_object* f = nullptr;

    {
        shared_lock lock(inode_lock);

        if (inode_type != inode_type::fifo)
            return nt_status::NOT_SUPPORTED;

        {
            sync_lock sl;
            lock_guard lock2(sl);

            if (!fifo)
                return nt_status::INTERNAL_ERROR;

            f = fifo;
            f->acquire();
        }
    }

    auto ret = f->write_nonblock(v, written_bytes);

    f->release();

    return ret;
}

file::file(object& ino, uint32_t create_options, uint32_t access_mask) : object(object_type::file), ino(ino), create_options(create_options), access_mask(access_mask) {
    ino.acquire();
    ino.handle_count++;
}

file::~file() {
    handle_closed(ino);

    object_release(ino);

    if (dentry)
        dentry->release();
}

NTSTATUS file::read(span<const span<uint8_t>> v, uint64_t& read_bytes) {
    NTSTATUS status;

    if (nonblock) {
        status = object_read_nonblock(ino, v, read_bytes);
        if (status != nt_status::NOT_SUPPORTED)
            return status;
    }

    status = object_read_without_offset(ino, v, read_bytes);
    if (status != nt_status::NOT_SUPPORTED)
        return status;

    status = object_read_with_offset(ino, v, file_offset, read_bytes);

    if (success(status)) {
        lock_guard l(lock);
        file_offset += read_bytes;
    }

    return status;
}

NTSTATUS file::read(span<const span<uint8_t>> v, uint64_t offset,
                    uint64_t& read_bytes) {
    auto status = object_read_with_offset(ino, v, offset, read_bytes);

    if (success(status)) {
        lock_guard l(lock);
        file_offset = offset + read_bytes;
    }

    return status;
}

NTSTATUS file::write(span<const span<const uint8_t>> v, uint64_t& written_bytes) {
    NTSTATUS status;

    // FIXME - update file position if FILE_SYNCHRONOUS_IO_ALERT or FILE_SYNCHRONOUS_IO_NONALERT set

    if (nonblock) {
        status = object_write_nonblock(ino, v, written_bytes);
        if (status != nt_status::NOT_SUPPORTED)
            return status;
    }

    status = object_write_without_offset(ino, v, written_bytes);
    if (status != nt_status::NOT_SUPPORTED)
        return status;

    status = object_write_with_offset(ino, v, file_offset, written_bytes);

    if (success(status) && written_bytes > 0) {
        lock_guard l(lock);
        file_offset += written_bytes;
    }

    return status;
}

NTSTATUS file::write(span<const span<const uint8_t>> v, uint64_t offset,
                     uint64_t& written_bytes) {
    // FIXME - if FILE_SYNCHRONOUS_IO_ALERT or FILE_SYNCHRONOUS_IO_NONALERT set, set file_offset to offset first

    auto status = object_write_with_offset(ino, v, offset, written_bytes);

    if (success(status) && written_bytes > 0) {
        lock_guard l(lock);
        file_offset = offset + written_bytes;
    }

    return status;
}

static nt_status file_readv(file& f, span<const uint8_t> inbuf,
                            span<uint8_t> outbuf, uintptr_t& written) {
    nt_status status;
    auto& t = *get_current_thread();

    if (inbuf.size() < sizeof(iovec))
        return nt_status::INVALID_PARAMETER;

    if (outbuf.size() < sizeof(uint64_t))
        return nt_status::INVALID_PARAMETER;

    auto vec = span((iovec*)inbuf.data(), inbuf.size() / sizeof(iovec));
    auto& read_bytes = *(uint64_t*)outbuf.data();

    optional<rangelock_req> rlr;
    span<uint8_t>* spans = nullptr;
    size_t num_spans = 0;

    for (auto& v : vec) {
        if (v.iov_len > 0 && (!is_userspace_address((uintptr_t)v.iov_base) || !is_userspace_address((uintptr_t)v.iov_base + v.iov_len - 1)))
            return nt_status::ACCESS_VIOLATION;
    }

    status = pin_iovecs_for_writing(*t.proc, vec, rlr, spans, num_spans);
    if (!success(status))
        return status;

    status = f.read(span(spans, num_spans), read_bytes);

    rlr.reset();

    auto spans_size = sizeof(span<uint8_t>) * num_spans;
    if (spans_size > PAGE_SIZE)
        vfree(spans, spans_size);
    else if (spans_size > 0)
        free(spans);

    if (!success(status) || status == nt_status::USER_APC)
        return status;

    written = sizeof(uint64_t);

    return nt_status::SUCCESS;
}

static nt_status file_writev(file& f, span<const uint8_t> inbuf,
                             span<uint8_t> outbuf, uintptr_t& written) {
    nt_status status;
    auto& t = *get_current_thread();

    if (inbuf.size() < sizeof(iovec))
        return nt_status::INVALID_PARAMETER;

    if (outbuf.size() < sizeof(uint64_t))
        return nt_status::INVALID_PARAMETER;

    auto vec = span((iovec*)inbuf.data(), inbuf.size() / sizeof(iovec));
    auto& written_bytes = *(uint64_t*)outbuf.data();

    optional<rangelock_req> rlr;
    span<const uint8_t>* spans = nullptr;
    size_t num_spans = 0;

    for (auto& v : vec) {
        if (v.iov_len > 0 && (!is_userspace_address((uintptr_t)v.iov_base) || !is_userspace_address((uintptr_t)v.iov_base + v.iov_len - 1)))
            return nt_status::ACCESS_VIOLATION;
    }

    status = pin_iovecs_for_reading(*t.proc, vec, rlr, spans, num_spans);
    if (!success(status))
        return status;

    status = f.write(span(spans, num_spans), written_bytes);

    rlr.reset();

    auto spans_size = sizeof(span<const uint8_t>) * num_spans;
    if (spans_size > PAGE_SIZE)
        vfree(spans, spans_size);
    else if (spans_size > 0)
        free(spans);

    if (status == nt_status::PIPE_BROKEN) {
        auto mask = ((sigset_t)1 << (SIGPIPE - 1));

        {
            sync_lock sl;
            lock_guard lock(sl);

            if (t.blocked_signals & mask)
                t.pending_signals |= mask;
            else {
                auto p = new pending_apc;

                if (p) { // if we OOM, just drop the signal
                    p->routine = (PS_APC_ROUTINE)(t.proc->ntdll_addr + mn_posix_signal);
                    p->arg1 = (uintptr_t)SIGPIPE;
                    p->arg2 = 0;
                    p->arg3 = 0;

                    {
                        lock_guard lock(t.apc_lock);

                        insert_tail_list(t.pending_apcs, p->le);
                    }
                }
            }
        }
    }

    if (!success(status) || status == nt_status::USER_APC)
        return status;

    written = sizeof(uint64_t);

    return nt_status::SUCCESS;
}

nt_status file::ioctl(uint32_t code, span<const uint8_t> inbuf,
                      span<uint8_t> outbuf, uintptr_t& written) {
    nt_status ret;

    switch (code) {
        case IOCTL_MOXNOX_FILE_READV:
            return file_readv(*this, inbuf, outbuf, written);

        case IOCTL_MOXNOX_FILE_WRITEV:
            return file_writev(*this, inbuf, outbuf, written);

        default:
            break;
    }

    if (nonblock && ino.type == object_type::socket) {
        auto& s = static_cast<socket&>(ino);

        ret = s.ioctl(code, inbuf, outbuf, written, true);
    } else
        ret = object_ioctl(ino, code, inbuf, outbuf, written);

    return ret;
}

extern "C"
void trace_NtQueryDirectoryFile(uintptr_t* args) {
    auto file_handle = (HANDLE)args[0];
    auto file_information = (void*)args[1];
    auto length = (uint32_t)args[2];
    auto file_information_class = (FILE_INFORMATION_CLASS)args[3];
    auto restart_scan = (bool)args[4];

    start_deferred_print();

    print("NtQueryDirectoryFile: file_handle = ");
    print_handle(file_handle);
    print(", file_information = {:x}, length = {}, file_information_class = {}, restart_scan = {}\n",
          (uintptr_t)file_information, length, file_information_class,
          restart_scan ? "true" : "false");

    end_deferred_print();
}

extern "C"
NTSTATUS NtQueryDirectoryFile(HANDLE file_handle, HANDLE event, PIO_APC_ROUTINE apc_routine,
                              void* apc_context, IO_STATUS_BLOCK* io_status_block,
                              void* file_information, uint32_t length,
                              FILE_INFORMATION_CLASS file_information_class, bool return_single_entry,
                              const UNICODE_STRING* file_name, bool restart_scan) {
    TRACE5("NtQueryDirectoryFile", file_handle, file_information, length,
           file_information_class, restart_scan);

    auto objrw = deref_handle(file_handle);
    if (!objrw)
        return nt_status::INVALID_HANDLE;

    auto& obj = objrw.get();

    if (obj.type != object_type::file)
        return nt_status::INVALID_HANDLE;

    auto& f = static_cast<file&>(obj);

    auto ret = f.query_directory(event, apc_routine, apc_context, *io_status_block, file_information,
                                 length, file_information_class, return_single_entry,
                                 file_name, restart_scan);

    return ret;
}

NTSTATUS file::query_directory(HANDLE, PIO_APC_ROUTINE, void*, IO_STATUS_BLOCK& io_status_block,
                               void* file_information, uint32_t length,
                               FILE_INFORMATION_CLASS file_information_class, bool,
                               const UNICODE_STRING*, bool restart_scan) {
    nt_status status;

    // FIXME - event
    // FIXME - APC
    // FIXME - filename

    if (restart_scan)
        query_dir_index = 0;

    // FIXME - returning multiple entries

    auto index = query_dir_index;
    auto sp = span((uint8_t*)file_information, length);

    if (ino.type != object_type::inode)
        return nt_status::INVALID_PARAMETER;

    if (index == 0) {
        status = static_cast<inode&>(ino).query_directory_inode(".", sp,
                                                                file_information_class);
        index = 1;
    } else if (index == 1) {
        inode* parent;

        {
            lock_guard lg(dentry->lock);

            if (!dentry->parent)
                parent = &static_cast<inode&>(ino);
            else
                parent = dentry->parent->inode;

            parent->acquire();
        }

        status = parent->query_directory_inode("..", sp, file_information_class);

        parent->release();

        if (!success(status))
            return status;

        index = 2;
    } else {
        status = static_cast<inode&>(ino).query_directory(sp, file_information_class, index);
        if (!success(status))
            return status;
    }

    query_dir_index = index;
    io_status_block.Information = sp.size();

    return status;
}

extern "C"
void trace_NtSetInformationFile(uintptr_t* args) {
    auto file_handle = (HANDLE)args[0];
    auto buffer = (void*)args[1];
    auto buffer_size = (uint32_t)args[2];
    auto file_information_class = (FILE_INFORMATION_CLASS)args[3];

    start_deferred_print();

    print("NtSetInformationFile: file_handle = ");
    print_handle(file_handle);
    print(", buffer = {:x}, buffer_size = {}, file_information_class = {}\n",
          (uintptr_t)buffer, buffer_size, file_information_class);

    end_deferred_print();
}

extern "C"
NTSTATUS NtSetInformationFile(HANDLE file_handle, IO_STATUS_BLOCK* iosb, void* buffer,
                              uint32_t buffer_size, FILE_INFORMATION_CLASS file_information_class) {
    TRACE4("NtSetInformationFile", file_handle, buffer, buffer_size,
           file_information_class);

    auto objrw = deref_handle(file_handle);
    if (!objrw)
        return nt_status::INVALID_HANDLE;

    auto& obj = objrw.get();

    if (obj.type != object_type::file)
        return nt_status::INVALID_HANDLE;

    auto& f = static_cast<file&>(obj);

    iosb->Information = 0;

    return f.set_information(buffer, buffer_size, file_information_class);
}

NTSTATUS file::rename(span<const uint8_t> buf) {
    if (ino.type != object_type::inode)
        return nt_status::INVALID_PARAMETER;

    if (buf.size() < offsetof(FILE_RENAME_INFORMATION, FileName))
        return nt_status::BUFFER_TOO_SMALL;

    auto& fri = *(FILE_RENAME_INFORMATION*)buf.data();

    if (buf.size() < offsetof(FILE_RENAME_INFORMATION, FileName) + fri.FileNameLength)
        return nt_status::BUFFER_TOO_SMALL;

    object_ref rootdir, destdir;
    unsigned int create_result;
    optional<reference_wrapper<::dentry>> drw;

    if (fri.RootDirectory != 0) {
        rootdir = deref_handle(fri.RootDirectory);
        if (!rootdir)
            return nt_status::INVALID_HANDLE;

        if (rootdir.get().type != object_type::file)
            return nt_status::INVALID_HANDLE;

        auto& f = static_cast<file&>(rootdir.get());

        if (f.ino.type != object_type::inode)
            return nt_status::INVALID_HANDLE;

        if (static_cast<inode&>(f.ino).inode_type != inode_type::directory)
            return nt_status::INVALID_HANDLE;
    }

    // don't allow if source is root of a filesystem
    auto& inode = static_cast<::inode&>(ino);
    if (inode.fs.root_inode == &inode)
        return nt_status::INVALID_PARAMETER;

    // file name to utf-8

    auto u16name = u16string_view(fri.FileName, fri.FileNameLength / sizeof(char16_t));

    // remove trailing slashes
    while (!u16name.empty() && u16name.back() == '/') {
        u16name = u16string_view(u16name.data(), u16name.size() - 1);
    }

    auto namelen = utf16_to_utf8_len(u16name);
    if (namelen == 0)
        return nt_status::INVALID_PARAMETER;

    auto name = (char*)malloc(namelen);
    if (!name)
        return nt_status::INSUFFICIENT_RESOURCES;

    utf16_to_utf8(span(name, namelen), u16name);

    auto fn = string_view(name, namelen);

    auto& root_dir_dentry = rootdir ? *static_cast<file&>(rootdir.get()).dentry : *root_dentry;

    // open parent of destination

    auto status = open_inode(fn, root_dir_dentry, destdir, drw, FILE_OPEN,
                             FILE_DIRECTORY_FILE, create_result, true);

    if (status == nt_status::OBJECT_NAME_NOT_FOUND) {
        free(name);
        return nt_status::OBJECT_PATH_NOT_FOUND;
    } else if (!success(status)) {
        free(name);
        return status;
    }

    assert(destdir);
    assert(destdir.get().type == object_type::inode);
    assert(drw);

    auto& dest_dir_inode = static_cast<::inode&>(destdir.get());

    if (&dest_dir_inode.fs != &inode.fs) {
        drw->get().release();
        free(name);
        return nt_status::NOT_SAME_DEVICE;
    }

    string_view namepart;
    object_ref victim;

    if (auto sl = fn.find_last_of('/'); sl != string_view::npos)
        namepart = string_view(fn.data() + sl + 1, fn.size() - sl - 1);
    else
        namepart = fn;

    status = inode.rename(*dentry, drw->get(), namepart, fri.ReplaceIfExists,
                          victim);

    if (victim) {
        assert(victim.get().type == object_type::inode);

        if (success(status)) {
            optional<reference_wrapper<::dentry>> vdrw;

            {
                lock_guard lg(drw->get().child_lock);

                search_dentry_cache(drw->get(), namepart, vdrw);
            }

            if (vdrw.has_value()) {
                bool already_done = false;

                auto& victim_dentry = vdrw->get();

                {
                    lock_guard lg(victim_dentry.lock);

                    if (victim_dentry.status == dentry_status::valid)
                        victim_dentry.status = dentry_status::going_away;
                    else
                        already_done = true;
                }

                if (!already_done) {
                    {
                        lock_guard lg(victim_dentry.parent->child_lock);
                        remove_entry_list(victim_dentry.le);
                    }

                    {
                        lock_guard lg(victim_dentry.lock);
                        victim_dentry.status = dentry_status::invalid;
                    }

                    // because removed from parent
                    victim_dentry.release();
                }

                // because of search_dentry_cache
                victim_dentry.release();
            }
        }

        handle_closed(victim.get());
    }

    if (success(status)) {
        bool moving_directories = dentry->parent != &drw->get();

        // change dentry in file

        auto name_buf = (char*)malloc(namepart.size());
        if (!name_buf) {
            drw->get().release();
            free(name);
            return nt_status::INSUFFICIENT_RESOURCES;
        }

        memcpy(name_buf, namepart.data(), namepart.size());

        // FIXME - is this racy?

        if (moving_directories) {
            lock_guard lg(dentry->parent->child_lock);

            remove_entry_list(dentry->le);
        }

        {
            lock_guard lg(dentry->lock);

            swap(name_buf, dentry->name);
            dentry->name_len = namepart.size();

            if (moving_directories) {
                dentry->parent->release();
                drw->get().acquire();
                dentry->parent = &drw->get();
            }
        }

        if (moving_directories) {
            lock_guard lg(dentry->parent->child_lock);

            insert_tail_list(dentry->parent->children, dentry->le);
        }

        free(name_buf);
    }

    drw->get().release();
    free(name);

    return status;
}

NTSTATUS file::hardlink(span<const uint8_t> buf) {
    if (ino.type != object_type::inode)
        return nt_status::INVALID_PARAMETER;

    if (buf.size() < offsetof(FILE_LINK_INFORMATION, FileName))
        return nt_status::BUFFER_TOO_SMALL;

    auto& fli = *(FILE_LINK_INFORMATION*)buf.data();

    if (buf.size() < offsetof(FILE_LINK_INFORMATION, FileName) + fli.FileNameLength)
        return nt_status::BUFFER_TOO_SMALL;

    object_ref rootdir, destdir;
    unsigned int create_result;
    optional<reference_wrapper<::dentry>> drw;

    if (fli.RootDirectory != 0) {
        rootdir = deref_handle(fli.RootDirectory);
        if (!rootdir)
            return nt_status::INVALID_HANDLE;

        if (rootdir.get().type != object_type::file)
            return nt_status::INVALID_HANDLE;

        auto& f = static_cast<file&>(rootdir.get());

        if (f.ino.type != object_type::inode)
            return nt_status::INVALID_HANDLE;

        if (static_cast<inode&>(f.ino).inode_type != inode_type::directory)
            return nt_status::INVALID_HANDLE;
    }

    // don't allow if source is directory
    auto& inode = static_cast<::inode&>(ino);
    if (inode.inode_type == inode_type::directory)
        return nt_status::INVALID_PARAMETER;

    // file name to utf-8

    auto u16name = u16string_view(fli.FileName, fli.FileNameLength / sizeof(char16_t));

    // remove trailing slashes
    while (!u16name.empty() && u16name.back() == '/') {
        u16name = u16string_view(u16name.data(), u16name.size() - 1);
    }

    auto namelen = utf16_to_utf8_len(u16name);
    if (namelen == 0)
        return nt_status::INVALID_PARAMETER;

    auto name = (char*)malloc(namelen);
    if (!name)
        return nt_status::INSUFFICIENT_RESOURCES;

    utf16_to_utf8(span(name, namelen), u16name);

    auto fn = string_view(name, namelen);

    auto& root_dir_dentry = rootdir ? *static_cast<file&>(rootdir.get()).dentry : *root_dentry;

    // open parent of destination

    auto status = open_inode(fn, root_dir_dentry, destdir, drw, FILE_OPEN,
                             FILE_DIRECTORY_FILE, create_result, true);

    if (status == nt_status::OBJECT_NAME_NOT_FOUND) {
        free(name);
        return nt_status::OBJECT_PATH_NOT_FOUND;
    } else if (!success(status)) {
        free(name);
        return status;
    }

    assert(destdir);
    assert(destdir.get().type == object_type::inode);
    assert(drw);

    auto& dest_dir_inode = static_cast<::inode&>(destdir.get());

    if (&dest_dir_inode.fs != &inode.fs) {
        drw->get().release();
        free(name);
        return nt_status::NOT_SAME_DEVICE;
    }

    string_view namepart;
    object_ref victim;

    if (auto sl = fn.find_last_of('/'); sl != string_view::npos)
        namepart = string_view(fn.data() + sl + 1, fn.size() - sl - 1);
    else
        namepart = fn;

    status = inode.hardlink(*dentry, drw->get(), namepart, fli.ReplaceIfExists,
                            victim);

    if (victim) {
        assert(victim.get().type == object_type::inode);

        if (success(status)) {
            optional<reference_wrapper<::dentry>> vdrw;

            {
                lock_guard lg(drw->get().child_lock);

                search_dentry_cache(drw->get(), namepart, vdrw);
            }

            if (vdrw.has_value()) {
                bool already_done = false;

                auto& victim_dentry = vdrw->get();

                {
                    lock_guard lg(victim_dentry.lock);

                    if (victim_dentry.status == dentry_status::valid)
                        victim_dentry.status = dentry_status::going_away;
                    else
                        already_done = true;
                }

                if (!already_done) {
                    {
                        lock_guard lg(victim_dentry.parent->child_lock);
                        remove_entry_list(victim_dentry.le);
                    }

                    {
                        lock_guard lg(victim_dentry.lock);
                        victim_dentry.status = dentry_status::invalid;
                    }

                    // because removed from parent
                    victim_dentry.release();
                }

                // because of search_dentry_cache
                victim_dentry.release();
            }
        }

        handle_closed(victim.get());
    }

    drw->get().release();
    free(name);

    return status;
}

nt_status file::set_eof(span<const uint8_t> buffer) {
    if (buffer.size() != sizeof(FILE_END_OF_FILE_INFORMATION))
        return nt_status::INVALID_PARAMETER;

    auto& feofi = *(FILE_END_OF_FILE_INFORMATION*)buffer.data();
    auto& i = static_cast<inode&>(ino);

    if (feofi.EndOfFile.QuadPart < 0)
        return nt_status::INVALID_PARAMETER;

    auto now = (uint64_t)kernel_time_page.system_time + kernel_time_page.system_time_offset;

    {
        lock_guard lock(i.inode_lock);

        i.file_size = (uint64_t)feofi.EndOfFile.QuadPart;
        i.mtime = now;
        i.ctime = now;
    }

    i.cache.mark_dirty();

    return nt_status::SUCCESS;
}

nt_status file::set_non_block(span<const uint8_t> buffer) {
    if (buffer.empty())
        return nt_status::INVALID_PARAMETER;

    nonblock = buffer[0] != 0;

    return nt_status::SUCCESS;
}

static nt_status set_information_non_inode(file& /*f*/, span<uint8_t> /*buf*/,
                                           FILE_INFORMATION_CLASS file_information_class) {
    switch (file_information_class) {
        using enum FILE_INFORMATION_CLASS;

        case MoxnoxFchown:
            // FIXME
            return nt_status::SUCCESS;

        case MoxnoxFchmod:
            // FIXME
            return nt_status::SUCCESS;

        default:
            print("set_information_non_inode: unhandled file information class {}\n",
                file_information_class);
            return nt_status::NOT_IMPLEMENTED;
    }
}

nt_status inode::set_posix_lock(uint64_t start, uint64_t length, bool write,
                                bool /*nowait*/) {
    auto& proc = *get_current_thread()->proc;

    // FIXME - detecting deadlocks if not nowait

    if (length == 0) // fcntl has already rationalized this
        return nt_status::INVALID_PARAMETER;

    if (length > numeric_limits<uint64_t>::max() - start)
        return nt_status::INVALID_PARAMETER;

    {
        lock_guard lock(posix_locks_lock);

        posix_lock_proc* this_plp = nullptr;

        // check if lock held by other process

        for (auto le = posix_lock_procs.next; le != &posix_lock_procs; le = le->next) {
            auto& plp = containing_record<posix_lock_proc>(le);

            // skip if same process
            if (&plp.proc == &proc) {
                this_plp = &plp;
                continue;
            }

            for (auto le2 = plp.locks.next; le2 != &plp.locks; le2 = le2->next) {
                auto& pl = containing_record<posix_lock>(le2);

                // skip if no overlap
                if (start >= pl.start + pl.length || pl.start >= start + length)
                    continue;

                // skip if both read locks
                if (!write && !pl.write)
                    continue;

                // FIXME - block instead if nowait not set

                return nt_status::CANT_WAIT;
            }
        }

        if (!this_plp) {
            this_plp = new posix_lock_proc(proc);
            if (!this_plp) {
                print("new failed\n");
                return nt_status::INSUFFICIENT_RESOURCES;
            }

            insert_tail_list(posix_lock_procs, this_plp->le);
        }

        posix_lock* newpl = nullptr;

        for (auto le = this_plp->locks.next; le != &this_plp->locks; le = le->next) {
            auto& pl = containing_record<posix_lock>(le);
            bool same_type = (pl.write && write) || (!pl.write && !write);

            // before, carry on
            if (pl.start + pl.length < start || (pl.start + pl.length == start && !same_type))
                continue;

            // after, stop
            if (pl.start > start + length || (pl.start == start + length && !same_type)) {
                if (newpl) {
                    newpl->start = start;
                    newpl->length = length;
                    newpl->write = write;
                } else {
                    newpl = new posix_lock;
                    if (!newpl) {
                        print("new failed\n");
                        return nt_status::INSUFFICIENT_RESOURCES;
                    }

                    newpl->start = start;
                    newpl->length = length;
                    newpl->write = write;

                    insert_tail_list(pl.le, newpl->le);
                }

                return nt_status::SUCCESS;
            }

            // in middle of existing entry
            if (pl.start < start && pl.start + pl.length > start + length && !same_type) {
                auto pl2 = new posix_lock;
                if (!pl2) {
                    print("new failed\n");
                    return nt_status::INSUFFICIENT_RESOURCES;
                }

                auto pl3 = new posix_lock;
                if (!pl3) {
                    print("new failed\n");
                    delete pl2;
                    return nt_status::INSUFFICIENT_RESOURCES;
                }

                pl2->write = write;
                pl2->start = start;
                pl2->length = length;

                pl3->write = pl.write;
                pl3->start = start + length;
                pl3->length = pl.start + pl.length - start - length;

                pl.length = start - pl.start;

                insert_tail_list(*pl.le.next, pl2->le);
                insert_tail_list(*pl2->le.next, pl3->le);

                return nt_status::SUCCESS;
            }

            // move end back
            if (pl.start < start && !same_type) {
                pl.length = start - pl.start;
                continue;
            }

            // move beginning forward
            if (pl.start + pl.length > start + length && !same_type) {
                pl.length = pl.start + pl.length - start - length;
                pl.start = start + length;

                if (newpl) {
                    newpl->start = start;
                    newpl->length = length;
                    newpl->write = write;
                } else {
                    newpl = new posix_lock;
                    if (!newpl) {
                        print("new failed\n");
                        return nt_status::INSUFFICIENT_RESOURCES;
                    }

                    newpl->start = start;
                    newpl->length = length;
                    newpl->write = write;

                    insert_tail_list(pl.le, newpl->le);
                }

                return nt_status::SUCCESS;
            }

            // consume next
            if (pl.start + pl.length > start + length)
                length = pl.start + pl.length - start;

            // consume previous
            if (pl.start < start) {
                length += start - pl.start;
                start = pl.start;
            }

            if (!newpl)
                newpl = &pl;
            else {
                auto le2 = le->prev;

                remove_entry_list(pl.le);
                delete &pl;

                le = le2;
            }
        }

        if (newpl) {
            newpl->start = start;
            newpl->length = length;
            newpl->write = write;
        } else {
            newpl = new posix_lock;
            if (!newpl) {
                print("new failed\n");
                return nt_status::INSUFFICIENT_RESOURCES;
            }

            newpl->start = start;
            newpl->length = length;
            newpl->write = write;

            insert_tail_list(this_plp->locks, newpl->le);
        }
    }

    return nt_status::SUCCESS;
}

nt_status file::set_posix_lock(span<const uint8_t> buffer) {
    if (buffer.size() < sizeof(FILE_MN_SET_POSIX_LOCK))
        return nt_status::INVALID_PARAMETER;

    auto& params = *(FILE_MN_SET_POSIX_LOCK*)buffer.data();

    if (ino.type != object_type::inode)
        return nt_status::INVALID_PARAMETER;

    // FIXME - fail if params.write set but file not opened for writing

    auto& i = static_cast<inode&>(ino);

    return i.set_posix_lock(params.start, params.length, params.write,
                            params.nowait);
}

NTSTATUS file::set_information(void* buffer, uint32_t buffer_size,
                               FILE_INFORMATION_CLASS file_information_class) {
    switch (file_information_class) {
        using enum FILE_INFORMATION_CLASS;

        case RenameInformation:
            return rename(span((uint8_t*)buffer, buffer_size));

        case LinkInformation:
            return hardlink(span((uint8_t*)buffer, buffer_size));

        case PositionInformation: {
            if (ino.type != object_type::inode)
                return nt_status::INVALID_DEVICE_REQUEST;

            if (buffer_size < sizeof(FILE_POSITION_INFORMATION))
                return nt_status::BUFFER_TOO_SMALL;

            auto& fpi = *(FILE_POSITION_INFORMATION*)buffer;

            if (fpi.CurrentByteOffset.QuadPart < 0)
                return nt_status::INVALID_PARAMETER;

            {
                lock_guard lg(lock);
                file_offset = (uint64_t)fpi.CurrentByteOffset.QuadPart;
            }

            return nt_status::SUCCESS;
        }

        case EndOfFileInformation:
            if (ino.type != object_type::inode)
                return nt_status::INVALID_PARAMETER;

            return set_eof(span((uint8_t*)buffer, buffer_size));

        case MoxnoxPosixNonBlock:
            return set_non_block(span((uint8_t*)buffer, buffer_size));

        case MoxnoxSetPosixLock:
            return set_posix_lock(span((uint8_t*)buffer, buffer_size));

        default:
            if (ino.type != object_type::inode) {
                return set_information_non_inode(*this, span((uint8_t*)buffer, buffer_size),
                                                 file_information_class);
            } else {
                return static_cast<inode&>(ino).set_information(span((uint8_t*)buffer, buffer_size),
                                                                *dentry,
                                                                file_information_class);
            }
    }
}

extern "C"
void trace_NtQueryInformationFile(uintptr_t* args) {
    auto file_handle = (HANDLE)args[0];
    auto buffer = (void*)args[1];
    auto buffer_size = (uint32_t)args[2];
    auto file_information_class = (FILE_INFORMATION_CLASS)args[3];

    start_deferred_print();

    print("NtQueryInformationFile: file_handle = ");
    print_handle(file_handle);
    print(", buffer = {:x}, buffer_size = {}, file_information_class = {}\n",
          (uintptr_t)buffer, buffer_size, file_information_class);

    end_deferred_print();
}

extern "C"
NTSTATUS NtQueryInformationFile(HANDLE file_handle, IO_STATUS_BLOCK* iosb, void* buffer,
                                uint32_t buffer_size, FILE_INFORMATION_CLASS file_information_class) {
    uint64_t retlen;

    TRACE4("NtQueryInformationFile", file_handle, buffer, buffer_size,
           file_information_class);

    auto objrw = deref_handle(file_handle);
    if (!objrw)
        return nt_status::INVALID_HANDLE;

    auto& obj = objrw.get();

    if (obj.type != object_type::file)
        return nt_status::INVALID_HANDLE;

    auto& f = static_cast<file&>(obj);

    auto ret = f.query_information(buffer, buffer_size, file_information_class,
                                   retlen);

    if (!success(ret) && ret != nt_status::BUFFER_OVERFLOW) {
        iosb->Information = 0;
        return ret;
    }

    iosb->Information = retlen;

    return ret;
}

static nt_status query_name(dentry& dentry, span<uint8_t> buf, uint64_t& retlen) {
    char* path;
    size_t path_len;

    if (buf.size() < offsetof(FILE_NAME_INFORMATION, FileName))
        return nt_status::BUFFER_TOO_SMALL;

    auto& fni = *(FILE_NAME_INFORMATION*)buf.data();

    auto status = get_dentry_path(dentry, path, path_len);
    if (!success(status)) {
        print("get_dentry_path returned {}\n", status);
        return status;
    }

    auto maxlen = (buf.size() - offsetof(FILE_NAME_INFORMATION, FileName)) / sizeof(char16_t);

    auto name = string_view(path, path_len);

    auto u16len = utf8_to_utf16_len(name);

    fni.FileNameLength = (char32_t)(u16len * sizeof(char16_t));

    utf8_to_utf16(span(fni.FileName, maxlen), name);

    free(path);

    if (maxlen < u16len) {
        retlen = offsetof(FILE_NAME_INFORMATION, FileName) + (maxlen * sizeof(char16_t));
        return nt_status::BUFFER_OVERFLOW;
    } else {
        retlen = offsetof(FILE_NAME_INFORMATION, FileName) + (u16len * sizeof(char16_t));
        return nt_status::SUCCESS;
    }
}

static nt_status query_information_non_inode_basic(span<uint8_t> buffer,
                                                   uint64_t& retlen) {
    if (buffer.size() < sizeof(FILE_BASIC_INFORMATION))
        return nt_status::BUFFER_TOO_SMALL;

    auto& fbi = *(FILE_BASIC_INFORMATION*)buffer.data();

    // FIXME - should populate with time that object was created?
    fbi.CreationTime.QuadPart = 0;
    fbi.LastAccessTime.QuadPart = 0;
    fbi.LastWriteTime.QuadPart = 0;
    fbi.ChangeTime.QuadPart = 0;
    fbi.FileAttributes = FILE_ATTRIBUTE_DEVICE;

    retlen = sizeof(fbi);

    return nt_status::SUCCESS;
}

static nt_status query_information_non_inode_standard(span<uint8_t> buffer,
                                                      uint64_t& retlen) {
    if (buffer.size() < sizeof(FILE_STANDARD_INFORMATION))
        return nt_status::BUFFER_TOO_SMALL;

    auto& fsi = *(FILE_STANDARD_INFORMATION*)buffer.data();

    fsi.AllocationSize.QuadPart = 0;
    fsi.EndOfFile.QuadPart = 0;
    fsi.NumberOfLinks = 1;
    fsi.DeletePending = 0;
    fsi.Directory = 0;

    retlen = sizeof(fsi);

    return nt_status::SUCCESS;
}

static nt_status query_information_non_inode_internal(file& f, span<uint8_t> buffer,
                                                      uint64_t& retlen) {
    if (buffer.size() < sizeof(FILE_INTERNAL_INFORMATION))
        return nt_status::BUFFER_TOO_SMALL;

    auto& fii = *(FILE_INTERNAL_INFORMATION*)buffer.data();

    auto& obj = f.ino;

    {
        lock_guard lock(name_lock);

        if (!obj.name)
            fii.IndexNumber.QuadPart = 0;
        else
            fii.IndexNumber.QuadPart = obj.name->index;
    }

    retlen = sizeof(fii);

    return nt_status::SUCCESS;
}

static nt_status query_information_non_inode_lx(file& f, span<uint8_t> buffer,
                                                uint64_t& retlen) {
    if (buffer.size() < sizeof(FILE_STAT_LX_INFORMATION))
        return nt_status::BUFFER_TOO_SMALL;

    auto& fsli = *(FILE_STAT_LX_INFORMATION*)buffer.data();

    {
        lock_guard lock(name_lock);

        if (!f.ino.name)
            fsli.FileId.QuadPart = 0;
        else
            fsli.FileId.QuadPart = f.ino.name->index;
    }

    fsli.CreationTime.QuadPart = 0; // FIXME - should populate with time that object was created?
    fsli.LastAccessTime.QuadPart = 0;
    fsli.LastWriteTime.QuadPart = 0;
    fsli.ChangeTime.QuadPart = 0;
    fsli.AllocationSize.QuadPart = 0;
    fsli.EndOfFile.QuadPart = 0;
    fsli.NumberOfLinks = 1;
    fsli.EffectiveAccess = 0; // FIXME
    fsli.LxFlags = LX_FILE_METADATA_HAS_UID | LX_FILE_METADATA_HAS_GID |
                   LX_FILE_METADATA_HAS_MODE;
    fsli.LxUid = 0;
    fsli.LxGid = 0;

    switch (f.ino.type) {
        case object_type::chardev: {
            auto& c = static_cast<chardev&>(f.ino);

            fsli.LxMode = (c.is_pipe() ? S_IFIFO : S_IFCHR) | 0644;
            break;
        }

        case object_type::volume:
            fsli.LxMode = S_IFBLK | 0644;
            break;

        case object_type::socket:
            fsli.LxMode = S_IFSOCK | 0777;
            break;

        default:
            fsli.LxMode = S_IFREG | 0644;
            break;
    }

    fsli.LxDeviceIdMajor = 0;
    fsli.LxDeviceIdMinor = 0;
    fsli.FileAttributes = FILE_ATTRIBUTE_DEVICE;
    fsli.ReparseTag = 0;

    retlen = sizeof(fsli);

    return nt_status::SUCCESS;
}

static nt_status query_information_non_inode(file& f, span<uint8_t> buf,
                                             FILE_INFORMATION_CLASS file_information_class,
                                             uint64_t& retlen) {
    switch (file_information_class) {
        using enum FILE_INFORMATION_CLASS;

        case BasicInformation:
            return query_information_non_inode_basic(buf, retlen);

        case StandardInformation:
            return query_information_non_inode_standard(buf, retlen);

        case InternalInformation:
            return query_information_non_inode_internal(f, buf, retlen);

        case StatLxInformation:
            return query_information_non_inode_lx(f, buf, retlen);

        default:
            return nt_status::NOT_SUPPORTED;
    }
}

NTSTATUS file::query_information(void* buffer, uint32_t buffer_size,
                                 FILE_INFORMATION_CLASS file_information_class,
                                 uint64_t& retlen) {
    switch (file_information_class) {
        using enum FILE_INFORMATION_CLASS;

        case NameInformation: {
            if (ino.type != object_type::inode)
                return nt_status::INVALID_PARAMETER;

            ::dentry* d;

            {
                lock_guard lg(lock);

                d = dentry;
                d->acquire();
            }

            auto ret = query_name(*d, span((uint8_t*)buffer, buffer_size), retlen);

            d->release();

            return ret;
        }

        case PositionInformation: {
            if (buffer_size < sizeof(FILE_POSITION_INFORMATION))
                return nt_status::BUFFER_TOO_SMALL;

            auto& fpi = *(FILE_POSITION_INFORMATION*)buffer;

            {
                lock_guard lg(lock);
                fpi.CurrentByteOffset.QuadPart = (int64_t)file_offset;
            }

            retlen = sizeof(FILE_POSITION_INFORMATION);

            return nt_status::SUCCESS;
        }

        case MoxnoxPosixNonBlock:
            if (buffer_size < sizeof(bool))
                return nt_status::BUFFER_TOO_SMALL;

            *(bool*)buffer = nonblock ? 1 : 0;

            retlen = sizeof(bool);

            return nt_status::SUCCESS;

        default: {
            auto buf = span((uint8_t*)buffer, buffer_size);

            if (ino.type != object_type::inode) {
                return query_information_non_inode(*this, buf, file_information_class,
                                                   retlen);
            } else {
                return static_cast<inode&>(ino).query_information(buf, file_information_class,
                                                                  retlen);
            }
        }
    }
}

nt_status get_dentry_path(dentry& dr, char*& path, size_t& path_len) {
    size_t len;
    char* s;

    if (&dr == root_dentry) {
        path = (char*)malloc(1);
        if (!path)
            return nt_status::INSUFFICIENT_RESOURCES;

        path[0] = '/';
        path_len = 1;

        return nt_status::SUCCESS;
    }

    while (true) {
        size_t left;
        bool done = false;

        // go to root and count characters

        len = 0;

        auto* d = &dr;
        while (d->name_len != 0) {
            {
                lock_guard lg(d->lock);
                len += d->name_len + 1;
            }

            d = d->parent;
        }

        s = (char*)malloc(len);
        if (!s)
            return nt_status::INSUFFICIENT_RESOURCES;

        auto p = s + len;

        // go to root again and copy

        left = len;
        d = &dr;
        while (d->name_len != 0) {
            {
                lock_guard lg(d->lock);

                if (d->name_len >= left)
                    break;

                memcpy(p - d->name_len, d->name, d->name_len);
                *(p - d->name_len - 1) = '/';

                p -= d->name_len + 1;
                left -= d->name_len + 1;
            }

            d = d->parent;

            if (d->name_len == 0) {
                if (left == 0)
                    done = true;
                break;
            }
        }

        if (done)
            break;

        free(s);
    }

    path = s;
    path_len = len;

    return nt_status::SUCCESS;
}

void file::last_handle_closed() {
    if (access_mask & (FILE_READ_DATA | FILE_WRITE_DATA | FILE_APPEND_DATA) && ino.type == object_type::inode) {
        auto& i = static_cast<inode&>(ino);
        fifo_object* fifo = nullptr;

        if (i.inode_type == inode_type::fifo) {
            sync_lock sl;
            lock_guard lg(sl);

            if (i.fifo) {
                i.fifo->acquire();
                fifo = i.fifo;
            }
        }

        if (fifo) {
            {
                sync_lock sl;
                lock_guard lock1(sl);
                lock_guard lock2(fifo->lock);

                if (access_mask & FILE_READ_DATA) {
                    fifo->num_readers--;

                    if (fifo->num_readers == 0) {
                        fifo->have_readers.signalled = false;
                        fifo->not_full_event.set_holding_sync_lock();

                        fifo->write_signalled = true;
                        wake_up_waiters(fifo->waiters, WAIT_TYPE_WRITING);
                    }
                }

                if (access_mask & (FILE_WRITE_DATA | FILE_APPEND_DATA)) {
                    fifo->num_writers--;

                    if (fifo->num_writers == 0) {
                        fifo->have_writers.signalled = false;

                        if (!fifo->signalled) {
                            fifo->signalled = true;
                            wake_up_waiters(fifo->waiters, WAIT_TYPE_READING);
                        }
                    }
                }
            }

            fifo->release();
        }
    }

    if (dentry)
        dentry->open_file_count--;
}

bool init_vfsctl() {
    auto pptr = new vfsctl;
    if (!pptr) {
        print("new failed\n");
        return false;
    }

    vfsctl_obj = object_ref{*pptr};

    auto status = name_object(vfsctl_obj.get(), "vfsctl");
    if (!success(status)) {
        print("name_object returned {}\n", status);
        return false;
    }

    return true;
}

static nt_status vfsctl_mount(span<const uint8_t> inbuf) {
    if (inbuf.size() < sizeof(MOXNOX_VFSCTL_MOUNT))
        return nt_status::BUFFER_TOO_SMALL;

    auto& m = *(MOXNOX_VFSCTL_MOUNT*)inbuf.data();

    // get mount_type

    if (m.mount_type >= inbuf.size())
        return nt_status::INVALID_PARAMETER;

    auto mount_type = string_view((char*)&m + m.mount_type, inbuf.size() - m.mount_type);

    if (auto n = mount_type.find('\0'); n != string_view::npos)
        mount_type = string_view(mount_type.data(), n);

    object_ref sourcerw;

    if (m.source != 0) {
        // dereference source (FIXME - can be 0 if "none"?)

        sourcerw = deref_handle(m.source);
        if (!sourcerw)
            return nt_status::INVALID_HANDLE;

        // FIXME - check access mask?

        if (sourcerw.get().type == object_type::file) {
            auto& f = static_cast<file&>(sourcerw.get());

            f.ino.acquire();
            object_ref tmp{f.ino};

            sourcerw.swap(tmp);
        }

        if (sourcerw.get().type != object_type::volume)
            return nt_status::OBJECT_TYPE_MISMATCH;
    }

    // dereference directory

    auto dirrw = deref_handle(m.directory);
    if (!dirrw)
        return nt_status::INVALID_HANDLE;

    // FIXME - check access mask?

    if (dirrw.get().type != object_type::file)
        return nt_status::OBJECT_TYPE_MISMATCH;

    auto& dir = static_cast<file&>(dirrw.get());

    if (dir.ino.type != object_type::inode)
        return nt_status::OBJECT_TYPE_MISMATCH;

    if (static_cast<inode&>(dir.ino).inode_type != inode_type::directory)
        return nt_status::NOT_A_DIRECTORY;

    // allocate mount_point

    auto mp = new mount_point(*dir.dentry);
    if (!mp) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    // do mount

    ::filesystem* fs;
    filesystem_recognizer* type;
    volume* vol = nullptr;

    if (sourcerw) {
        auto& source = static_cast<volume&>(sourcerw.get());

        {
            lock_guard lock(source.mount_lock);

            if (source.filesystem) { // volume already mounted
                mp->release();
                return nt_status::DEVICE_NOT_READY;
            }

            nt_status status;

            // FIXME - spinlock for this (refcount recognizers once we have a module system?)
            for (auto le = filesystem_types.next; le != &filesystem_types; le = le->next) {
                auto& r = containing_record<filesystem_recognizer>(le);

                if (!r.try_mount_volume) {
                    if (r.name == mount_type)
                        return nt_status::INVALID_PARAMETER;

                    continue;
                }

                if (!mount_type.empty() && r.name != mount_type)
                    continue;

                status = r.try_mount_volume(source, m.options);

                if (status == nt_status::UNRECOGNIZED_VOLUME && mount_type.empty())
                    continue;
                else if (!success(status)) {
                    mp->release();
                    return status;
                }

                type = &r;
                vol = &source;

                break;
            }

            if (!vol) {
                mp->release();
                return mount_type.empty() ? nt_status::UNRECOGNIZED_VOLUME : nt_status::FS_DRIVER_REQUIRED;
            }

            fs = &source.filesystem->get();
        }
    } else {
        bool found_fs_type = false;

        // FIXME - spinlock for this (refcount recognizers once we have a module system?)
        for (auto le = filesystem_types.next; le != &filesystem_types; le = le->next) {
            auto& r = containing_record<filesystem_recognizer>(le);

            if (r.name != mount_type)
                continue;

            found_fs_type = true;

            if (!r.mount_without_volume) {
                mp->release();
                return nt_status::INVALID_PARAMETER;
            }

            fs = r.mount_without_volume(m.options); // FIXME - get returning error
            if (!fs) {
                mp->release();
                return nt_status::INTERNAL_ERROR;
            }

            type = &r;
            vol = nullptr;

            break;
        }

        if (!found_fs_type) {
            mp->release();
            return nt_status::FS_DRIVER_REQUIRED;
        }
    }

    inode* ino2 = fs->root_inode;

    ino2->acquire();

    auto& d = *dir.dentry;
    nt_status status;

    {
        lock_guard lg(d.lock);

        if (d.status != dentry_status::valid)
            status = nt_status::INVALID_PARAMETER;
        else if (d.mount_point) // FIXME - allow overmounting?
            status = nt_status::INVALID_PARAMETER;
        else {
            swap(d.inode, ino2);
            d.mount_point = true;
            status = nt_status::SUCCESS;
        }
    }

    ino2->release();

    if (!success(status)) {
        if (sourcerw) {
            auto& source = static_cast<volume&>(sourcerw.get());

            {
                lock_guard lock(source.mount_lock);

                source.filesystem = nullopt;
            }
        }
    }

    mp->num = mount_point_num.fetch_add(1) + 1;
    mp->type = type;

    if (vol) {
        mp->vol = vol;
        vol->acquire();
    }

    mp->d.acquire();

    {
        lock_guard lg(mount_points_lock);
        insert_tail_list(mount_points, mp->le);
    }

    return status;
}

static nt_status vfsctl_list_mounts(span<uint8_t> outbuf, uintptr_t& written) {
    if (outbuf.size() < offsetof(MOXNOX_VFSCTL_LIST_MOUNTS, ids))
        return nt_status::BUFFER_TOO_SMALL;

    auto& m = *(MOXNOX_VFSCTL_LIST_MOUNTS*)outbuf.data();
    auto max_num = (outbuf.size() - offsetof(MOXNOX_VFSCTL_LIST_MOUNTS, ids)) / sizeof(uint64_t);

    m.num = 0;

    {
        lock_guard lg(mount_points_lock);

        for (auto le = mount_points.next; le != &mount_points; le = le->next) {
            auto& mp = containing_record<mount_point>(le);

            if (m.num < max_num)
                m.ids[m.num] = mp.num;

            m.num++;
        }
    }

    if (m.num > max_num) {
        written = offsetof(MOXNOX_VFSCTL_LIST_MOUNTS, ids) + (max_num * sizeof(uint64_t));
        return nt_status::BUFFER_OVERFLOW;
    } else {
        written = offsetof(MOXNOX_VFSCTL_LIST_MOUNTS, ids) + (m.num * sizeof(uint64_t));
        return nt_status::SUCCESS;
    }
}

static nt_status get_source_path(object& obj, char*& src, size_t& src_len) {
    // FIXME - do we really want to be hardcoding this path?
    static const string_view prefix = "/dev/";

    do {
        size_t len;

        {
            lock_guard lg(name_lock);

            if (!obj.name) {
                src = nullptr;
                src_len = 0;
                return nt_status::NOT_FOUND;
            }

            len = obj.name->name_len;
        }

        src = (char*)malloc(prefix.size() + len);
        if (!src)
            return nt_status::INSUFFICIENT_RESOURCES;

        memcpy(src, prefix.data(), prefix.size());

        src_len = prefix.size() + len;

        {
            lock_guard lg(name_lock);

            if (!obj.name) {
                src = nullptr;
                src_len = 0;
                return nt_status::NOT_FOUND;
            }

            if (obj.name->name_len == len) {
                memcpy(src + prefix.size(), obj.name->name,
                       obj.name->name_len);
                return nt_status::SUCCESS;
            }
        }

        free(src);
    } while (true);
}

static nt_status vfsctl_get_mount_info(span<const uint8_t> inbuf,
                                       span<uint8_t> outbuf,
                                       uintptr_t& written) {
    if (inbuf.size() < sizeof(uint64_t))
        return nt_status::BUFFER_TOO_SMALL;

    if (outbuf.size() < sizeof(MOXNOX_VFSCTL_MOUNT_INFO))
        return nt_status::BUFFER_TOO_SMALL;

    auto num = *(uint64_t*)inbuf.data();
    auto& out = *(MOXNOX_VFSCTL_MOUNT_INFO*)outbuf.data();
    mount_point* pmp = nullptr;

    {
        lock_guard lg(mount_points_lock);

        for (auto le = mount_points.next; le != &mount_points; le = le->next) {
            auto& mp2 = containing_record<mount_point>(le);

            if (mp2.num == num) {
                mp2.acquire();
                pmp = &mp2;
                break;
            }
        }
    }

    if (!pmp)
        return nt_status::NOT_FOUND;

    auto& mp = *pmp;
    char* path;
    char* src = nullptr;
    size_t path_len, src_len = 0;

    auto status = get_dentry_path(mp.d, path, path_len);
    if (!success(status)) {
        print("get_dentry_path returned {}\n", status);
        mp.release();
        return status;
    }

    if (mp.vol) {
        status = get_source_path(*mp.vol, src, src_len);
        if (status == nt_status::NOT_FOUND) {
            src_len = 1;
            src = (char*)malloc(src_len);
            if (!src) {
                print("malloc failed\n");
                free(path);
                mp.release();
                return nt_status::INSUFFICIENT_RESOURCES;
            }

            src[0] = '?';
        } else if (!success(status)) {
            print("get_source_path returned {}\n", status);
            free(path);
            mp.release();
            return status;
        }
    }

    string_view source = string_view(src, src_len);
    string_view directory = string_view(path, path_len);
    string_view type = mp.type->name;

    auto left = outbuf.size() - sizeof(MOXNOX_VFSCTL_MOUNT_INFO);

    out.source = 0;
    out.directory = 0;
    out.type = 0;
    written = sizeof(MOXNOX_VFSCTL_MOUNT_INFO);

    if (!source.empty()) {
        if (left < source.size() + 1) {
            mp.release();
            free(path);
            free(src);
            return nt_status::BUFFER_OVERFLOW;
        }

        out.source = (uint32_t)(outbuf.size() - left);
        memcpy((char*)&out + out.source, source.data(), source.size());
        *((char*)&out + out.source + source.size()) = 0;
        left -= source.size() + 1;
        written += source.size() + 1;
    }

    if (!directory.empty()) {
        if (left < directory.size() + 1) {
            mp.release();
            free(path);
            free(src);
            return nt_status::BUFFER_OVERFLOW;
        }

        out.directory = (uint32_t)(outbuf.size() - left);
        memcpy((char*)&out + out.directory, directory.data(), directory.size());
        *((char*)&out + out.directory + directory.size()) = 0;
        left -= directory.size() + 1;
        written += directory.size() + 1;
    }

    if (!type.empty()) {
        if (left < type.size() + 1) {
            mp.release();
            free(path);
            free(src);
            return nt_status::BUFFER_OVERFLOW;
        }

        out.type = (uint32_t)(outbuf.size() - left);
        memcpy((char*)&out + out.type, type.data(), type.size());
        *((char*)&out + out.type + type.size()) = 0;
        left -= type.size() + 1;
        written += type.size() + 1;
    }

    mp.release();
    free(path);
    free(src);

    return nt_status::SUCCESS;
}

static bool evict_children(dentry& d, bool force) {
    lock_guard lg(d.child_lock);

    // FIXME - absolutely should not be doing recursion here

    while (!is_list_empty(d.children)) {
        auto& c = containing_record<dentry>(d.children.next);

        {
            lock_guard lg2(c.lock);

            if (!force && c.open_file_count != 0)
                return false;

            if (!evict_children(c, force))
                return false;

            if (c.open_file_count == 0)
                c.parent = nullptr;

            remove_entry_list(c.le);
        }

        c.release();
    }

    return true;
}

static nt_status do_unmount(dentry& d, bool force) {
    dentry* parent_dentry;
    nt_status status;

    {
        lock_guard lg(d.lock);

        parent_dentry = d.parent;

        if (parent_dentry)
            parent_dentry->acquire();
    }

    if (!parent_dentry) {
        d.release();
        return nt_status::INTERNAL_ERROR;
    }

    status = nt_status::SUCCESS;

    {
        lock_guard lg(parent_dentry->child_lock);
        lock_guard lg2(d.lock);

        if (d.parent != parent_dentry)
            status = nt_status::INTERNAL_ERROR;
        else if (!d.mount_point)
            status = nt_status::INVALID_PARAMETER;
        else if (!force && d.open_file_count != 0)
            status = nt_status::ACCESS_DENIED;
        else if (!evict_children(d, force))
            status = nt_status::ACCESS_DENIED;

        if (success(status))
            remove_entry_list(d.le);
    }

    parent_dentry->release();

    if (!success(status)) {
        d.release();
        return status;
    }

    // remove from mount_points list
    mount_point* pmp = nullptr;

    {
        lock_guard lg(mount_points_lock);

        for (auto le = mount_points.next; le != &mount_points; le = le->next) {
            auto& mp2 = containing_record<mount_point>(le);

            if (&mp2.d == &d) {
                mp2.acquire();
                remove_entry_list(mp2.le);
                pmp = &mp2;
                break;
            }
        }
    }

    if (!pmp) {
        d.release();
        return nt_status::INTERNAL_ERROR;
    }

    auto& mp = *pmp;

    auto& ino = *d.inode;

    ino.acquire();
    d.release();

    if (mp.vol) {
        lock_guard lock(mp.vol->mount_lock);

        mp.vol->filesystem->get().release();
        mp.vol->filesystem = nullopt;
    }

    mp.release();

    // make FS readonly
    status = ino.fs.flush(true);
    if (!success(status)) {
        print("filesystem::flush returned {}\n", status);
        ino.release();
        return status;
    }

    ino.release();

    return nt_status::SUCCESS;
}

static nt_status vfsctl_unmount(span<const uint8_t> inbuf) {
    nt_status status;
    object_ref inorw;
    optional<reference_wrapper<dentry>> drw;
    unsigned int create_result;
    bool force;

    if (inbuf.size() < sizeof(MOXNOX_VFSCTL_UNMOUNT))
        return nt_status::BUFFER_TOO_SMALL;

    auto& m = *(MOXNOX_VFSCTL_UNMOUNT*)inbuf.data();

    if (inbuf.size() < offsetof(MOXNOX_VFSCTL_UNMOUNT, path) + m.path_len)
        return nt_status::BUFFER_TOO_SMALL;

    if (m.flags & ~MOXNOX_VFSCTL_UNMOUNT_FLAGS_FORCE)
        return nt_status::INVALID_PARAMETER;

    force = m.flags & MOXNOX_VFSCTL_UNMOUNT_FLAGS_FORCE;

    auto path = string_view(m.path, m.path_len);

    // open file

    status = open_inode(path, *root_dentry, inorw, drw, FILE_OPEN,
                        FILE_DIRECTORY_FILE, create_result, false);
    if (!success(status))
        return status;

    if (!drw.has_value()) // shouldn't happen
        return nt_status::INTERNAL_ERROR;

    // cleanup and make sure not in use

    return do_unmount(drw->get(), force);
}

nt_status vfsctl::ioctl(uint32_t code, span<const uint8_t> inbuf,
                        span<uint8_t> outbuf, uintptr_t& written) {
    switch (code) {
        case IOCTL_MOXNOX_VFSCTL_MOUNT:
            return vfsctl_mount(inbuf);

        case IOCTL_MOXNOX_VFSCTL_LIST_MOUNTS:
            return vfsctl_list_mounts(outbuf, written);

        case IOCTL_MOXNOX_VFSCTL_GET_MOUNT_INFO:
            return vfsctl_get_mount_info(inbuf, outbuf, written);

        case IOCTL_MOXNOX_VFSCTL_UNMOUNT:
            return vfsctl_unmount(inbuf);

        default:
            return nt_status::NOT_IMPLEMENTED;
    }
}

void surprise_removal(volume& v) {
    {
        lock_guard lg(v.mount_lock);

        if (!v.filesystem.has_value())
            return;
    }

    mount_point* pmp = nullptr;

    {
        lock_guard lock(mount_points_lock);

        for (auto le = mount_points.next; le != &mount_points; le = le->next) {
            auto& mp = containing_record<::mount_point>(le);

            if (mp.vol != &v)
                continue;

            pmp = &mp;
            pmp->acquire();
        }
    }

    if (!pmp)
        return;

    auto& mp = *pmp;

    auto status = do_unmount(mp.d, true);
    if (!success(status))
        print("do_unmount returned {}\n", status);

    mp.release();
}

void remove_partitions(volume& v) {
    struct part {
        list_entry le;
        diskpart* part;
    };

    list_entry parts = { &parts, &parts };

    {
        lock_guard lg(v.part_lock);

        for (auto le = v.part_list.next; le != &v.part_list; le = le->next) {
            auto& dp = *(diskpart*)((uint8_t*)le - offsetof(diskpart, part_le));

            auto p = new part;
            if (!p) {
                print("new failed\n");
                // FIXME - probably should cleanup list done so far
                return;
            }

            dp.acquire();

            p->part = &dp;
            insert_tail_list(parts, p->le);
        }
    }

    while (!is_list_empty(parts)) {
        auto& p = containing_record<part>(parts.next);

        surprise_removal(*p.part);

        remove_object_name(*p.part);
        p.part->release();

        remove_entry_list(p.le);
        delete &p;
    }
}

nt_status file::fs_control(uint32_t code, span<const uint8_t> inbuf,
                           span<uint8_t> outbuf, uint32_t& retlen) {
    if (ino.type != object_type::inode)
        return nt_status::INVALID_PARAMETER;

    auto& i = static_cast<inode&>(ino);

    return i.fs_control(code, inbuf, outbuf, retlen, dentry);
}

static void print_fsctl_code(uint32_t code) {
    switch (code) {
        case FSCTL_SET_REPARSE_POINT:
            print("FSCTL_SET_REPARSE_POINT");
            break;

        case FSCTL_GET_REPARSE_POINT:
            print("FSCTL_GET_REPARSE_POINT");
            break;

        default:
            print("{:x}", code);
            break;
    }
}

extern "C"
void trace_NtFsControlFile(uintptr_t* args) {
    auto file_handle = (HANDLE)args[0];
    auto code = (uint32_t)args[1];
    auto input_buffer = (void*)args[2];
    auto input_buffer_length = (uint32_t)args[3];
    auto output_buffer = (void*)args[4];
    auto output_buffer_length = (uint32_t)args[5];

    start_deferred_print();

    print("NtFsControlFile: file_handle = ");
    print_handle(file_handle);
    print(", code = ");
    print_fsctl_code(code);
    print(", input_buffer = {:x}, input_buffer_length = {}, output_buffer = {:x}, output_buffer_length = {}\n",
          (uintptr_t)input_buffer, input_buffer_length,
          (uintptr_t)output_buffer, output_buffer_length);

    end_deferred_print();
}

extern "C"
NTSTATUS NtFsControlFile(HANDLE file_handle, HANDLE /*event*/,
                         PIO_APC_ROUTINE /*apc_routine*/, void* /*apc_context*/,
                         IO_STATUS_BLOCK* iosb, uint32_t code,
                         void* input_buffer, uint32_t input_buffer_length,
                         void* output_buffer, uint32_t output_buffer_length) {
    uint32_t retlen = 0;

    TRACE6("NtFsControlFile", file_handle, code, input_buffer, input_buffer_length,
           output_buffer, output_buffer_length);

    auto objrw = deref_handle(file_handle);
    if (!objrw)
        return nt_status::INVALID_HANDLE;

    auto& obj = objrw.get();

    if (obj.type != object_type::file)
        return nt_status::INVALID_HANDLE;

    auto& f = static_cast<file&>(obj);

    auto ret = f.fs_control(code, span((uint8_t*)input_buffer, input_buffer_length),
                            span((uint8_t*)output_buffer, output_buffer_length),
                            retlen);

    if (success(ret))
        iosb->Information = retlen;

    return ret;
}

nt_status file::query_volume(FS_INFORMATION_CLASS fs_information_class,
                             span<uint8_t> buf, uint32_t& retlen) {
    // FIXME - should work for entries in devfs too?
    if (ino.type != object_type::inode)
        return nt_status::INVALID_PARAMETER;

    auto& i = static_cast<inode&>(ino);

    return i.fs.query_volume(fs_information_class, buf, retlen);
}

extern "C"
void trace_NtQueryVolumeInformationFile(uintptr_t* args) {
    auto file_handle = (HANDLE)args[0];
    auto fs_information = (void*)args[1];
    auto length = (uint32_t)args[2];
    auto fs_information_class = (FS_INFORMATION_CLASS)args[3];

    start_deferred_print();

    print("NtQueryVolumeInformationFile: file_handle = ");
    print_handle(file_handle);
    print(", fs_information = {:x}, length = {}, fs_information_class = {}\n",
          (uintptr_t)fs_information, length, fs_information_class);

    end_deferred_print();
}

extern "C"
NTSTATUS NtQueryVolumeInformationFile(HANDLE file_handle, IO_STATUS_BLOCK* iosb,
                                      void* fs_information, uint32_t length,
                                      FS_INFORMATION_CLASS fs_information_class) {
    uint32_t retlen = 0;

    TRACE4("NtQueryVolumeInformationFile", file_handle, fs_information, length,
           fs_information_class);

    auto objrw = deref_handle(file_handle);
    if (!objrw)
        return nt_status::INVALID_HANDLE;

    auto& obj = objrw.get();

    if (obj.type != object_type::file)
        return nt_status::INVALID_HANDLE;

    auto& f = static_cast<file&>(obj);

    auto ret = f.query_volume(fs_information_class,
                              span((uint8_t*)fs_information, length), retlen);

    if (success(ret))
        iosb->Information = retlen;

    return ret;
}

void inode::last_handle_closed() {
    fifo_object* f = nullptr;

    {
        sync_lock sl;
        lock_guard lock1(inode_lock);
        lock_guard lock2(sl);

        if (handle_count != 0) // avoid race
            return;

        swap(f, fifo);
    }

    if (f)
        f->release();
}

rw_sync_object& inode::get_rw_sync_object() {
    // sync_lock held, this is okay
    if (fifo)
        return *fifo;

    return *this;
}
