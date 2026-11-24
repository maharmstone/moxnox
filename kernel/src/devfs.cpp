// SPDX-License-Identifier: GPL-2.0-only
#include <string.h>
#include <stdint.h>
#include <string_view>
#include <optional>
#include <span>
#include <mutex>
#include <shared_mutex>
#include <sys/stat.h>

import nt;
import vfs;
import list;
import print;
import kernel_time_page;
import spinlock;
import object_ref;
import handle;
import objects;
import unicode;
import module_init;
import visit;
import config;
import mem;

using namespace std;

class devfs_filesystem : public ::filesystem {
public:
    devfs_filesystem();

    nt_status flush(bool make_readonly) override;
    nt_status query_volume(FS_INFORMATION_CLASS fs_information_class,
                           span<uint8_t> buf, uint32_t& retlen) override;

    spinlock inode_lock;
    list_entry inode_list = { &inode_list, &inode_list };

private:
    nt_status query_volume_size_info(span<uint8_t> buf, uint32_t& retlen);
    nt_status query_volume_attribute_info(span<uint8_t> buf, uint32_t& retlen);
};

class devfs_inode : public inode {
public:
    devfs_inode(devfs_filesystem& fs, enum inode_type inode_type,
                uint32_t mode);
    void release() override;

    nt_status open_child(string_view name, object_ref& obj) override;
    nt_status create_child(string_view name, uint32_t create_options,
                           optional<reference_wrapper<inode>>& obj, bool socket) override;
    nt_status query_directory(span<uint8_t> buf, FILE_INFORMATION_CLASS file_information_class,
                             uint64_t& index) override;
    nt_status query_directory_inode(string_view name, span<uint8_t> buf,
                                    FILE_INFORMATION_CLASS file_information_class) override;
    nt_status set_information(span<const uint8_t> buffer, dentry& dentry,
                              FILE_INFORMATION_CLASS file_information_class) override;
    nt_status query_information(span<uint8_t> buffer,
                                FILE_INFORMATION_CLASS file_information_class,
                                uint64_t& retlen) override;
    nt_status rename(::dentry& dentry, ::dentry& dest_dir, string_view name,
                     bool replace_if_exists, object_ref& victim) override;
    nt_status hardlink(::dentry& dentry, ::dentry& dest_dir, string_view name,
                       bool replace_if_exists, object_ref& victim) override;
    nt_status read_into_cache(uint64_t page_num, uint64_t num_pages,
                              uint64_t& read_pages) override;
    nt_status flush_cache(uint64_t page_num, uint64_t num_pages) override;
    nt_status query_basic_information(span<uint8_t> buffer, uint64_t& retlen);
    nt_status query_standard_information(span<uint8_t> buffer, uint64_t& retlen);
    nt_status query_internal_information(span<uint8_t> buffer, uint64_t& retlen);
    nt_status query_lx_information(span<uint8_t> buffer, uint64_t& retlen);

    nt_status fs_control(uint32_t, span<const uint8_t>, span<uint8_t>,
                         uint32_t&, dentry*) override {
        return nt_status::NOT_SUPPORTED;
    }

    devfs_filesystem& fs; // FIXME - use reference in ::inode
    list_entry le;
    uint64_t otime, atime;
    uint32_t mode;
    uint32_t uid = 0, gid = 0;
};

static ::filesystem* create_devfs(uint32_t options);

static filesystem_recognizer mod_filesystem_type = {
    {}, "devfs", nullptr, create_devfs
};

static void init_devfs() {
    insert_tail_list(filesystem_types, mod_filesystem_type.le);
}

static module_init module_init_btrfs(init_devfs);

static ::filesystem* create_devfs(uint32_t options) {
    if (options != 0) {
        print("create_devfs: unrecognized options {}\n", options);
        return nullptr;
    }

    auto fs = new devfs_filesystem;
    if (!fs)
        return nullptr;

    return fs;
}

devfs_filesystem::devfs_filesystem() : filesystem(nullptr) {
    auto devfs_root_inode = new devfs_inode(*this, inode_type::directory, S_IFDIR | 0755);
    if (!devfs_root_inode) {
        print("new failed\n");
        return;
    }

    root_inode = devfs_root_inode;

    // FIXME - increase with number of children?
    devfs_root_inode->file_size = 0;

    auto cur_time = (uint64_t)kernel_time_page.system_time + kernel_time_page.system_time_offset;

    devfs_root_inode->atime = cur_time;
    devfs_root_inode->ctime = cur_time;
    devfs_root_inode->mtime = cur_time;
    devfs_root_inode->otime = cur_time;

    {
        lock_guard lg(inode_lock);
        insert_tail_list(inode_list, devfs_root_inode->le);
    }
}

nt_status devfs_filesystem::flush(bool) {
    return nt_status::SUCCESS;
}

devfs_inode::devfs_inode(devfs_filesystem& fs, enum inode_type inode_type, uint32_t mode) : inode(fs), fs(fs), mode(mode) {
    this->inode_type = inode_type;
}

void devfs_inode::release() {
    {
        lock_guard lock(static_cast<devfs_filesystem&>(fs).inode_lock);

        if (--refcount != 0)
            return;

        remove_entry_list(le);
    }

    delete this;
}

nt_status devfs_inode::open_child(string_view name, object_ref& obj) {
    // FIXME - allow child directories
    if (this != fs.root_inode)
        return nt_status::OBJECT_NAME_NOT_FOUND;

    auto status = open_object(name, obj);
    if (!success(status))
        return status;

    return object_userspace_open(obj);
}

nt_status devfs_inode::create_child(string_view /*name*/, uint32_t /*create_options*/,
                                    optional<reference_wrapper<inode>>& /*obj*/,
                                    bool /*socket*/) {
    print("FIXME - devfs_inode::create_child\n");

    // FIXME

    return nt_status::NOT_IMPLEMENTED;
}

static nt_status query_directory_dirent(string_view name, bool is_dir,
                                        uint64_t time, span<uint8_t> buf,
                                        FILE_INFORMATION_CLASS file_information_class,
                                        uint64_t inode) {
    auto func = []<typename T>(string_view name, bool is_dir, uint64_t time,
                               span<uint8_t> buf, uint64_t inode) -> nt_status {
        if (buf.size() < offsetof(T, FileName))
            return nt_status::BUFFER_TOO_SMALL;

        auto utf16len = utf8_to_utf16_len(name);
        auto len = offsetof(T, FileName) + (utf16len * sizeof(char16_t));

        if (buf.size() < len)
            return nt_status::BUFFER_TOO_SMALL;

        auto& fdi = *(T*)buf.data();

        fdi.NextEntryOffset = 0;
        fdi.FileIndex = 0;
        fdi.CreationTime.QuadPart = (int64_t)time;
        fdi.LastAccessTime.QuadPart = (int64_t)time;
        fdi.LastWriteTime.QuadPart = (int64_t)time;
        fdi.ChangeTime.QuadPart = (int64_t)time;
        fdi.EndOfFile.QuadPart = 0;
        fdi.AllocationSize.QuadPart = 0;
        fdi.FileAttributes = is_dir ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_DEVICE;

        if constexpr (requires { T::FileId; })
            fdi.FileId.QuadPart = inode;

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
            return func.operator()<FILE_DIRECTORY_INFORMATION>(name, is_dir,
                                                               time, buf, inode);

        case IdFullDirectoryInformation:
            return func.operator()<FILE_ID_FULL_DIR_INFORMATION>(name, is_dir,
                                                                 time, buf, inode);

        default:
            print("devfs query_directory_dirent: unhandled info class {}\n",
                  file_information_class);
            return nt_status::INVALID_INFO_CLASS;
    }
}

nt_status devfs_inode::query_directory(span<uint8_t> buf,
                                       FILE_INFORMATION_CLASS file_information_class,
                                       uint64_t& index) {
    if (inode_type != inode_type::directory)
        return nt_status::NOT_A_DIRECTORY;

    object_ref obj;
    uint64_t time;
    char name_buf[MAX_NAME_LEN];
    string_view name;
    uint64_t inode;

    {
        lock_guard lock(name_lock);

        for (auto le = name_list.next; le != &name_list; le = le->next) {
            auto& ne = containing_record<name_entry>(le);

            if (ne.index < index)
                continue;

            time = ne.creation_time;
            inode = ne.index;
            memcpy(name_buf, ne.name, ne.name_len);
            name = string_view(name_buf, ne.name_len);

            break;
        }
    }

    if (name.empty())
        return nt_status::NO_MORE_FILES;

    auto status = query_directory_dirent(name, false, time, buf, file_information_class,
                                         inode);
    if (!success(status))
        return status;

    index = inode + 1;

    return nt_status::SUCCESS;
}

nt_status devfs_inode::query_directory_inode(string_view name, span<uint8_t> buf,
                                             FILE_INFORMATION_CLASS file_information_class) {
    // FIXME - store creation time and size for directories

    // FIXME - provide index number in same namespace as in name_entry
    return query_directory_dirent(name, inode_type == inode_type::directory,
                                  0, buf, file_information_class, 0);
}

nt_status devfs_inode::set_information(span<const uint8_t> /*buffer*/,
                                       dentry& /*dentry*/,
                                       FILE_INFORMATION_CLASS /*file_information_class*/) {
    print("FIXME - devfs_inode::set_information\n");

    // FIXME

    return nt_status::NOT_IMPLEMENTED;
}

nt_status devfs_inode::query_basic_information(span<uint8_t> buffer,
                                               uint64_t& retlen) {
    if (buffer.size() < sizeof(FILE_BASIC_INFORMATION))
        return nt_status::BUFFER_TOO_SMALL;

    auto& fbi = *(FILE_BASIC_INFORMATION*)buffer.data();

    {
        shared_lock lg(inode_lock);

        fbi.CreationTime.QuadPart = (int64_t)otime;
        fbi.LastAccessTime.QuadPart = (int64_t)atime;
        fbi.LastWriteTime.QuadPart = (int64_t)mtime;
        fbi.ChangeTime.QuadPart = (int64_t)ctime;
        fbi.FileAttributes = inode_type == inode_type::directory ? FILE_ATTRIBUTE_DIRECTORY : 0; // FIXME
    }

    retlen = sizeof(fbi);

    return nt_status::SUCCESS;
}

nt_status devfs_inode::query_standard_information(span<uint8_t> buffer,
                                                  uint64_t& retlen) {
    if (buffer.size() < sizeof(FILE_STANDARD_INFORMATION))
        return nt_status::BUFFER_TOO_SMALL;

    auto& fsi = *(FILE_STANDARD_INFORMATION*)buffer.data();

    {
        shared_lock lock(inode_lock);

        fsi.AllocationSize.QuadPart = 0; // FIXME
        fsi.EndOfFile.QuadPart = (int64_t)file_size;
        fsi.NumberOfLinks = 1;
        fsi.DeletePending = 0; // FIXME
        fsi.Directory = inode_type == inode_type::directory ? 1 : 0;
    }

    retlen = sizeof(fsi);

    return nt_status::SUCCESS;
}

nt_status devfs_inode::query_internal_information(span<uint8_t> buffer,
                                                  uint64_t& retlen) {
    if (buffer.size() < sizeof(FILE_INTERNAL_INFORMATION))
        return nt_status::BUFFER_TOO_SMALL;

    auto& fii = *(FILE_INTERNAL_INFORMATION*)buffer.data();

    fii.IndexNumber.QuadPart = 0; // FIXME?

    retlen = sizeof(fii);

    return nt_status::SUCCESS;
}

nt_status devfs_inode::query_information(span<uint8_t> buffer,
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
            print("devfs_inode::query_information: unhandled file information class {}\n",
                  file_information_class);
            return nt_status::NOT_IMPLEMENTED;
    }
}

nt_status devfs_inode::read_into_cache(uint64_t /*page_num*/, uint64_t /*num_pages*/,
                                       uint64_t& /*read_pages*/) {
    return nt_status::NOT_SUPPORTED;
}

nt_status devfs_inode::flush_cache(uint64_t /*page_num*/, uint64_t /*num_pages*/) {
    return nt_status::NOT_SUPPORTED;
}

nt_status devfs_inode::rename(::dentry& /*dentry*/, ::dentry& /*dest_dir*/,
                              string_view /*name*/, bool /*replace_if_exists*/,
                              object_ref& /*victim*/) {
    return nt_status::NOT_SUPPORTED;
}

nt_status devfs_inode::hardlink(::dentry& /*dentry*/, ::dentry& /*dest_dir*/,
                                string_view /*name*/, bool /*replace_if_exists*/,
                                object_ref& /*victim*/) {
    return nt_status::NOT_SUPPORTED;
}

nt_status devfs_filesystem::query_volume(FS_INFORMATION_CLASS fs_information_class,
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

nt_status devfs_filesystem::query_volume_size_info(span<uint8_t> buf,
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

nt_status devfs_filesystem::query_volume_attribute_info(span<uint8_t> buf,
                                                        uint32_t& retlen) {
    static const u16string_view name = u"devfs";

    if (buf.size() < offsetof(FILE_FS_ATTRIBUTE_INFORMATION, FileSystemName))
        return nt_status::INVALID_PARAMETER;

    size_t name_len = min(name.size() * sizeof(char16_t),
                          buf.size() - offsetof(FILE_FS_ATTRIBUTE_INFORMATION, FileSystemName));

    auto& info = *(FILE_FS_ATTRIBUTE_INFORMATION*)buf.data();

    info.FileSystemAttributes = FILE_CASE_PRESERVED_NAMES | FILE_CASE_SENSITIVE_SEARCH |
                                FILE_UNICODE_ON_DISK;

    // FIXME - FILE_PERSISTENT_ACLS, FILE_SUPPORTS_REPARSE_POINTS,
    //         FILE_SUPPORTS_POSIX_UNLINK_RENAME

    info.MaximumComponentNameLength = 255;
    info.FileSystemNameLength = (uint32_t)(name.size() * sizeof(char16_t));
    memcpy(info.FileSystemName, name.data(), name_len);

    retlen = (uint32_t)(offsetof(FILE_FS_ATTRIBUTE_INFORMATION, FileSystemName) + name_len);

    if (name_len < name.size() * sizeof(char16_t))
        return nt_status::BUFFER_OVERFLOW;

    return nt_status::SUCCESS;
}

nt_status devfs_inode::query_lx_information(span<uint8_t> buffer, uint64_t& retlen) {
    if (buffer.size() < sizeof(FILE_STAT_LX_INFORMATION))
        return nt_status::BUFFER_TOO_SMALL;

    auto& fsli = *(FILE_STAT_LX_INFORMATION*)buffer.data();

    {
        shared_lock lg(inode_lock);

        fsli.FileId.QuadPart = 0; // FIXME
        fsli.CreationTime.QuadPart = (int64_t)otime;
        fsli.LastAccessTime.QuadPart = (int64_t)atime;
        fsli.LastWriteTime.QuadPart = (int64_t)mtime;
        fsli.ChangeTime.QuadPart = (int64_t)ctime;
        fsli.AllocationSize.QuadPart = 0; // FIXME
        fsli.EndOfFile.QuadPart = (int64_t)file_size;
        fsli.NumberOfLinks = 1;
        fsli.EffectiveAccess = 0; // FIXME
        fsli.LxFlags = LX_FILE_METADATA_HAS_UID | LX_FILE_METADATA_HAS_GID |
                       LX_FILE_METADATA_HAS_MODE;
        fsli.LxUid = uid;
        fsli.LxGid = gid;
        fsli.LxMode = mode;

        fsli.LxDeviceIdMajor = 0;
        fsli.LxDeviceIdMinor = 0;
        fsli.ReparseTag = 0;

        switch (inode_type) {
            case inode_type::directory:
                fsli.FileAttributes = FILE_ATTRIBUTE_DIRECTORY;
                break;

            default:
                fsli.FileAttributes = FILE_ATTRIBUTE_DEVICE;
                break;
        }
    }

    retlen = sizeof(fsli);

    return nt_status::SUCCESS;
}
