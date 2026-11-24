// SPDX-License-Identifier: GPL-2.0-only
#include <string.h>
#include <shared_mutex>
#include <span>
#include <optional>
#include <string_view>
#include <mutex>
#include <memory>
#include <atomic>
#include <sys/stat.h>
#include "assert.h"

import nt;
import bsod;
import print;
import mem;
import config;
import list;
import volume;
import timer;
import vfs;
import unicode;
import objects;
import object_ref;
import page_cache;
import spinlock;
import kernel_time_page;
import module_init;
import moxnox_ioctls;

using namespace std;

static const size_t NAME_MAX_LEN = 255;
static atomic<uint64_t> last_inode_number = 0;

class tmpfs_filesystem;

class tmpfs_inode : public inode {
public:
    tmpfs_inode(tmpfs_filesystem& fs, enum inode_type inode_type, uint32_t mode,
                uint32_t uid, uint32_t gid);
    virtual ~tmpfs_inode() override;
    void release() override;

    nt_status open_child(string_view name, object_ref& obj) override;
    nt_status create_child(string_view name, uint32_t create_options,
                           optional<reference_wrapper<inode>>& obj, bool socket) override;
    NTSTATUS query_directory(span<uint8_t> buf, FILE_INFORMATION_CLASS file_information_class,
                             uint64_t& index) override;
    NTSTATUS set_information(span<const uint8_t> buffer, dentry& dentry,
                             FILE_INFORMATION_CLASS file_information_class) override;
    NTSTATUS query_information(span<uint8_t> buffer,
                               FILE_INFORMATION_CLASS file_information_class,
                               uint64_t& retlen) override;
    nt_status query_directory_inode(string_view name, span<uint8_t> buf,
                                    FILE_INFORMATION_CLASS file_information_class) override;
    nt_status rename(::dentry& dentry, ::dentry& dest_dir, string_view name,
                     bool replace_if_exists, object_ref& victim) override;
    nt_status hardlink(::dentry& dentry, ::dentry& dest_dir, string_view name,
                       bool replace_if_exists, object_ref& victim) override;
    nt_status read_into_cache(uint64_t page_num, uint64_t num_pages,
                              uint64_t& read_pages) override;
    nt_status flush_cache(uint64_t page_num, uint64_t num_pages) override;
    nt_status set_basic_information(span<const uint8_t> buffer);
    nt_status query_basic_information(span<uint8_t> buffer, uint64_t& retlen);
    nt_status query_standard_information(span<uint8_t> buffer, uint64_t& retlen);
    nt_status query_internal_information(span<uint8_t> buffer, uint64_t& retlen);
    nt_status query_lx_information(span<uint8_t> buffer, uint64_t& retlen);
    nt_status set_disposition_information(span<const uint8_t> buffer,
                                          dentry& dentry);
    nt_status fs_control(uint32_t code, span<const uint8_t> inbuf,
                         span<uint8_t> outbuf, uint32_t& retlen,
                         dentry* dentry) override;

    tmpfs_filesystem& fs; // FIXME - use reference in ::inode
    uint64_t otime, atime;
    unsigned int nlink = 1;
    spinlock child_lock;
    list_entry children = { &children, &children };
    unique_ptr<char[]> symlink;
    size_t symlink_len;
    uint64_t inode;
    uint32_t mode, uid, gid;

private:
    nt_status get_reparse_point(span<uint8_t> outbuf, uint32_t& retlen);
    nt_status set_reparse_point(span<const uint8_t> inbuf);
    nt_status set_reparse_point_symlink(span<const uint8_t> inbuf);
    nt_status set_reparse_point_fifo();
    nt_status do_fchown(span<const uint8_t> buffer);
    nt_status do_fchmod(span<const uint8_t> buffer);
};

struct tmpfs_dentry {
    list_entry le;
    tmpfs_inode* ino;
    unsigned int index;
    size_t name_len;
    char name[0];
};

class tmpfs_filesystem : public ::filesystem {
public:
    tmpfs_filesystem();

    nt_status flush(bool make_readonly) override;
    nt_status query_volume(FS_INFORMATION_CLASS fs_information_class,
                           span<uint8_t> buf, uint32_t& retlen) override;

    bool readonly = false;

private:
    nt_status query_volume_size_info(span<uint8_t> buf, uint32_t& retlen);
    nt_status query_volume_attribute_info(span<uint8_t> buf, uint32_t& retlen);
};

tmpfs_filesystem::tmpfs_filesystem() : filesystem(nullptr) {
    auto tmpfs_root_inode = new tmpfs_inode(*this, inode_type::directory, S_IFDIR | 0755,
                                            0, 0);
    if (!tmpfs_root_inode) {
        print("new failed\n");
        return;
    }

    root_inode = tmpfs_root_inode;

    auto cur_time = (uint64_t)kernel_time_page.system_time + kernel_time_page.system_time_offset;

    tmpfs_root_inode->file_size = 0;
    tmpfs_root_inode->atime = cur_time;
    tmpfs_root_inode->ctime = cur_time;
    tmpfs_root_inode->mtime = cur_time;
    tmpfs_root_inode->otime = cur_time;
}

static ::filesystem* create_tmpfs(uint32_t options);

static filesystem_recognizer mod_filesystem_type = {
    {}, "tmpfs", nullptr, create_tmpfs
};

static void init_tmpfs() {
    insert_tail_list(filesystem_types, mod_filesystem_type.le);
}

static module_init module_init_btrfs(init_tmpfs);

static ::filesystem* create_tmpfs(uint32_t options) {
    if (options & MOXNOX_VFSCTL_MOUNT_READONLY) {
        print("cannot create readonly tmpfs\n");
        return nullptr;
    } else if (options != 0) {
        print("create_tmpfs: unknown option bits {}\n", options);
        return nullptr;
    }

    auto fs = new tmpfs_filesystem();
    if (!fs)
        return nullptr;

    return fs;
}

// FIXME - make this global?
static bool validate_name(string_view name) {
    if (name.empty())
        return false;

    if (name.size() > NAME_MAX_LEN)
        return false;

    if (name == "." || name == "..")
        return false;

    for (auto c : name) {
        if (c == '/' || c == '\\' || c == 0)
            return false;
    }

    // FIXME - check is valid UTF-8?

    return true;
}

nt_status tmpfs_inode::create_child(string_view name, uint32_t create_options,
                                    optional<reference_wrapper<::inode>>& obj,
                                    bool socket) {
    if (fs.readonly)
        return nt_status::MEDIA_WRITE_PROTECTED;

    if (!validate_name(name))
        return nt_status::OBJECT_NAME_INVALID;

    ::inode_type type;
    uint32_t mode;

    if (socket) {
        type = inode_type::socket;
        mode = S_IFSOCK | 0777;
    } else if (create_options & FILE_DIRECTORY_FILE) {
        type = inode_type::directory;
        mode = S_IFDIR | 0755;
    } else {
        type = inode_type::file;
        mode = S_IFREG | 0644;
    }

    auto new_ino = new tmpfs_inode(static_cast<tmpfs_filesystem&>(fs), type,
                                   mode, 0, 0);
    if (!new_ino)
        return nt_status::INSUFFICIENT_RESOURCES;

    auto d = (tmpfs_dentry*)malloc(offsetof(tmpfs_dentry, name) + name.size());
    if (!d) {
        print("malloc failed\n");
        new_ino->release();
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    auto cur_time = (uint64_t)kernel_time_page.system_time + kernel_time_page.system_time_offset;

    new_ino->file_size = 0;
    new_ino->atime = cur_time;
    new_ino->ctime = cur_time;
    new_ino->mtime = cur_time;
    new_ino->otime = cur_time;

    d->ino = new_ino;
    d->name_len = name.size();
    memcpy(d->name, name.data(), name.size());

    new_ino->acquire();

    {
        lock_guard lock(child_lock);

        if (is_list_empty(children))
            d->index = 2;
        else
            d->index = containing_record<tmpfs_dentry>(children.prev).index + 1;

        insert_tail_list(children, d->le);
    }

    obj = *new_ino;

    return nt_status::SUCCESS;
}

nt_status tmpfs_inode::open_child(string_view name, object_ref& obj_ref) {
    if (inode_type != inode_type::directory)
        return nt_status::INVALID_PARAMETER;

    tmpfs_inode* ino = nullptr;

    {
        lock_guard lg(child_lock);

        for (auto le = children.next; le != &children; le = le->next) {
            auto& d = containing_record<tmpfs_dentry>(le);
            auto sv = string_view(d.name, d.name_len);

            if (sv == name) {
                d.ino->acquire();
                ino = d.ino;
                break;
            }
        }
    }

    if (!ino)
        return nt_status::OBJECT_NAME_NOT_FOUND;

    obj_ref = *ino;

    return nt_status::SUCCESS;
}

nt_status tmpfs_filesystem::flush(bool make_readonly) {
    if (make_readonly)
        readonly = true;

    return nt_status::SUCCESS;
}

void tmpfs_inode::release() {
    if (--refcount != 0)
        return;

    delete this;
}

tmpfs_inode::tmpfs_inode(tmpfs_filesystem& fs, enum inode_type inode_type, uint32_t mode,
                         uint32_t uid, uint32_t gid) : ::inode(fs), fs(fs), mode(mode), uid(uid), gid(gid) {
    this->inode_type = inode_type;
    this->inode = last_inode_number.fetch_add(1) + 1;
}

tmpfs_inode::~tmpfs_inode() {
    while (true) {
        tmpfs_dentry* d;

        {
            lock_guard lock(child_lock);

            if (is_list_empty(children))
                break;

            d = &containing_record<tmpfs_dentry>(children.next);
            remove_entry_list(d->le);
        }

        d->ino->release();
        free(d);
    }
}

static nt_status query_directory_dirent(string_view name, tmpfs_inode& ino,
                                        span<uint8_t> buf,
                                        FILE_INFORMATION_CLASS file_information_class) {
    auto func = []<typename T>(string_view name, tmpfs_inode& ino,
                               span<uint8_t> buf) -> nt_status {
        if (buf.size() < offsetof(T, FileName))
            return nt_status::BUFFER_TOO_SMALL;

        auto utf16len = utf8_to_utf16_len(name);
        auto len = offsetof(T, FileName) + (utf16len * sizeof(char16_t));

        if (buf.size() < len)
            return nt_status::BUFFER_TOO_SMALL;

        auto& fdi = *(T*)buf.data();

        {
            shared_lock lock(ino.inode_lock);

            fdi.NextEntryOffset = 0;
            fdi.FileIndex = 0;
            fdi.CreationTime.QuadPart = (int64_t)ino.otime;
            fdi.LastAccessTime.QuadPart = (int64_t)ino.atime;
            fdi.LastWriteTime.QuadPart = (int64_t)ino.mtime;
            fdi.ChangeTime.QuadPart = (int64_t)ino.ctime;
            fdi.EndOfFile.QuadPart = (int64_t)ino.file_size;
            fdi.AllocationSize.QuadPart = 0; // FIXME

            switch (ino.inode_type) {
                case inode_type::directory:
                    fdi.FileAttributes = FILE_ATTRIBUTE_DIRECTORY;
                    break;

                case inode_type::symlink:
                    fdi.FileAttributes = FILE_ATTRIBUTE_REPARSE_POINT;
                    break;

                default:
                    fdi.FileAttributes = 0;
                    break;
            }

            if constexpr (requires { T::FileId; })
                fdi.FileId.QuadPart = ino.inode;
        }

        // FIXME - EaSize should contain reparse tag code?

        if constexpr (requires { T::EaSize; })
            fdi.EaSize = 0;

        fdi.FileNameLength = (uint32_t)(utf16len * sizeof(char16_t));
        utf8_to_utf16(span(fdi.FileName, utf16len), name);

        buf = buf.subspan(0, len);

        return nt_status::SUCCESS;
    };

    switch (file_information_class) {
        using enum FILE_INFORMATION_CLASS;

        case DirectoryInformation:
            return func.template operator()<FILE_DIRECTORY_INFORMATION>(name, ino, buf);

        case IdFullDirectoryInformation:
            return func.template operator()<FILE_ID_FULL_DIR_INFORMATION>(name, ino, buf);

        default:
            print("tmpfs query_directory_dirent: unhandled info class {}\n",
                  file_information_class);
            return nt_status::INVALID_INFO_CLASS;
    }
}

NTSTATUS tmpfs_inode::query_directory(span<uint8_t> buf,
                                      FILE_INFORMATION_CLASS file_information_class,
                                      uint64_t& index)  {
    if (inode_type != inode_type::directory)
        return nt_status::NOT_A_DIRECTORY;

    {
        lock_guard lock(child_lock);

        for (auto le = children.next; le != &children; le = le->next) {
            auto& d = containing_record<tmpfs_dentry>(le);

            if (d.index < index)
                continue;

            // FIXME - what if name changes while we're doing this?

            auto ret = query_directory_dirent(string_view(d.name, d.name_len),
                                              *d.ino, buf,
                                              file_information_class);

            index = d.index + 1;

            return ret;
        }
    }

    return nt_status::NO_MORE_FILES;
}

nt_status tmpfs_inode::query_directory_inode(string_view name, span<uint8_t> buf,
                                             FILE_INFORMATION_CLASS file_information_class) {
    return query_directory_dirent(name, *this, buf, file_information_class);
}

nt_status tmpfs_inode::read_into_cache(uint64_t page_num, uint64_t num_pages,
                                       uint64_t& read_pages) {
    page_cache_entry* ent;

    for (uint64_t i = 0; i < num_pages; i++) {
        lock_guard lg(cache.base.cache_lock);

        if (i == 0)
            ent = cache.find_page_cache_entry(page_num);
        else
            ent = cache.next_page_cache_entry(*ent);

        assert(ent->tree.key == page_num + i);

        if (!ent->pfn) {
            increase_page_refcount(zero_pfn);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
            ent->pfn = zero_pfn;
#pragma GCC diagnostic pop
        }
    }

    read_pages = num_pages;
    return nt_status::SUCCESS;
}

nt_status tmpfs_inode::flush_cache(uint64_t, uint64_t) {
    return nt_status::SUCCESS;
}

nt_status tmpfs_inode::do_fchown(span<const uint8_t> buffer) {
    if (buffer.size() < sizeof(MN_FCHOWN))
        return nt_status::BUFFER_TOO_SMALL;

    auto& params = *(MN_FCHOWN*)buffer.data();

    auto cur_time = (uint64_t)kernel_time_page.system_time + kernel_time_page.system_time_offset;

    {
        lock_guard lock(inode_lock);

        if (params.uid != 0xffffffff)
            uid = params.uid;

        if (params.gid != 0xffffffff)
            gid = params.gid;

        ctime = cur_time;

        if (!S_ISDIR(mode)) {
            mode &= ~S_ISUID;

            if (mode & S_IXGRP)
                mode &= ~S_ISGID;
        }
    }

    return nt_status::SUCCESS;
}

nt_status tmpfs_inode::do_fchmod(span<const uint8_t> buffer) {
    static const uint32_t MASK = 0xfff;

    if (buffer.size() < sizeof(uint32_t))
        return nt_status::BUFFER_TOO_SMALL;

    auto v = *(uint32_t*)buffer.data();

    v &= MASK;

    auto cur_time = (uint64_t)kernel_time_page.system_time + kernel_time_page.system_time_offset;

    {
        lock_guard lock(inode_lock);

        mode &= ~MASK;
        mode |= v;

        ctime = cur_time;
    }

    return nt_status::SUCCESS;
}

NTSTATUS tmpfs_inode::set_information(span<const uint8_t> buffer, dentry& dentry,
                                      FILE_INFORMATION_CLASS file_information_class) {
    if (static_cast<tmpfs_filesystem&>(fs).readonly)
        return nt_status::MEDIA_WRITE_PROTECTED;

    switch (file_information_class) {
        using enum FILE_INFORMATION_CLASS;

        case BasicInformation:
            return set_basic_information(buffer);

        case DispositionInformation:
            return set_disposition_information(buffer, dentry);

        case MoxnoxFchown:
            return do_fchown(buffer);

        case MoxnoxFchmod:
            return do_fchmod(buffer);

        default:
            print("tmpfs_inode::set_information: unhandled file information class {}\n",
                  file_information_class);
            return nt_status::NOT_IMPLEMENTED;
    }
}

nt_status tmpfs_inode::set_basic_information(span<const uint8_t> buffer) {
    if (buffer.size() < sizeof(FILE_BASIC_INFORMATION))
        return nt_status::INVALID_PARAMETER;

    const auto& fbi = *(FILE_BASIC_INFORMATION*)buffer.data();

    // FIXME - setting times to -1 or -2
    // FIXME - subsequent writes etc. shouldn't clobber set time

    if (fbi.CreationTime.QuadPart != 0)
        otime = (uint64_t)fbi.CreationTime.QuadPart;

    if (fbi.LastAccessTime.QuadPart != 0)
        atime = (uint64_t)fbi.LastAccessTime.QuadPart;

    if (fbi.LastWriteTime.QuadPart != 0)
        mtime = (uint64_t)fbi.LastWriteTime.QuadPart;

    if (fbi.ChangeTime.QuadPart != 0)
        ctime = (uint64_t)fbi.ChangeTime.QuadPart;

    // FIXME - FileAttributes

    return nt_status::SUCCESS;
}

nt_status tmpfs_inode::query_basic_information(span<uint8_t> buffer,
                                               uint64_t& retlen) {
    if (buffer.size() < sizeof(FILE_BASIC_INFORMATION))
        return nt_status::BUFFER_TOO_SMALL;

    auto& fbi = *(FILE_BASIC_INFORMATION*)buffer.data();

    {
        shared_lock llock(inode_lock);

        fbi.CreationTime.QuadPart = (int64_t)otime;
        fbi.LastAccessTime.QuadPart = (int64_t)atime;
        fbi.LastWriteTime.QuadPart = (int64_t)mtime;
        fbi.ChangeTime.QuadPart = (int64_t)ctime;

        switch (inode_type) {
            case inode_type::directory:
                fbi.FileAttributes = FILE_ATTRIBUTE_DIRECTORY;
            break;

            case inode_type::symlink:
                fbi.FileAttributes = FILE_ATTRIBUTE_REPARSE_POINT;
            break;

            default:
                fbi.FileAttributes = 0;
            break;
        }
    }

    retlen = sizeof(fbi);

    return nt_status::SUCCESS;
}

nt_status tmpfs_inode::query_standard_information(span<uint8_t> buffer,
                                                  uint64_t& retlen) {
    if (buffer.size() < sizeof(FILE_STANDARD_INFORMATION))
        return nt_status::BUFFER_TOO_SMALL;

    auto& fsi = *(FILE_STANDARD_INFORMATION*)buffer.data();

    {
        shared_lock lock(inode_lock);

        fsi.AllocationSize.QuadPart = 0; // FIXME
        fsi.EndOfFile.QuadPart = (int64_t)file_size;
        fsi.NumberOfLinks = nlink;
        fsi.DeletePending = 0; // FIXME
        fsi.Directory = inode_type == inode_type::directory ? 1 : 0;
    }

    retlen = sizeof(fsi);

    return nt_status::SUCCESS;
}

nt_status tmpfs_inode::query_internal_information(span<uint8_t> buffer,
                                                  uint64_t& retlen) {
    if (buffer.size() < sizeof(FILE_INTERNAL_INFORMATION))
        return nt_status::BUFFER_TOO_SMALL;

    auto& fii = *(FILE_INTERNAL_INFORMATION*)buffer.data();

    fii.IndexNumber.QuadPart = (int64_t)inode;

    retlen = sizeof(fii);

    return nt_status::SUCCESS;
}

NTSTATUS tmpfs_inode::query_information(span<uint8_t> buffer,
                                        FILE_INFORMATION_CLASS file_information_class,
                                        uint64_t& retlen) {
    switch (file_information_class) {
        using enum FILE_INFORMATION_CLASS;

        case BasicInformation:
            return query_basic_information(buffer, retlen);

        case StandardInformation:
            return query_standard_information(buffer, retlen);

        case InternalInformation:
            return query_internal_information(buffer, retlen);

        case StatLxInformation:
            return query_lx_information(buffer, retlen);

        default:
            print("tmpfs_inode::query_information: unhandled file information class {}\n",
                  file_information_class);
            return nt_status::NOT_IMPLEMENTED;
    }
}

nt_status tmpfs_inode::set_disposition_information(span<const uint8_t> buffer,
                                                   dentry& dentry) {
    if (buffer.size() < sizeof(FILE_DISPOSITION_INFORMATION))
        return nt_status::INVALID_PARAMETER;

    if (dentry.inode == fs.root_inode)
        return nt_status::INVALID_PARAMETER;

    const auto& fdi = *(FILE_DISPOSITION_INFORMATION*)buffer.data();

    // FIXME - this should actually be a non-POSIX deletion

    if (!fdi.DeleteFile) // FIXME
        return nt_status::NOT_SUPPORTED;

    // FIXME - make sure we can't move or rename file at the same time

    char* name_buf;
    size_t name_len;

    while (true) {
        {
            lock_guard lock(dentry.lock);
            name_len = dentry.name_len;
        }

        assert(name_len != 0);

        name_buf = (char*)malloc(name_len);
        if (!name_buf)
            return nt_status::INSUFFICIENT_RESOURCES;

        bool done = false;

        {
            lock_guard lock(dentry.lock);

            if (dentry.name_len == name_len) {
                memcpy(name_buf, dentry.name, name_len);
                done = true;
            }
        }

        if (done)
            break;

        free(name_buf);
    }

    auto name = string_view(name_buf, name_len);

    {
        auto& p = static_cast<tmpfs_inode&>(*dentry.parent->inode);

        bool done = false;

        {
            lock_guard lock(p.child_lock);

            if (inode_type == inode_type::directory) {
                lock_guard lock(child_lock);

                if (!is_list_empty(children))
                    return nt_status::DIRECTORY_NOT_EMPTY;
            }

            for (auto le = p.children.next; le != &p.children; le = le->next) {
                auto& d = containing_record<tmpfs_dentry>(le);

                if (string_view(d.name, d.name_len) == name) {
                    remove_entry_list(d.le);
                    free(&d);
                    done = true;
                    break;
                }
            }
        }

        if (!done) {
            print("tmpfs_inode::set_disposition_information: tmpfs_dentry not found\n");
            free(name_buf);
            return nt_status::INTERNAL_ERROR;
        }
    }

    free(name_buf);

    // invalidate dentry

    {
        lock_guard lg(dentry.parent->child_lock);
        remove_entry_list(dentry.le);
    }

    {
        lock_guard lg(dentry.lock);
        dentry.status = dentry_status::invalid;
    }

    {
        lock_guard lg(inode_lock);
        nlink--;
    }

    dentry.release();
    this->release();

    return nt_status::SUCCESS;
}

nt_status tmpfs_inode::rename(::dentry& dentry, ::dentry& dest_dir,
                              string_view name, bool replace_if_exists,
                              object_ref& victim) {
    nt_status status;

    assert(dentry.parent);
    assert(dentry.parent->inode);

    if (!validate_name(name))
        return nt_status::OBJECT_NAME_INVALID;

    if (fs.readonly)
        return nt_status::MEDIA_WRITE_PROTECTED;

    auto& src_parent_ino = static_cast<tmpfs_inode&>(*dentry.parent->inode);
    auto& dst_parent_ino = static_cast<tmpfs_inode&>(*dest_dir.inode);
    auto cur_name = string_view(dentry.name, dentry.name_len);

    // early return if trying to replace file with itself
    // FIXME - what does NT do here? Does it depend on replace_if_exists?
    if (&src_parent_ino == &dst_parent_ino && name == cur_name)
        return nt_status::SUCCESS;

    // FIXME - lock inode to prevent simultaneous renames?

    bool done_overwrite = false;
    auto cur_time = (uint64_t)kernel_time_page.system_time + kernel_time_page.system_time_offset;

    // FIXME - overwrites

    status = dst_parent_ino.open_child(name, victim);
    if (!success(status) && status != nt_status::OBJECT_NAME_NOT_FOUND) {
        print("tmpfs_inode::open_child returned {}\n", status);
        return status;
    }

    if (success(status)) {
        if (!replace_if_exists)
            return nt_status::OBJECT_NAME_COLLISION;

        assert(victim.get().type == object_type::inode);

        auto& victim_ino = static_cast<tmpfs_inode&>(victim.get());

        if (&victim_ino == fs.root_inode)
            return nt_status::ACCESS_DENIED;

        {
            lock_guard lg2(victim_ino.inode_lock);

            if (victim_ino.inode_type == inode_type::directory) {
                if (inode_type != inode_type::directory)
                    return nt_status::ACCESS_DENIED;

                if (victim_ino.file_size != 0)
                    return nt_status::DIRECTORY_NOT_EMPTY;
            } else {
                if (inode_type == inode_type::directory)
                    return nt_status::ACCESS_DENIED;
            }

            victim_ino.nlink--;
            victim_ino.ctime = cur_time;

            // pretend we have an open handle
            victim_ino.handle_count++;
        }

        {
            bool done = false;

            {
                lock_guard lock(dst_parent_ino.child_lock);

                if (victim_ino.inode_type == inode_type::directory) {
                    lock_guard lock(victim_ino.child_lock);

                    if (!is_list_empty(victim_ino.children))
                        return nt_status::DIRECTORY_NOT_EMPTY;
                }

                for (auto le = dst_parent_ino.children.next; le != &dst_parent_ino.children; le = le->next) {
                    auto& d2 = containing_record<tmpfs_dentry>(le);

                    if (string_view(d2.name, d2.name_len) == name) {
                        remove_entry_list(d2.le);
                        free(&d2);
                        done = true;
                        break;
                    }
                }
            }

            if (!done) {
                print("tmpfs_inode::rename: victim tmpfs_dentry not found\n");
                return nt_status::INTERNAL_ERROR;
            }

            victim_ino.release();
        }

        done_overwrite = true;
    }

    // add new tmpfs dentry

    auto d = (tmpfs_dentry*)malloc(offsetof(tmpfs_dentry, name) + name.size());
    if (!d) {
        print("malloc failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    d->ino = this;
    d->name_len = name.size();
    memcpy(d->name, name.data(), name.size());

    acquire();

    {
        lock_guard lock(dst_parent_ino.child_lock);

        if (is_list_empty(dst_parent_ino.children))
            d->index = 0;
        else
            d->index = containing_record<tmpfs_dentry>(dst_parent_ino.children.prev).index + 1;

        insert_tail_list(dst_parent_ino.children, d->le);
    }

    // remove old tmpfs dentry

    {
        bool done = false;

        {
            lock_guard lock(src_parent_ino.child_lock);

            if (inode_type == inode_type::directory) {
                lock_guard lock(child_lock);

                if (!is_list_empty(children))
                    return nt_status::DIRECTORY_NOT_EMPTY;
            }

            for (auto le = src_parent_ino.children.next; le != &src_parent_ino.children; le = le->next) {
                auto& d2 = containing_record<tmpfs_dentry>(le);

                if (string_view(d2.name, d2.name_len) == cur_name) {
                    remove_entry_list(d2.le);
                    free(&d2);
                    done = true;
                    break;
                }
            }
        }

        if (!done) {
            print("tmpfs_inode::rename: existing tmpfs_dentry not found\n");
            return nt_status::INTERNAL_ERROR;
        }

        release();
    }

    // update inode

    {
        lock_guard lg(inode_lock);

        ctime = cur_time;
    }

    // adjust size etc. of parent directories

    {
        lock_guard lg(src_parent_ino.inode_lock);

        // FIXME - size munging needs to be done in same lock as checking victim_ino size?

        if (&src_parent_ino == &dst_parent_ino && !done_overwrite)
            src_parent_ino.file_size += name.size() * 2;

        src_parent_ino.file_size -= cur_name.size() * 2;
        src_parent_ino.ctime = cur_time;
        src_parent_ino.mtime = cur_time;
    }

    if (&src_parent_ino != &dst_parent_ino) {
        lock_guard lg(dst_parent_ino.inode_lock);

        if (!done_overwrite)
            dst_parent_ino.file_size += name.size() * 2;

        dst_parent_ino.ctime = cur_time;
        dst_parent_ino.mtime = cur_time;
    }

    return nt_status::SUCCESS;
}

nt_status tmpfs_inode::fs_control(uint32_t code, span<const uint8_t> inbuf,
                                  span<uint8_t> outbuf, uint32_t& retlen,
                                  dentry* /*dentry*/) {
    switch (code) {
        case FSCTL_GET_REPARSE_POINT:
            return get_reparse_point(outbuf, retlen);

        case FSCTL_SET_REPARSE_POINT:
            return set_reparse_point(inbuf);

        default:
            return nt_status::NOT_SUPPORTED;
    }
}

nt_status tmpfs_inode::get_reparse_point(span<uint8_t> outbuf, uint32_t& retlen) {
    if (outbuf.size() < offsetof(REPARSE_DATA_BUFFER, SymbolicLinkReparseBuffer.PathBuffer))
        return nt_status::BUFFER_TOO_SMALL;

    auto& rdb = *(REPARSE_DATA_BUFFER*)outbuf.data();
    size_t u16len, rdblen;

    {
        lock_guard lg(inode_lock);

        if (inode_type != inode_type::symlink)
            return nt_status::NOT_A_REPARSE_POINT;

        if (!symlink)
            return nt_status::INTERNAL_ERROR;

        auto sv = string_view(symlink.get(), symlink_len);

        u16len = utf8_to_utf16_len(sv);

        rdblen = offsetof(REPARSE_DATA_BUFFER, SymbolicLinkReparseBuffer.PathBuffer) + (u16len * sizeof(char16_t));

        if (outbuf.size() < rdblen)
            return nt_status::BUFFER_TOO_SMALL;

        utf8_to_utf16(span(rdb.SymbolicLinkReparseBuffer.PathBuffer, u16len), sv);
    }

    rdb.ReparseTag = IO_REPARSE_TAG_SYMLINK;
    rdb.ReparseDataLength = (uint16_t)(rdblen - offsetof(REPARSE_DATA_BUFFER, SymbolicLinkReparseBuffer));
    rdb.Reserved = 0;

    rdb.SymbolicLinkReparseBuffer.SubstituteNameOffset = 0;
    rdb.SymbolicLinkReparseBuffer.SubstituteNameLength = (uint16_t)(u16len * sizeof(char16_t));
    rdb.SymbolicLinkReparseBuffer.PrintNameOffset = 0; // FIXME?
    rdb.SymbolicLinkReparseBuffer.PrintNameLength = 0; // FIXME?
    rdb.SymbolicLinkReparseBuffer.Flags = 0; // FIXME?

    retlen = (uint32_t)rdblen;

    return nt_status::SUCCESS;
}

nt_status tmpfs_inode::set_reparse_point_symlink(span<const uint8_t> inbuf) {
    unique_ptr<char[]> u8;

    if (inbuf.size() < offsetof(REPARSE_DATA_BUFFER, SymbolicLinkReparseBuffer.PathBuffer))
        return nt_status::BUFFER_TOO_SMALL;

    auto& rdb = *(REPARSE_DATA_BUFFER*)inbuf.data();

    size_t buflen = inbuf.size() - offsetof(REPARSE_DATA_BUFFER, SymbolicLinkReparseBuffer.PathBuffer);

    if (rdb.SymbolicLinkReparseBuffer.SubstituteNameLength == 0)
        return nt_status::INVALID_PARAMETER;

    if (rdb.SymbolicLinkReparseBuffer.SubstituteNameOffset >= buflen)
        return nt_status::INVALID_PARAMETER;

    if (rdb.SymbolicLinkReparseBuffer.SubstituteNameOffset + rdb.SymbolicLinkReparseBuffer.SubstituteNameLength > buflen)
        return nt_status::INVALID_PARAMETER;

    auto u16sv = u16string_view(rdb.SymbolicLinkReparseBuffer.PathBuffer + (rdb.SymbolicLinkReparseBuffer.SubstituteNameOffset / sizeof(char16_t)),
                                rdb.SymbolicLinkReparseBuffer.SubstituteNameLength / sizeof(char16_t));

    auto u8len = utf16_to_utf8_len(u16sv);

    u8.reset(new char[u8len]);
    if (!u8) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    utf16_to_utf8(span(u8.get(), u8len), u16sv);

    {
        lock_guard lock(inode_lock);

        if (inode_type != inode_type::file)
            return nt_status::INVALID_PARAMETER;

        inode_type = inode_type::symlink;
        mode = S_IFLNK | 0777;

        swap(symlink, u8);
        symlink_len = u8len;
    }

    return nt_status::SUCCESS;
}

nt_status tmpfs_inode::set_reparse_point_fifo() {
    {
        lock_guard lock(inode_lock);

        if (file_size != 0)
            return nt_status::INVALID_PARAMETER;

        if (nlink != 1)
            return nt_status::INVALID_PARAMETER;

        if (inode_type != inode_type::file)
            return nt_status::INVALID_PARAMETER;

        inode_type = inode_type::fifo;
        mode = (mode & ~S_IFREG) | S_IFIFO;
    }

    return nt_status::SUCCESS;
}

nt_status tmpfs_inode::set_reparse_point(span<const uint8_t> inbuf) {
    if (inbuf.size() < offsetof(REPARSE_DATA_BUFFER, GenericReparseBuffer.DataBuffer))
        return nt_status::BUFFER_TOO_SMALL;

    auto& rdb = *(REPARSE_DATA_BUFFER*)inbuf.data();

    switch (rdb.ReparseTag) {
        case IO_REPARSE_TAG_SYMLINK:
           return set_reparse_point_symlink(inbuf);

        case IO_REPARSE_TAG_LX_FIFO:
            return set_reparse_point_fifo();

        default:
            return nt_status::INVALID_PARAMETER;
    }
}

nt_status tmpfs_inode::hardlink(::dentry& dentry, ::dentry& dest_dir,
                                string_view name, bool replace_if_exists,
                                object_ref& victim) {
    nt_status status;

    assert(dentry.parent);
    assert(dentry.parent->inode);

    if (!validate_name(name))
        return nt_status::OBJECT_NAME_INVALID;

    if (fs.readonly)
        return nt_status::MEDIA_WRITE_PROTECTED;

    auto& src_parent_ino = static_cast<tmpfs_inode&>(*dentry.parent->inode);
    auto& dst_parent_ino = static_cast<tmpfs_inode&>(*dest_dir.inode);
    auto cur_name = string_view(dentry.name, dentry.name_len);

    // early return if trying to replace file with itself
    // FIXME - what does NT do here? Does it depend on replace_if_exists?
    if (&src_parent_ino == &dst_parent_ino && name == cur_name)
        return nt_status::SUCCESS;

    // FIXME - lock inode to prevent simultaneous renames?

    bool done_overwrite = false;
    auto cur_time = (uint64_t)kernel_time_page.system_time + kernel_time_page.system_time_offset;

    status = dst_parent_ino.open_child(name, victim);
    if (!success(status) && status != nt_status::OBJECT_NAME_NOT_FOUND) {
        print("tmpfs_inode::open_child returned {}\n", status);
        return status;
    }

    if (success(status)) {
        if (!replace_if_exists)
            return nt_status::OBJECT_NAME_COLLISION;

        assert(victim.get().type == object_type::inode);

        auto& victim_ino = static_cast<tmpfs_inode&>(victim.get());

        {
            lock_guard lg2(victim_ino.inode_lock);

            if (victim_ino.inode_type == inode_type::directory)
                return nt_status::ACCESS_DENIED;

            victim_ino.nlink--;
            victim_ino.ctime = cur_time;

            // pretend we have an open handle
            victim_ino.handle_count++;
        }

        {
            bool done = false;

            {
                lock_guard lock(dst_parent_ino.child_lock);

                if (victim_ino.inode_type == inode_type::directory) {
                    lock_guard lock(victim_ino.child_lock);

                    if (!is_list_empty(victim_ino.children))
                        return nt_status::DIRECTORY_NOT_EMPTY;
                }

                for (auto le = dst_parent_ino.children.next; le != &dst_parent_ino.children; le = le->next) {
                    auto& d2 = containing_record<tmpfs_dentry>(le);

                    if (string_view(d2.name, d2.name_len) == name) {
                        remove_entry_list(d2.le);
                        free(&d2);
                        done = true;
                        break;
                    }
                }
            }

            if (!done) {
                print("tmpfs_inode::hardlink: victim tmpfs_dentry not found\n");
                return nt_status::INTERNAL_ERROR;
            }

            victim_ino.release();
        }

        done_overwrite = true;
    }

    // add new tmpfs dentry

    auto d = (tmpfs_dentry*)malloc(offsetof(tmpfs_dentry, name) + name.size());
    if (!d) {
        print("malloc failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    d->ino = this;
    d->name_len = name.size();
    memcpy(d->name, name.data(), name.size());

    acquire();

    {
        lock_guard lock(dst_parent_ino.child_lock);

        if (is_list_empty(dst_parent_ino.children))
            d->index = 0;
        else
            d->index = containing_record<tmpfs_dentry>(dst_parent_ino.children.prev).index + 1;

        insert_tail_list(dst_parent_ino.children, d->le);
    }

    // update inode

    {
        lock_guard lg(inode_lock);

        ctime = cur_time;
        nlink++;
    }

    // adjust size etc. of parent directory

    {
        lock_guard lg(dst_parent_ino.inode_lock);

        if (!done_overwrite)
            dst_parent_ino.file_size += name.size() * 2;

        dst_parent_ino.ctime = cur_time;
        dst_parent_ino.mtime = cur_time;
    }

    return nt_status::SUCCESS;
}

nt_status tmpfs_filesystem::query_volume(FS_INFORMATION_CLASS fs_information_class,
                                         span<uint8_t> buf, uint32_t& retlen) {
    switch (fs_information_class) {
        using enum FS_INFORMATION_CLASS;

        case SizeInformation:
            return query_volume_size_info(buf, retlen);

        case AttributeInformation:
            return query_volume_attribute_info(buf, retlen);

        default:
            return nt_status::NOT_SUPPORTED;
    }
}

nt_status tmpfs_filesystem::query_volume_size_info(span<uint8_t> buf,
                                                   uint32_t& retlen) {
    if (buf.size() < sizeof(FILE_FS_SIZE_INFORMATION))
        return nt_status::INVALID_PARAMETER;

    auto& info = *(FILE_FS_SIZE_INFORMATION*)buf.data();

    info.TotalAllocationUnits.QuadPart = (int64_t)num_ram_pages;
    info.AvailableAllocationUnits.QuadPart = info.TotalAllocationUnits.QuadPart;
    info.SectorsPerAllocationUnit = 1;
    info.BytesPerSector = PAGE_SIZE;

    retlen = sizeof(FILE_FS_SIZE_INFORMATION);

    return nt_status::SUCCESS;
}

nt_status tmpfs_filesystem::query_volume_attribute_info(span<uint8_t> buf,
                                                        uint32_t& retlen) {
    static const u16string_view name = u"tmpfs";

    if (buf.size() < offsetof(FILE_FS_ATTRIBUTE_INFORMATION, FileSystemName))
        return nt_status::INVALID_PARAMETER;

    size_t name_len = min(name.size() * sizeof(char16_t),
                          buf.size() - offsetof(FILE_FS_ATTRIBUTE_INFORMATION, FileSystemName));

    auto& info = *(FILE_FS_ATTRIBUTE_INFORMATION*)buf.data();

    info.FileSystemAttributes = FILE_CASE_PRESERVED_NAMES | FILE_CASE_SENSITIVE_SEARCH |
                                FILE_UNICODE_ON_DISK | FILE_SUPPORTS_HARD_LINKS |
                                FILE_SUPPORTS_REPARSE_POINTS | FILE_SUPPORTS_SPARSE_FILES |
                                FILE_SUPPORTS_POSIX_UNLINK_RENAME;

    // FIXME - FILE_SUPPORTS_OPEN_BY_FILE_ID, FILE_SUPPORTS_EXTENDED_ATTRIBUTES,
    //         FILE_SUPPORTS_OBJECT_IDS, FILE_SUPPORTS_BLOCK_REFCOUNTING,
    //         FILE_PERSISTENT_ACLS, FILE_NAMED_STREAMS

    if (readonly)
        info.FileSystemAttributes |= FILE_READ_ONLY_VOLUME;

    info.MaximumComponentNameLength = NAME_MAX_LEN;
    info.FileSystemNameLength = (uint32_t)(name.size() * sizeof(char16_t));
    memcpy(info.FileSystemName, name.data(), name_len);

    retlen = (uint32_t)(offsetof(FILE_FS_ATTRIBUTE_INFORMATION, FileSystemName) + name_len);

    if (name_len < name.size() * sizeof(char16_t))
        return nt_status::BUFFER_OVERFLOW;

    return nt_status::SUCCESS;
}

nt_status tmpfs_inode::query_lx_information(span<uint8_t> buffer, uint64_t& retlen) {
    if (buffer.size() < sizeof(FILE_STAT_LX_INFORMATION))
        return nt_status::BUFFER_TOO_SMALL;

    auto& fsli = *(FILE_STAT_LX_INFORMATION*)buffer.data();

    {
        shared_lock lg(inode_lock);

        fsli.FileId.QuadPart = (int64_t)inode;
        fsli.CreationTime.QuadPart = (int64_t)otime;
        fsli.LastAccessTime.QuadPart = (int64_t)atime;
        fsli.LastWriteTime.QuadPart = (int64_t)mtime;
        fsli.ChangeTime.QuadPart = (int64_t)ctime;
        fsli.AllocationSize.QuadPart = 0; // FIXME
        fsli.EndOfFile.QuadPart = (int64_t)file_size;
        fsli.NumberOfLinks = nlink;
        fsli.EffectiveAccess = 0; // FIXME
        fsli.LxFlags = LX_FILE_METADATA_HAS_UID | LX_FILE_METADATA_HAS_GID |
                       LX_FILE_METADATA_HAS_MODE;
        fsli.LxUid = uid;
        fsli.LxGid = gid;
        fsli.LxMode = mode;

        fsli.LxDeviceIdMajor = 0;
        fsli.LxDeviceIdMinor = 0;

        switch (inode_type) {
            case inode_type::directory:
                fsli.FileAttributes = FILE_ATTRIBUTE_DIRECTORY;
                fsli.ReparseTag = 0;
                break;

            case inode_type::symlink:
                fsli.FileAttributes = FILE_ATTRIBUTE_REPARSE_POINT;
                fsli.ReparseTag = IO_REPARSE_TAG_SYMLINK;
                break;

            case inode_type::fifo:
                fsli.FileAttributes = FILE_ATTRIBUTE_REPARSE_POINT;
                fsli.ReparseTag = IO_REPARSE_TAG_LX_FIFO;
                break;

            default:
                fsli.FileAttributes = 0;
                fsli.ReparseTag = 0;
                break;
        }
    }

    retlen = sizeof(fsli);

    return nt_status::SUCCESS;
}
