// SPDX-License-Identifier: GPL-2.0-only
module;

#include <optional>
#include <string_view>
#include <atomic>
#include <span>
#include <stddef.h>
#include <stdint.h>

export module vfs;

import nt;
import list;
import volume;
import object_ref;
import spinlock;
import objects;
import rwsem;
import page_cache;
import format;
import mem;
import fifo;

using namespace std;

export enum class inode_type : uint8_t {
    unknown = 0,
    file = 1,
    directory = 2,
    chardev = 3,
    blockdev = 4,
    fifo = 5,
    socket = 6,
    symlink = 7,
    ea = 8
};

template<>
consteval bool format_spec_valid<inode_type>(string_view fmt) {
    return fmt.empty();
}

export span<char> do_format_arg(span<char> sp, string_view, inode_type arg) {
    switch (arg) {
        using enum inode_type;

        case unknown:
            return do_format_arg(sp, "", "unknown");
        case file:
            return do_format_arg(sp, "", "file");
        case directory:
            return do_format_arg(sp, "", "directory");
        case chardev:
            return do_format_arg(sp, "", "chardev");
        case blockdev:
            return do_format_arg(sp, "", "blockdev");
        case fifo:
            return do_format_arg(sp, "", "fifo");
        case socket:
            return do_format_arg(sp, "", "socket");
        case symlink:
            return do_format_arg(sp, "", "symlink");
        case ea:
            return do_format_arg(sp, "", "ea");
        default:
            return do_format_arg(sp, "", (unsigned int)arg);
    }
}

export struct dentry;

export class inode;

export extern "C++" class filesystem {
public:
    filesystem(volume* vol) : vol(vol) { }
    virtual ~filesystem() { }
    virtual nt_status flush(bool make_readonly) = 0;
    virtual nt_status query_volume(FS_INFORMATION_CLASS fs_information_class,
                                   span<uint8_t> buf, uint32_t& retlen) = 0;

    void acquire() {
        refcount++;
    }

    void release() {
        if (refcount.fetch_sub(1) == 1)
            delete this;
    }

    volume* vol;
    atomic<unsigned int> refcount = 1;
    inode* root_inode = nullptr;
    spinlock writeback_lock;
    list_entry writeback_list = { &writeback_list, &writeback_list };
    bool readonly = false;
};

export class inode : public rw_sync_object {
public:
    inode(filesystem& fs) : rw_sync_object(object_type::inode, true, true), fs(fs) {
        fs.acquire();
    }

    virtual ~inode();

    virtual void release() = 0;

    nt_status read(span<const span<uint8_t>> v, uint64_t offset,
                   uint64_t& read_bytes);
    nt_status read_nonblock(span<const span<uint8_t>> v, uint64_t& read_bytes);
    nt_status write(span<const span<const uint8_t>> v, uint64_t offset,
                    uint64_t& written_bytes);
    nt_status write_nonblock(span<const span<const uint8_t>> v,
                             uint64_t& written_bytes);
    nt_status set_posix_lock(uint64_t start, uint64_t length, bool write,
                             bool nowait);
    nt_status release_posix_lock(uint64_t start, uint64_t length);

    virtual nt_status open_child(string_view name, object_ref& obj) = 0;
    virtual nt_status create_child(string_view name, uint32_t create_options,
                                   optional<reference_wrapper<::inode>>& obj) = 0;
    virtual nt_status query_directory(span<uint8_t> buf, FILE_INFORMATION_CLASS file_information_class,
                                      uint64_t& index) = 0;
    virtual nt_status set_information(span<const uint8_t> buffer, dentry& dentry,
                                      FILE_INFORMATION_CLASS file_information_class) = 0;
    virtual nt_status query_information(span<uint8_t> buffer,
                                        FILE_INFORMATION_CLASS file_information_class,
                                        uint64_t& retlen) = 0;
    virtual nt_status query_directory_inode(string_view name, span<uint8_t> buf,
                                            FILE_INFORMATION_CLASS file_information_class) = 0;
    virtual nt_status rename(::dentry& dentry, ::dentry& dest_dir, string_view name,
                             bool replace_if_exists, object_ref& victim) = 0;
    virtual nt_status hardlink(::dentry& dentry, ::dentry& dest_dir, string_view name,
                               bool replace_if_exists, object_ref& victim) = 0;
    virtual nt_status fs_control(uint32_t code, span<const uint8_t> inbuf,
                                 span<uint8_t> outbuf, uint32_t& retlen,
                                 dentry* dentry) = 0;
    virtual void last_handle_closed();
    rw_sync_object& get_rw_sync_object();

    virtual nt_status read_into_cache(uint64_t page_num, uint64_t num_pages,
                                      uint64_t& read_pages) = 0;
    virtual nt_status flush_cache(uint64_t page_num, uint64_t num_pages) = 0;

    filesystem& fs;
    rwsem inode_lock;
    uint64_t file_size;
    uint64_t mtime, ctime;
    enum inode_type inode_type;
    page_cache<inode> cache{*this};
    list_entry writeback_le;
    fifo_object* fifo = nullptr;
    spinlock posix_locks_lock;
    list_entry posix_lock_procs = { &posix_lock_procs, &posix_lock_procs };
};

static_assert(is_readable_object_with_offset<inode>);
static_assert(is_writable_object_with_offset<inode>);
static_assert(is_nonblocking_readable_vec_object<inode>);
static_assert(is_nonblocking_writable_vec_object<inode>);
static_assert(cacheable_flushable<inode>);
static_assert(cache_supports_writeback<inode>);
static_assert(has_last_handle_closed<inode>);
static_assert(has_get_rw_sync_object<inode>);

export enum class dentry_status {
    valid,
    going_away,
    invalid
};

export struct dentry {
    ~dentry() {
        if (inode)
            inode->release();

        if (parent)
            parent->release();

        if (name)
            free(name);
    }

    void acquire() {
        refcount++;
    }

    void release() {
        // FIXME - clean up valid dentries with refcounts of 0

        if (--refcount != 0)
            return;

        if (status == dentry_status::invalid)
            delete this;
    }

    inode* inode = nullptr;
    list_entry le;
    atomic<unsigned int> refcount = 1;
    dentry* parent = nullptr;
    rwsem child_lock;
    list_entry children = { &children, &children };
    spinlock lock;
    enum dentry_status status = dentry_status::valid;
    atomic<unsigned int> open_file_count = 0;
    bool mount_point = false;
    size_t name_len;
    char* name = nullptr;
};

export struct filesystem_recognizer {
    list_entry le;
    string_view name;
    nt_status (*try_mount_volume)(volume& vol, uint32_t options);
    filesystem* (*mount_without_volume)(uint32_t options);
};

export class file : public object {
public:
    file(object& ino, uint32_t create_options, uint32_t access_mask);
    ~file();

    void acquire() {
        refcount++;
    }

    void release() {
        if (--refcount == 0)
            delete this;
    }

    object& ino;
    spinlock lock;
    uint64_t file_offset = 0;
    uint64_t query_dir_index = 0;
    dentry* dentry = nullptr;
    uint32_t create_options;
    uint32_t access_mask;
    atomic<bool> nonblock = false;

    NTSTATUS read(span<const span<uint8_t>> v, uint64_t& read_bytes);
    NTSTATUS read(span<const span<uint8_t>> v, uint64_t offset,
                  uint64_t& read_bytes);
    NTSTATUS write(span<const span<const uint8_t>> v, uint64_t& written_bytes);
    NTSTATUS write(span<const span<const uint8_t>> v, uint64_t offset,
                   uint64_t& written_bytes);
    nt_status ioctl(uint32_t code, span<const uint8_t> inbuf,
                    span<uint8_t> outbuf, uintptr_t& written);
    NTSTATUS query_directory(HANDLE event, PIO_APC_ROUTINE apc_routine,
                             void* apc_context, IO_STATUS_BLOCK& io_status_block,
                             void* file_information, uint32_t length,
                             FILE_INFORMATION_CLASS file_information_class, bool return_single_entry,
                             const UNICODE_STRING* file_name, bool restart_scan);
    NTSTATUS rename(span<const uint8_t> buf);
    NTSTATUS hardlink(span<const uint8_t> buf);
    NTSTATUS set_information(void* buffer, uint32_t buffer_size,
                             FILE_INFORMATION_CLASS file_information_class);
    NTSTATUS query_information(void* buffer, uint32_t buffer_size,
                               FILE_INFORMATION_CLASS file_information_class,
                               uint64_t& retlen);
    nt_status fs_control(uint32_t code, span<const uint8_t> inbuf,
                         span<uint8_t> outbuf, uint32_t& retlen);
    nt_status query_volume(FS_INFORMATION_CLASS fs_information_class,
                           span<uint8_t> buf, uint32_t& retlen);
    void last_handle_closed();

private:
    nt_status set_eof(span<const uint8_t> buf);
    nt_status set_non_block(span<const uint8_t> buffer);
    nt_status set_posix_lock(span<const uint8_t> buffer);
    nt_status release_posix_lock(span<const uint8_t> buffer);
};

static_assert(is_readable_object_without_offset<file>);
static_assert(is_readable_object_with_offset<file>);
static_assert(is_writable_object_without_offset<file>);
static_assert(is_writable_object_with_offset<file>);
static_assert(has_ioctls<file>);
static_assert(has_last_handle_closed<file>);

export list_entry filesystem_types = { &filesystem_types, &filesystem_types };
export dentry* root_dentry;

export bool mount_root();
export nt_status open_inode(string_view fn, dentry& dir,
                            object_ref& obj_ref, optional<reference_wrapper<dentry>>& dentry,
                            uint32_t create_disposition, uint32_t create_options,
                            unsigned int& create_result, bool parent);
export nt_status flush_filesystems(bool make_readonly);
export nt_status get_dentry_path(dentry& dr, char*& path, size_t& path_len);
export void search_dentry_cache(dentry& dir, string_view name,
                                optional<reference_wrapper<dentry>>& drw);
export bool init_vfsctl();
export void surprise_removal(volume& v);
export void remove_partitions(volume& v);
export nt_status vec_NtWriteFile(HANDLE file_handle, HANDLE, PIO_APC_ROUTINE, void*,
                                 IO_STATUS_BLOCK* io_status_block,
                                 span<const span<const uint8_t>> v,
                                 const LARGE_INTEGER* byte_offset,
                                 const uint32_t*);
export nt_status vec_NtReadFile(HANDLE file_handle, HANDLE, PIO_APC_ROUTINE, void*,
                                IO_STATUS_BLOCK* io_status_block,
                                span<const span<uint8_t>> v,
                                const LARGE_INTEGER* byte_offset,
                                const uint32_t*);
export void print_file_access_mask(ACCESS_MASK access_mask);
export void print_create_disposition(uint32_t create_disposition);
