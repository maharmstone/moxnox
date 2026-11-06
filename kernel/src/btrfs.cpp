// SPDX-License-Identifier: GPL-2.0-only
#include <shared_mutex>
#include <variant>
#include <span>
#include <optional>
#include <string_view>
#include <mutex>
#include <memory>
#include <algorithm>
#include <sys/stat.h>
#include "assert.h"
#include "btrfs.h"

import nt;
import print;
import format;
import bsod;
import mem;
import list;
import timer;
import unicode;
import event;
import module_init;
import thread;
import volume;
import vfs;
import object_ref;
import config;
import rbtree;
import rangelock;
import rwsem;
import page_cache;
import scheduler;
import crc32c;
import spinlock;
import objects;
import crt;
import kernel_time_page;
import handle;
import moxnox_ioctls;

using namespace std;

static const size_t MAX_INLINE = 2048; // FIXME - should be mount option

static const uint64_t superblock_addrs[] = { 0x10000, 0x4000000, 0x4000000000, 0x4000000000000 };

// FIXME - BTRFS_FEATURE_INCOMPAT_DEFAULT_SUBVOL
// FIXME - BTRFS_FEATURE_INCOMPAT_COMPRESS_LZO
// FIXME - BTRFS_FEATURE_INCOMPAT_COMPRESS_ZSTD
static const uint64_t supported_incompat_flags = BTRFS_FEATURE_INCOMPAT_MIXED_BACKREF |
                                                 BTRFS_FEATURE_INCOMPAT_BIG_METADATA |
                                                 BTRFS_FEATURE_INCOMPAT_EXTENDED_IREF |
                                                 BTRFS_FEATURE_INCOMPAT_SKINNY_METADATA |
                                                 BTRFS_FEATURE_INCOMPAT_NO_HOLES;

// FIXME - BTRFS_FEATURE_COMPAT_RO_VERITY
static const uint64_t supported_compat_ro_flags = BTRFS_FEATURE_COMPAT_RO_FREE_SPACE_TREE |
                                                  BTRFS_FEATURE_COMPAT_RO_FREE_SPACE_TREE_VALID |
                                                  BTRFS_FEATURE_COMPAT_RO_BLOCK_GROUP_TREE;

static nt_status btrfs_probe(volume& v, uint32_t options);

static filesystem_recognizer mod_filesystem_type = {
    {}, "btrfs", btrfs_probe, nullptr
};

static void init_btrfs() {
    insert_tail_list(filesystem_types, mod_filesystem_type.le);
}

static module_init module_init_btrfs(init_btrfs);

struct ref_change_data {
    list_entry le;
    uint64_t root;
    uint64_t objectid;
    uint64_t offset;
    int32_t count;
};

struct ref_change {
    rbtree_node<uint64_t> tree;
    bool is_metadata;

    union {
        struct {
            uint8_t level;
            uint64_t root;
            int32_t count;
        } metadata;
        struct {
            uint64_t extent_length;
            list_entry rcds;
        } data;
    };
};

struct chunk {
    ~chunk();

    list_entry le;
    uint64_t offset;
    list_entry space_list = { &space_list, &space_list };
    list_entry pending_space_list = { &pending_space_list, &pending_space_list };
    rwsem space_loading_lock;
    bool space_loaded = false;
    bool using_fst_bitmaps;
    bool changed = false;
    list_entry changed_le;
    list_entry space_pending_le;
    list_entry new_le;
    int64_t delta;
    uint64_t last_allocation_end = 0;
    btrfs_chunk ci; // must be last item
};

struct btrfs_path {
    btrfs_path(size_t node_size) : node_size(node_size) {
        for (unsigned int i = 0; i < BTRFS_MAX_LEVEL; i++) {
            data[i] = nullptr;
        }
    }

    ~btrfs_path() {
        reset();
    }

    void reset();

    uint8_t* data[BTRFS_MAX_LEVEL];
    uint32_t slots[BTRFS_MAX_LEVEL];
    size_t node_size; // FIXME - we shouldn't need this
};

class btrfs_filesystem;

struct root {
    list_entry le;
    uint64_t id;
    btrfs_root_item ri;
    atomic<uint64_t> next_inode_number = 0;
    bool is_dirty = false;
    list_entry dirty_le;
    spinlock inode_lock;
    rbtree_node<uint64_t>* inodes = nullptr;
};

class btrfs_inode : public inode {
public:
    btrfs_inode(btrfs_filesystem& fs, uint64_t subvol, uint64_t inode, enum btrfs_inode_type inode_type);
    ~btrfs_inode();
    bool load_inode();
    void release() override;

    nt_status open_child(string_view name, object_ref& obj) override;
    nt_status create_child(string_view name, uint32_t create_options,
                           optional<reference_wrapper<inode>>& obj) override;
    NTSTATUS query_directory(span<uint8_t> buf, FILE_INFORMATION_CLASS file_information_class,
                             uint64_t& index) override;
    nt_status query_directory_inode(string_view name, span<uint8_t> buf,
                                    FILE_INFORMATION_CLASS file_information_class) override;
    NTSTATUS set_information(span<const uint8_t> buffer, dentry& dentry,
                             FILE_INFORMATION_CLASS file_information_class) override;
    NTSTATUS query_information(span<uint8_t> buffer,
                               FILE_INFORMATION_CLASS file_information_class,
                               uint64_t& retlen) override;
    nt_status rename(::dentry& dentry, ::dentry& dest_dir, string_view name,
                     bool replace_if_exists, object_ref& victim) override;
    nt_status hardlink(::dentry& dentry, ::dentry& dest_dir, string_view name,
                       bool replace_if_exists, object_ref& victim) override;
    nt_status read_into_cache(uint64_t page_num, uint64_t num_pages,
                              uint64_t& read_pages) override;
    nt_status flush_cache(uint64_t page_num, uint64_t num_pages) override;
    nt_status flush_cache_holding_trans_lock(uint64_t page_num, uint64_t num_pages);
    nt_status set_basic_information(span<const uint8_t> buffer);
    nt_status set_disposition_information(span<const uint8_t> buffer,
                                          dentry& dentry);
    nt_status query_basic_information(span<uint8_t> buffer, uint64_t& retlen);
    nt_status query_standard_information(span<uint8_t> buffer, uint64_t& retlen);
    nt_status query_internal_information(span<uint8_t> buffer, uint64_t& retlen);
    nt_status query_lx_information(span<uint8_t> buffer, uint64_t& retlen);
    nt_status flush_inode_item(btrfs_path& p);
    nt_status get_last_dir_index(uint64_t& ret);
    void last_handle_closed() override;
    nt_status fs_control(uint32_t code, span<const uint8_t> inbuf,
                         span<uint8_t> outbuf, uint32_t& retlen,
                         dentry* dentry) override;

    root* r;
    btrfs_filesystem& fs; // FIXME - use reference in ::inode
    btrfs_inode_item ii;
    list_entry extents = { &extents, &extents };
    bool dead = false;
    rbtree_node<uint64_t> tree;

private:
    nt_status read_zeroes_into_cache(uint64_t page_num, uint64_t num_pages,
                                     uint64_t& read_pages);
    nt_status read_inline_extent_into_cache(uint64_t page_num, uint64_t num_pages,
                                            const inode_extent& ie,
                                            uint64_t& read_pages);
    nt_status read_extent_into_cache(uint64_t page_num, uint64_t num_pages,
                                     const inode_extent& ie,
                                     uint64_t& read_pages);
    nt_status add_file_extent(uint64_t extent_address, uint64_t extent_length,
                              uint64_t extent_offset, uint64_t file_offset,
                              uint64_t length);
    nt_status remove_orphan(bool no_orphan_item);
    nt_status get_reparse_point(span<uint8_t> outbuf, uint32_t& retlen);
    nt_status set_reparse_point(span<const uint8_t> inbuf, dentry& dentry);
    nt_status set_reparse_point_symlink(span<const uint8_t> inbuf,
                                        dentry& dentry);
    nt_status set_reparse_point_fifo(dentry& dentry);
    nt_status do_fchown(span<const uint8_t> inbuf);
    nt_status do_fchmod(span<const uint8_t> inbuf);
};

class btrfs_filesystem : public ::filesystem {
public:
    btrfs_filesystem(volume& vol, bool readonly);

    ~btrfs_filesystem() override {
        free_chunk_list();
        free_root_list();
        free_dev_space_list();
        vol->release();

        if (sb)
            free_folio(phys_memory_pfn(sb) << PAGE_SHIFT);
    }

    nt_status read_metadata(uint64_t addr, span<uint8_t> data);
    nt_status open_inode(uint64_t subvol, uint64_t inode, object_ref& obj);
    nt_status remove_item(root& r, const btrfs_key& k);
    nt_status remove_item2(const btrfs_path& p, root& r);
    nt_status change_key(btrfs_path& p, const btrfs_key& k);
    void shorten_item(btrfs_path& p, uint32_t delta);
    nt_status extend_item(btrfs_path& p, uint32_t delta);
    nt_status find_item(root& r, const btrfs_key& k, btrfs_path& p, bool cow);
    nt_status move_path_back(btrfs_path& p, root& r, bool cow);
    nt_status move_path_forward(btrfs_path& p, root& r, bool cow);
    root* find_root(uint64_t root_num);
    nt_status flush(bool make_readonly) override;
    void flush_thread();
    bool flush_tree(const chunk& c, uint64_t addr);
    nt_status flush_data(const chunk& c, uint64_t addr, uint64_t length);
    nt_status read_into_cache(uint64_t page_num, uint64_t num_pages,
                              uint64_t& read_pages);
    nt_status flush_cache(uint64_t page_num, uint64_t num_pages);
    nt_status query_volume(FS_INFORMATION_CLASS fs_information_class,
                           span<uint8_t> buf, uint32_t& retlen) override;

    btrfs_super_block* sb = nullptr;
    bool valid = false;
    list_entry chunk_list = { &chunk_list, &chunk_list };
    list_entry root_list = { &root_list, &root_list };
    rwsem trans_lock;
    root* root_root;
    root* chunk_root;
    root* extent_root = nullptr;
    root* free_space_root = nullptr;
    root* csum_root = nullptr;
    root* dev_root = nullptr;
    root* fs_root = nullptr;
    root* block_group_root = nullptr;
    list_entry dirty_root_list = { &dirty_root_list, &dirty_root_list };
    spinlock ref_changes_lock;
    rbtree_node<uint64_t>* ref_changes = nullptr;
    list_entry space_pending_chunks = { &space_pending_chunks, &space_pending_chunks };
    object_ref flush_thread_obj;
    uint8_t node_folio_order;
    page_cache<btrfs_filesystem> cache{*this};
    object_ref readonly_event;
    btrfs_uuid chunk_uuid;
    list_entry new_chunks = { &new_chunks, &new_chunks };
    list_entry dev_space_list = { &dev_space_list, &dev_space_list };
    uint64_t data_profile, metadata_profile, system_profile;

private:
    bool load_chunks();
    bool load_roots();
    bool load_dev_space();
    void free_chunk_list();
    void free_root_list();
    void free_dev_space_list();
    void go_readonly();
    nt_status query_volume_size_info(span<uint8_t> buf, uint32_t& retlen);
    nt_status query_volume_attribute_info(span<uint8_t> buf, uint32_t& retlen);
};

static_assert(cacheable_flushable<btrfs_filesystem>);

static nt_status add_new_level(btrfs_filesystem& fs, btrfs_path& p, root& r,
                               uint8_t level);
static nt_status insert_internal_node(btrfs_filesystem& fs, btrfs_path& p,
                                      root& r, uint8_t level, const btrfs_key& k,
                                      uint64_t address);
static nt_status insert_item(btrfs_filesystem& fs, root& r, const btrfs_key& k,
                             span<const uint8_t> data);
static nt_status cow_tree(btrfs_filesystem& fs, root& r, btrfs_path& p);

static bool check_checksum(const btrfs_super_block& sb) {
    auto csum = ~calc_crc32c(0xffffffff, span((const uint8_t*)&sb.fsid, sizeof(btrfs_super_block) - sizeof(sb.csum)));

    return csum == *(uint32_t*)&sb;
}

struct folio_iter : public pfn_iterator {
    folio_iter(uint64_t page_num) : page_num(page_num) { }

    uint64_t operator*() const override {
        return page_num;
    }

    folio_iter& operator++() override {
        page_num++;

        return *this;
    }

private:
    uint64_t page_num;
};

template<>
consteval bool format_spec_valid<btrfs_key>(string_view fmt) {
    return fmt.empty();
}

span<char> do_format_arg(span<char> sp, string_view, const btrfs_key& k) {
    sp = do_format_arg(sp, "", "(");
    sp = do_format_arg(sp, ":x", k.objectid);
    sp = do_format_arg(sp, "", ",");
    sp = do_format_arg(sp, ":x", (uint8_t)k.type);
    sp = do_format_arg(sp, "", ",");
    sp = do_format_arg(sp, ":x", k.offset);
    sp = do_format_arg(sp, "", ")");

    return sp;
}

template<>
consteval bool format_spec_valid<enum key_type>(string_view fmt) {
    return fmt.empty();
}

span<char> do_format_arg(span<char> sp, string_view, enum key_type t) {
    switch (t) {
        case key_type::INODE_ITEM:
            return do_format_arg(sp, "", "INODE_ITEM");
        case key_type::INODE_REF:
            return do_format_arg(sp, "", "INODE_REF");
        case key_type::INODE_EXTREF:
            return do_format_arg(sp, "", "INODE_EXTREF");
        case key_type::XATTR_ITEM:
            return do_format_arg(sp, "", "XATTR_ITEM");
        case key_type::VERITY_DESC_ITEM:
            return do_format_arg(sp, "", "VERITY_DESC_ITEM");
        case key_type::VERITY_MERKLE_ITEM:
            return do_format_arg(sp, "", "VERITY_MERKLE_ITEM");
        case key_type::ORPHAN_INODE:
            return do_format_arg(sp, "", "ORPHAN_INODE");
        case key_type::DIR_LOG_INDEX:
            return do_format_arg(sp, "", "DIR_LOG_INDEX");
        case key_type::DIR_ITEM:
            return do_format_arg(sp, "", "DIR_ITEM");
        case key_type::DIR_INDEX:
            return do_format_arg(sp, "", "DIR_INDEX");
        case key_type::EXTENT_DATA:
            return do_format_arg(sp, "", "EXTENT_DATA");
        case key_type::EXTENT_CSUM:
            return do_format_arg(sp, "", "EXTENT_CSUM");
        case key_type::ROOT_ITEM:
            return do_format_arg(sp, "", "ROOT_ITEM");
        case key_type::ROOT_BACKREF:
            return do_format_arg(sp, "", "ROOT_BACKREF");
        case key_type::ROOT_REF:
            return do_format_arg(sp, "", "ROOT_REF");
        case key_type::EXTENT_ITEM:
            return do_format_arg(sp, "", "EXTENT_ITEM");
        case key_type::METADATA_ITEM:
            return do_format_arg(sp, "", "METADATA_ITEM");
        case key_type::EXTENT_OWNER_REF:
            return do_format_arg(sp, "", "EXTENT_OWNER_REF");
        case key_type::TREE_BLOCK_REF:
            return do_format_arg(sp, "", "TREE_BLOCK_REF");
        case key_type::EXTENT_DATA_REF:
            return do_format_arg(sp, "", "EXTENT_DATA_REF");
        case key_type::SHARED_BLOCK_REF:
            return do_format_arg(sp, "", "SHARED_BLOCK_REF");
        case key_type::SHARED_DATA_REF:
            return do_format_arg(sp, "", "SHARED_DATA_REF");
        case key_type::BLOCK_GROUP_ITEM:
            return do_format_arg(sp, "", "BLOCK_GROUP_ITEM");
        case key_type::FREE_SPACE_INFO:
            return do_format_arg(sp, "", "FREE_SPACE_INFO");
        case key_type::FREE_SPACE_EXTENT:
            return do_format_arg(sp, "", "FREE_SPACE_EXTENT");
        case key_type::FREE_SPACE_BITMAP:
            return do_format_arg(sp, "", "FREE_SPACE_BITMAP");
        case key_type::DEV_EXTENT:
            return do_format_arg(sp, "", "DEV_EXTENT");
        case key_type::DEV_ITEM:
            return do_format_arg(sp, "", "DEV_ITEM");
        case key_type::CHUNK_ITEM:
            return do_format_arg(sp, "", "CHUNK_ITEM");
        case key_type::RAID_STRIPE:
            return do_format_arg(sp, "", "RAID_STRIPE");
        case key_type::QGROUP_STATUS:
            return do_format_arg(sp, "", "QGROUP_STATUS");
        case key_type::QGROUP_INFO:
            return do_format_arg(sp, "", "QGROUP_INFO");
        case key_type::QGROUP_LIMIT:
            return do_format_arg(sp, "", "QGROUP_LIMIT");
        case key_type::QGROUP_RELATION:
            return do_format_arg(sp, "", "QGROUP_RELATION");
        case key_type::TEMPORARY_ITEM:
            return do_format_arg(sp, "", "TEMPORARY_ITEM");
        case key_type::PERSISTENT_ITEM:
            return do_format_arg(sp, "", "PERSISTENT_ITEM");
        case key_type::DEV_REPLACE:
            return do_format_arg(sp, "", "DEV_REPLACE");
        case key_type::UUID_SUBVOL:
            return do_format_arg(sp, "", "UUID_SUBVOL");
        case key_type::UUID_RECEIVED_SUBVOL:
            return do_format_arg(sp, "", "UUID_RECEIVED_SUBVOL");
        case key_type::STRING_ITEM:
            return do_format_arg(sp, "", "STRING_ITEM");
        default:
            return do_format_arg(sp, "", (unsigned int)t);
    }
}

btrfs_filesystem::btrfs_filesystem(volume& vol, bool readonly) : filesystem(&vol) {
    this->readonly = readonly;

    vol.acquire();

    auto sb_pa = allocate_folio(bytes_to_folio_order(sizeof(btrfs_super_block)));
    if (!sb_pa)
        return;

    sb = (btrfs_super_block*)phys_address(sb_pa);

    auto iter = folio_iter(sb_pa);

    uint64_t read_pages;

    auto status = vol.dio_read(superblock_addrs[0] >> PAGE_SHIFT,
                               bytes_to_pages(sizeof(btrfs_super_block)),
                               iter, read_pages);
    if (!success(status)) {
        print("volume::dio_read returned {}\n", status);
        return;
    }

    if (sb->magic != BTRFS_MAGIC)
        return;

    // FIXME - if checksum fails, try other superblock addresses
    if (!check_checksum(*sb)) {
        print("Incorrect superblock checksum\n");
        return;
    }

    // FIXME - check other superblock addresses, and choose one with highest generation

    if (sb->incompat_flags & ~supported_incompat_flags) {
        print("Unsupported incompat flags\n");
        return;
    }

    if (!(sb->incompat_flags & BTRFS_FEATURE_INCOMPAT_MIXED_BACKREF)) {
        print("Not mounting as BTRFS_FEATURE_INCOMPAT_MIXED_BACKREF not set\n");
        return;
    }

    sb->incompat_flags |= BTRFS_FEATURE_INCOMPAT_BIG_METADATA |
                          BTRFS_FEATURE_INCOMPAT_EXTENDED_IREF |
                          BTRFS_FEATURE_INCOMPAT_SKINNY_METADATA |
                          BTRFS_FEATURE_INCOMPAT_NO_HOLES;

    // FIXME - support free space cache?
    if (!(sb->compat_ro_flags & (BTRFS_FEATURE_COMPAT_RO_FREE_SPACE_TREE | BTRFS_FEATURE_COMPAT_RO_FREE_SPACE_TREE_VALID))) {
        print("free space cache not supported\n");
        return;
    }

    if (!readonly && sb->compat_ro_flags & ~supported_compat_ro_flags) {
        print("Unsupported compat_ro_flags {:x}, mounting readonly\n", sb->compat_ro_flags & ~supported_compat_ro_flags);
        readonly = true;
    }

    sb->incompat_flags |= BTRFS_FEATURE_INCOMPAT_SKINNY_METADATA;

    if (sb->num_devices != 1) {
        print("Multiple devices not supported\n");
        return;
    }

    if (sb->nodesize & (PAGE_SIZE - 1)) {
        print("btrfs node size not multiple of CPU page size\n");
        return;
    }

    if (sb->nodesize & (sb->nodesize - 1)) {
        print("btrfs node size {:x} not a power of two\n", +sb->nodesize);
        return;
    }

    node_folio_order = (uint8_t)((sizeof(uint32_t) * 8) - (uint8_t)countl_zero((uint32_t)sb->nodesize) - 1 - PAGE_SHIFT);

    if (node_folio_order > MAX_FOLIO_ORDER) {
        print("btrfs metadata folio order {} larger than maximum {}\n", node_folio_order, MAX_FOLIO_ORDER);
        return;
    }

    // FIXME - subpage on ARM?
    if (sb->sectorsize != PAGE_SIZE) {
        print("btrfs sector size is not CPU page size\n");
        return;
    }

    if (sb->csum_type != 0) {
        print("unsupported csum type {}\n", +sb->csum_type);
        return;
    }

    if (sb->sys_chunk_array_size > sb->sys_chunk_array.size()) {
        print("sys_chunk_array_size was {}, expected {} or less\n",
              +sb->sys_chunk_array_size, sb->sys_chunk_array.size());
        return;
    }

    auto chunk_array = span(sb->sys_chunk_array.data(), sb->sys_chunk_array_size);

    while (!chunk_array.empty()) {
        if (chunk_array.size() < sizeof(btrfs_key)) {
            print("Chunk array truncated\n");
            return;
        }

        auto& k = *(btrfs_key*)chunk_array.data();

        if (k.type != key_type::CHUNK_ITEM) {
            print("Unexpected item in chunk array\n");
            return;
        }

        chunk_array = chunk_array.subspan(sizeof(btrfs_key));

        if (chunk_array.size() < offsetof(btrfs_chunk, stripe)) {
            print("Chunk array truncated\n");
            return;
        }

        auto& ci = *(btrfs_chunk*)chunk_array.data();
        size_t chunk_size = offsetof(btrfs_chunk, stripe) + (sizeof(btrfs_stripe) * ci.num_stripes);

        if (chunk_array.size() < chunk_size) {
            print("Chunk array truncated\n");
            return;
        }

        if (ci.type & (BTRFS_BLOCK_GROUP_RAID0 | BTRFS_BLOCK_GROUP_RAID1 | BTRFS_BLOCK_GROUP_RAID10 | BTRFS_BLOCK_GROUP_RAID5 | BTRFS_BLOCK_GROUP_RAID6 | BTRFS_BLOCK_GROUP_RAID1C3 | BTRFS_BLOCK_GROUP_RAID1C4)) {
            print("RAID not supported\n");
            return;
        }

        auto cptr = (chunk*)malloc(offsetof(chunk, ci) + chunk_size);
        if (!cptr) {
            print("malloc failed\n");
            return;
        }

        auto& c = *cptr;
        new (&c) chunk;

        c.offset = k.offset;
        memcpy(&c.ci, chunk_array.data(), chunk_size);
        insert_tail_list(chunk_list, c.le);

        chunk_array = chunk_array.subspan(chunk_size);
    }

    {
        auto rptr = new root;
        if (!rptr) {
            print("new failed\n");
            return;
        }

        auto& r = *rptr;
        r.id = BTRFS_ROOT_TREE_OBJECTID;
        r.ri.bytenr = sb->root;
        r.ri.level = sb->root_level;

        root_root = &r;
        insert_tail_list(root_list, r.le);
    }

    {
        auto rptr = new root;
        if (!rptr) {
            print("new failed\n");
            return;
        }

        auto& r = *rptr;
        r.id = BTRFS_CHUNK_TREE_OBJECTID;
        r.ri.bytenr = sb->chunk_root;
        r.ri.level = sb->chunk_root_level;
        r.ri.generation = sb->chunk_root_generation;

        chunk_root = &r;
        insert_tail_list(root_list, r.le);
    }

    if (!load_chunks()) {
        print("load_chunks failed\n");
        return;
    }

    if (!load_roots()) {
        print("load_roots failed\n");
        return;
    }

    // FIXME - set readonly if underlying disk is RO

    if (!readonly && !load_dev_space()) {
        print("load_dev_space failed\n");
        return;
    }

    // FIXME - default subvol

    auto btrfs_root_inode = new btrfs_inode(*this, BTRFS_FS_TREE_OBJECTID, SUBVOL_ROOT_INODE,
                                            btrfs_inode_type::directory);
    if (!btrfs_root_inode) {
        print("new failed\n");
        return;
    }

    if (!btrfs_root_inode->load_inode()) {
        btrfs_root_inode->release();
        return;
    }

    root_inode = btrfs_root_inode;

    insert_into_rbtree(btrfs_root_inode->r->inodes, btrfs_root_inode->tree);

    sb->generation++;

    // clear log tree (FIXME - replay this)
    sb->log_root = 0;
    sb->log_root_level = 0;

    readonly_event = create_event(false);
    if (!readonly_event) {
        print("create_event failed\n");
        return;
    }

    valid = true;

    if (!readonly) {
        acquire();

        flush_thread_obj = add_kernel_thread([](void* ctx) {
            auto& fs = *(btrfs_filesystem*)ctx;

            fs.flush_thread();

            fs.release();
        }, this);
    }
}

static nt_status btrfs_probe(volume& vol, uint32_t options) {
    static const uint32_t SUPPORTED_OPTIONS = MOXNOX_VFSCTL_MOUNT_READONLY;

    // FIXME - need quick is_btrfs function before we do allocations?

    if (options & ~SUPPORTED_OPTIONS) {
        print("btrfs_probe: unsupported mount options {}\n", options);
        return nt_status::INVALID_PARAMETER;
    }

    auto fs = new btrfs_filesystem(vol, options & MOXNOX_VFSCTL_MOUNT_READONLY);
    if (!fs) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    if (!fs->valid) {
        delete fs;
        return nt_status::UNRECOGNIZED_VOLUME;
    }

    vol.filesystem = *fs;

    return nt_status::SUCCESS;
}

nt_status btrfs_filesystem::read_metadata(uint64_t addr, span<uint8_t> data) {
    assert(data.size() == sb->nodesize);

    auto status = cache.read(span(&data, 1), addr);
    if (!success(status)) {
        print("btrfs_filesystem::read_metadata: page_cache::read returned {}\n",
              status);
        return status;
    }

    // FIXME - check level (and subvol ID?)
    // FIXME - if duff, read from other version (and replace)

    return nt_status::SUCCESS;
}

bool btrfs_filesystem::load_chunks() {
    list_entry new_chunk_list = { &new_chunk_list, &new_chunk_list };

    btrfs_path p(sb->nodesize);

    auto status = find_item(*chunk_root, {0, key_type::CHUNK_ITEM, 0}, p, false);
    if (!success(status))
        return false;

    bool fail = false;

    {
        auto& th = *(btrfs_header*)p.data[0];

        chunk_uuid = th.chunk_tree_uuid;
    }

    data_profile = 0;
    metadata_profile = 0;
    system_profile = 0;

    do {
        auto& th = *(btrfs_header*)p.data[0];
        auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

        if (p.slots[0] >= th.nritems)
            break;

        const auto& it = items[p.slots[0]];
        const auto& k = it.key;

        if (k.type != key_type::CHUNK_ITEM) {
            status = move_path_forward(p, *chunk_root, false);
            if (status == nt_status::NOT_FOUND)
                break;
            else if (!success(status))
                return false; // FIXME

            continue;
        }

        auto data = span((const uint8_t*)&th + sizeof(btrfs_header) + it.offset, it.size);

        if (data.size() < offsetof(btrfs_chunk, stripe)) {
            print("Chunk item truncated\n");
            fail = true;
            break;
        }

        auto& ci = *(btrfs_chunk*)data.data();
        size_t chunk_size = offsetof(btrfs_chunk, stripe) + (sizeof(btrfs_stripe) * ci.num_stripes);

        if (data.size() < chunk_size) {
            print("Chunk array truncated\n");
            fail = true;
            break;
        }

        if (ci.type & (BTRFS_BLOCK_GROUP_RAID0 | BTRFS_BLOCK_GROUP_RAID1 | BTRFS_BLOCK_GROUP_RAID10 | BTRFS_BLOCK_GROUP_RAID5 | BTRFS_BLOCK_GROUP_RAID6 | BTRFS_BLOCK_GROUP_RAID1C3 | BTRFS_BLOCK_GROUP_RAID1C4)) {
            print("RAID not supported\n");
            fail = true;
            break;
        }

        auto cptr = (chunk*)malloc(offsetof(chunk, ci) + chunk_size);
        if (!cptr) {
            print("malloc failed\n");
            fail = true;
            break;
        }

        auto& c = *cptr;
        new (&c) chunk;

        c.offset = k.offset;
        memcpy(&c.ci, data.data(), chunk_size);
        insert_tail_list(new_chunk_list, c.le);

        if (data_profile == 0 && ci.type & BTRFS_BLOCK_GROUP_DATA)
            data_profile = ci.type;

        if (metadata_profile == 0 && ci.type & BTRFS_BLOCK_GROUP_METADATA)
            metadata_profile = ci.type;

        if (system_profile == 0 && ci.type & BTRFS_BLOCK_GROUP_SYSTEM)
            system_profile = ci.type;

        status = move_path_forward(p, *chunk_root, false);
        if (status == nt_status::NOT_FOUND)
            break;
        else if (!success(status)) {
            fail = true;
            break;
        }
    } while (true);

    if (fail) {
        swap_lists(chunk_list, new_chunk_list);
        free_chunk_list();
        swap_lists(chunk_list, new_chunk_list);
        return false;
    }

    free_chunk_list();
    swap_lists(chunk_list, new_chunk_list);

    return true;
}

bool btrfs_filesystem::load_dev_space() {
    btrfs_path p(sb->nodesize);
    uint64_t devid = sb->dev_item.devid;

    auto status = find_item(*dev_root, {devid - 1, key_type::DEV_EXTENT, 0xffffffffffffffff}, p, false);
    if (!success(status))
        return false;

    bool fail = false;
    uint64_t last_end = 0x100000; // 1 MB
    bool have_items = true;

    {
        auto& th = *(btrfs_header*)p.data[0];

        if (p.slots[0] == th.nritems) {
            status = move_path_forward(p, *dev_root, false);

            if (status == nt_status::NOT_FOUND)
                have_items = false;
            else if (!success(status))
                return false; // FIXME
        }
    }

    if (have_items) {
        do {
            auto& th = *(btrfs_header*)p.data[0];
            auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

            if (p.slots[0] >= th.nritems)
                break;

            const auto& it = items[p.slots[0]];
            const auto& k = it.key;

            if (k.objectid > devid)
                break;

            if (k.type != key_type::DEV_EXTENT || k.objectid != devid) {
                status = move_path_forward(p, *dev_root, false);

                if (status == nt_status::NOT_FOUND)
                    break;
                else if (!success(status)) {
                    fail = true;
                    break;
                }

                continue;
            }

            auto data = span((const uint8_t*)&th + sizeof(btrfs_header) + it.offset, it.size);

            if (data.size() < sizeof(btrfs_dev_extent)) {
                fail = true;
                break;
            }

            auto& de = *(btrfs_dev_extent*)data.data();

            if (last_end < k.offset) {
                auto s = (chunk_space*)malloc(sizeof(chunk_space));
                if (!s) {
                    print("malloc failed\n");
                    fail = true;
                    break;
                }

                s->offset = last_end;
                s->length = k.offset - last_end;
                insert_tail_list(dev_space_list, s->le);
            }

            last_end = k.offset + de.length;

            status = move_path_forward(p, *dev_root, false);
            if (status == nt_status::NOT_FOUND)
                break;
            else if (!success(status)) {
                fail = true;
                break;
            }
        } while (true);
    }

    if (last_end < sb->dev_item.total_bytes) {
        auto s = (chunk_space*)malloc(sizeof(chunk_space));
        if (!s) {
            print("malloc failed\n");
            fail = true;
        } else {
            s->offset = last_end;
            s->length = sb->dev_item.total_bytes - last_end;
            insert_tail_list(dev_space_list, s->le);
        }
    }

    if (fail) {
        while (!is_list_empty(dev_space_list)) {
            auto& s = containing_record<chunk_space>(dev_space_list.next);

            remove_entry_list(s.le);
            free(&s);
        }

        return false;
    }

    return true;
}

static chunk* find_chunk(const btrfs_filesystem& fs, uint64_t address) {
    for (auto le = fs.chunk_list.next; le != &fs.chunk_list; le = le->next) {
        auto& c = containing_record<chunk>(le);

        if (address >= c.offset && address < c.offset + c.ci.length)
            return &c;
    }

    return nullptr;
}

nt_status btrfs_filesystem::read_into_cache(uint64_t page_num, uint64_t num_pages,
                                            uint64_t& read_pages) {
    uint64_t addr = page_num << PAGE_SHIFT;

    // find chunk

    auto pc = find_chunk(*this, addr);

    if (!pc) {
        print("Could not find chunk for logical address {:x}\n", addr);
        return nt_status::DISK_CORRUPT_ERROR;
    }

    auto& c = *pc;

    if (c.ci.type & BTRFS_BLOCK_GROUP_RAID0) {
        print("RAID0 not supported\n");
        return nt_status::NOT_SUPPORTED;
    } else if (c.ci.type & BTRFS_BLOCK_GROUP_RAID10) {
        print("RAID10 not supported\n");
        return nt_status::NOT_SUPPORTED;
    } else if (c.ci.type & BTRFS_BLOCK_GROUP_RAID5) {
        print("RAID5 not supported\n");
        return nt_status::NOT_SUPPORTED;
    } else if (c.ci.type & BTRFS_BLOCK_GROUP_RAID6) {
        print("RAID6 not supported\n");
        return nt_status::NOT_SUPPORTED;
    }

    // chunk is SINGLE, DUP, RAID1, RAID1C3, or RAID1C4

    // FIXME - choose stripe and device
    // FIXME - handle missing devices

    uint64_t disk_addr = addr - c.offset + c.ci.stripe[0].offset;

    if (addr + (num_pages << PAGE_SHIFT) > c.offset + c.ci.length)
        num_pages = (c.offset + c.ci.length - addr) >> PAGE_SHIFT;

    uint64_t pages_read;

    uint64_t disk_read_start = disk_addr >> PAGE_SHIFT;
    uint64_t disk_read_pages = num_pages;
    uint64_t prefix_pages = 0, suffix_pages = 0;
    bool is_metadata = c.ci.type & (BTRFS_BLOCK_GROUP_METADATA | BTRFS_BLOCK_GROUP_SYSTEM);
    unsigned int pages_per_tree = sb->nodesize >> PAGE_SHIFT;

    if (is_metadata) {
        prefix_pages = disk_read_start & (pages_per_tree - 1);
        disk_read_start -= prefix_pages;
        disk_read_pages += prefix_pages;

        if (disk_read_pages & (pages_per_tree - 1)) {
            suffix_pages = pages_per_tree - (disk_read_pages & (pages_per_tree - 1));
            disk_read_pages += suffix_pages;
        }
    }

    auto status = vol->cache.read_into_cache_and_pin(disk_read_start, disk_read_pages,
                                                     pages_read);
    if (!success(status))
        return status;

    if (is_metadata && pages_read & (pages_per_tree - 1)) {
        auto ignore_pages = (unsigned int)(pages_read & (pages_per_tree - 1));

        vol->cache.unpin(disk_read_start + pages_read - ignore_pages,
                         ignore_pages);
        pages_read -= ignore_pages;
        suffix_pages = 0;
    }

    if (pages_read > prefix_pages + suffix_pages) {
        rangelock_req rlr(vol->cache.base.rl, disk_read_start, pages_read, rangelock_mode::exclusive);

        if (is_metadata) {
            uint64_t exp_addr = (page_num - prefix_pages) << PAGE_SHIFT;
            page_cache_entry* p = nullptr;

            for (unsigned int i = 0; i < pages_read / pages_per_tree; i++) {
                uint32_t exp_csum, csum;
                bool error = false;

                for (unsigned int j = 0; j < pages_per_tree; j++) {
                    {
                        lock_guard lg(vol->cache.base.cache_lock);

                        if (!p)
                            p = vol->cache.find_page_cache_entry(disk_read_start);
                        else
                            p = vol->cache.next_page_cache_entry(*p);
                    }

                    if (j == 0) {
                        const auto& th = *(btrfs_header*)get_phys_address(*p);

                        if (th.bytenr != exp_addr) {
                            print("btrfs_filesystem::read_into_cache: metadata had address of {:x}, expected {:x}\n",
                                  th.bytenr, exp_addr);
                            error = true;
                            break;
                        }

                        exp_csum = *(uint32_t*)th.csum.data();
                        csum = calc_crc32c(0xffffffff, span((const uint8_t*)&th.fsid, PAGE_SIZE - sizeof(th.csum)));
                    } else
                        csum = calc_crc32c(csum, span((const uint8_t*)get_phys_address(*p), PAGE_SIZE));
                }

                if (!error) {
                    csum = ~csum;

                    if (csum != exp_csum) {
                        print("btrfs_filesystem::read_into_cache: metadata at {:x} had csum {:08x}, expected {:08x}\n",
                              exp_addr, csum, exp_csum);
                        error = true;
                    }
                }

                if (error) {
                    // FIXME - try to read other copies if present
                    // FIXME - if success, copy good copies over bad
                    // FIXME - increase dev stats counter

                    if (i == 0) {
                        vol->cache.unpin(disk_read_start, pages_read);
                        return nt_status::DISK_CORRUPT_ERROR;
                    }

                    vol->cache.unpin(disk_read_start + (i * pages_per_tree),
                                     pages_read - (i * pages_per_tree));

                    pages_read = i * pages_per_tree;
                    suffix_pages = 0;
                    break;
                }

                exp_addr += pages_per_tree << PAGE_SHIFT;
            }
        }

        page_cache_entry* dest_p;
        page_cache_entry* src_p;

        for (uint64_t i = 0; i < pages_read - prefix_pages - suffix_pages; i++) {
            {
                lock_guard lg(cache.base.cache_lock);

                if (i == 0)
                    dest_p = cache.find_page_cache_entry(page_num);
                else
                    dest_p = cache.next_page_cache_entry(*dest_p);
            }

            {
                lock_guard lg(vol->cache.base.cache_lock);

                if (i == 0)
                    src_p = vol->cache.find_page_cache_entry(disk_read_start + prefix_pages);
                else
                    src_p = vol->cache.next_page_cache_entry(*src_p);
            }

            increase_page_refcount(src_p->pfn);
            dest_p->pfn = src_p->pfn;
        }
    }

    vol->cache.unpin(disk_read_start, pages_read);

    read_pages = pages_read - prefix_pages - suffix_pages;
    return nt_status::SUCCESS;
}

bool btrfs_filesystem::flush_tree(const chunk& c, uint64_t addr) {
    unsigned int pages_per_tree = sb->nodesize >> PAGE_SHIFT;

    if (c.ci.type & BTRFS_BLOCK_GROUP_RAID0) {
        print("RAID0 not supported\n");
        return false;
    } else if (c.ci.type & BTRFS_BLOCK_GROUP_RAID10) {
        print("RAID10 not supported\n");
        return false;
    } else if (c.ci.type & BTRFS_BLOCK_GROUP_RAID5) {
        print("RAID5 not supported\n");
        return false;
    } else if (c.ci.type & BTRFS_BLOCK_GROUP_RAID6) {
        print("RAID6 not supported\n");
        return false;
    }

    // chunk is SINGLE, DUP, RAID1, RAID1C3, or RAID1C4

    // FIXME - handle missing devices

    // copy into page cache of volumes

    cache.pin(addr >> PAGE_SHIFT, pages_per_tree, nullptr);

    page_cache_entry* first_page;

    {
        lock_guard lock(cache.base.cache_lock);
        first_page = cache.find_page_cache_entry(addr >> PAGE_SHIFT);
    }

    // set WRITTEN flag

    auto& th = *(btrfs_header*)get_phys_address(*first_page);
    th.flags |= HEADER_FLAG_WRITTEN;

    // calculate hash

    {
        auto ent = first_page;
        uint32_t crc32 = 0xffffffff;

        for (unsigned j = 0; j < pages_per_tree; j++) {
            span<const uint8_t> sp;

            if (j == 0) {
                sp = span((const uint8_t*)get_phys_address(*ent) + offsetof(btrfs_header, fsid),
                          PAGE_SIZE - offsetof(btrfs_header, fsid));
            } else
                sp = span((const uint8_t*)get_phys_address(*ent), PAGE_SIZE);

            crc32 = calc_crc32c(crc32, sp);

            if (j != pages_per_tree - 1) {
                lock_guard lock(cache.base.cache_lock);
                ent = cache.next_page_cache_entry(*ent);
            }
        }

        *(uint32_t*)&th = ~crc32;
    }

    for (unsigned int i = 0; i < c.ci.num_stripes; i++) {
        auto ent = first_page;
        uint64_t disk_addr = addr - c.offset + c.ci.stripe[i].offset;
        page_cache_entry* dest_p = nullptr;

        rangelock_req rlr(vol->cache.base.rl, disk_addr >> PAGE_SHIFT, pages_per_tree, rangelock_mode::exclusive);

        vol->cache.pin(disk_addr >> PAGE_SHIFT, pages_per_tree, nullptr);

        for (unsigned j = 0; j < pages_per_tree; j++) {
            assert(ent->tree.key == (addr >> PAGE_SHIFT) + j);

            if (ent->dirty) {
                {
                    lock_guard lock(vol->cache.base.cache_lock);

                    if (!dest_p)
                        dest_p = vol->cache.find_page_cache_entry((disk_addr >> PAGE_SHIFT) + j);
                    else
                        dest_p = vol->cache.next_page_cache_entry(*dest_p);
                }

                if (dest_p->pfn)
                    free_folio(dest_p->pfn << PAGE_SHIFT);

                increase_page_refcount(ent->pfn);
                dest_p->pfn = ent->pfn;
            } else
                dest_p = nullptr;

            if (j != pages_per_tree - 1) {
                lock_guard lock(cache.base.cache_lock);
                ent = cache.next_page_cache_entry(*ent);
            }

        }

        vol->cache.unpin(disk_addr >> PAGE_SHIFT, pages_per_tree);
    }

    auto ent = first_page;
    for (unsigned j = 0; j < pages_per_tree; j++) {
        ent->dirty = 0;

        if (j != pages_per_tree - 1) {
            lock_guard lock(cache.base.cache_lock);
            ent = cache.next_page_cache_entry(*ent);
        }
    }

    cache.unpin(addr >> PAGE_SHIFT, pages_per_tree);

    return true;
}

nt_status btrfs_filesystem::flush_data(const chunk& c, uint64_t addr, uint64_t length) {
    uint64_t num_pages = length >> PAGE_SHIFT;

    if (c.ci.type & BTRFS_BLOCK_GROUP_RAID0) {
        print("RAID0 not supported\n");
        return nt_status::NOT_IMPLEMENTED;
    } else if (c.ci.type & BTRFS_BLOCK_GROUP_RAID10) {
        print("RAID10 not supported\n");
        return nt_status::NOT_IMPLEMENTED;
    } else if (c.ci.type & BTRFS_BLOCK_GROUP_RAID5) {
        print("RAID5 not supported\n");
        return nt_status::NOT_IMPLEMENTED;
    } else if (c.ci.type & BTRFS_BLOCK_GROUP_RAID6) {
        print("RAID6 not supported\n");
        return nt_status::NOT_IMPLEMENTED;
    }

    // chunk is SINGLE, DUP, RAID1, RAID1C3, or RAID1C4

    // FIXME - handle missing devices

    // copy into page cache of volumes

    cache.pin(addr >> PAGE_SHIFT, num_pages, nullptr);

    page_cache_entry* first_page;

    {
        lock_guard lock(cache.base.cache_lock);
        first_page = cache.find_page_cache_entry(addr >> PAGE_SHIFT);
    }

    for (unsigned int i = 0; i < c.ci.num_stripes; i++) {
        auto ent = first_page;
        page_cache_entry* dest_p = nullptr;
        uint64_t disk_addr = addr - c.offset + c.ci.stripe[i].offset;

        rangelock_req rlr(vol->cache.base.rl, disk_addr >> PAGE_SHIFT, num_pages, rangelock_mode::exclusive);

        vol->cache.pin(disk_addr >> PAGE_SHIFT, num_pages, nullptr);

        for (uint64_t j = 0; j < num_pages; j++) {
            assert(ent->tree.key == (addr >> PAGE_SHIFT) + j);

            if (ent->dirty) {
                {
                    lock_guard lock(vol->cache.base.cache_lock);

                    if (!dest_p)
                        dest_p = vol->cache.find_page_cache_entry((disk_addr >> PAGE_SHIFT) + j);
                    else
                        dest_p = vol->cache.next_page_cache_entry(*dest_p);
                }

                if (dest_p->pfn)
                    free_folio(dest_p->pfn << PAGE_SHIFT);

                increase_page_refcount(ent->pfn);
                dest_p->pfn = ent->pfn;
            } else
                dest_p = nullptr;

            if (j != num_pages - 1) {
                lock_guard lock(cache.base.cache_lock);
                ent = cache.next_page_cache_entry(*ent);
            }
        }

        vol->cache.unpin(disk_addr >> PAGE_SHIFT, num_pages);
    }

    auto ent = first_page;
    for (uint64_t j = 0; j < num_pages; j++) {
        ent->dirty = 0;

        if (j != num_pages - 1) {
            lock_guard lock(cache.base.cache_lock);
            ent = cache.next_page_cache_entry(*ent);
        }
    }

    cache.unpin(addr >> PAGE_SHIFT, num_pages);

    return nt_status::SUCCESS;
}

nt_status btrfs_filesystem::flush_cache(uint64_t page_num, uint64_t num_pages) {
    unsigned int pages_per_tree = sb->nodesize >> PAGE_SHIFT;
    uint64_t addr = page_num << PAGE_SHIFT;

    // find chunk

    auto pc = find_chunk(*this, addr);

    if (!pc) {
        print("Could not find chunk for logical address {:x}\n", addr);
        return nt_status::INTERNAL_ERROR;
    }

    auto& c = *pc;

    if (c.ci.type & (BTRFS_BLOCK_GROUP_METADATA | BTRFS_BLOCK_GROUP_SYSTEM)) {
        uint64_t pages_written = 0;

        if (page_num % pages_per_tree || num_pages % pages_per_tree) // not aligned to tree
            return nt_status::INVALID_PARAMETER;

        while (addr < c.offset + c.ci.length) {
            if (!flush_tree(c, addr)) // FIXME - should return nt_status
                return nt_status::INTERNAL_ERROR;

            addr += sb->nodesize;
            pages_written += pages_per_tree;

            if (pages_written >= num_pages)
                break;
        }
    } else {
        auto status = flush_data(c, addr, num_pages << PAGE_SHIFT);
        if (!success(status)) {
            print("btrfs_filesystem::flush_data returned {}\n", status);
            return status;
        }
    }

    uint64_t pages_to_flush = num_pages;
    for (unsigned int i = 0; i < c.ci.num_stripes; i++) {
        uint64_t disk_addr = (page_num << PAGE_SHIFT) - c.offset + c.ci.stripe[i].offset;
        uint64_t first_page = disk_addr >> PAGE_SHIFT;

        auto status = vol->flush_cache(first_page, pages_to_flush);

        if (!success(status)) {
            print("volume::flush_cache returned {}\n", status);
            return status;
        }
    }

    return nt_status::SUCCESS;
}

void btrfs_filesystem::free_chunk_list() {
    while (!is_list_empty(chunk_list)) {
        auto& c = containing_record<chunk>(chunk_list.next);

        remove_entry_list(c.le);
        free(&c);
    }
}

void btrfs_filesystem::free_root_list() {
    while (!is_list_empty(root_list)) {
        auto& r = containing_record<root>(root_list.next);

        remove_entry_list(r.le);
        free(&r);
    }
}

void btrfs_filesystem::free_dev_space_list() {
    while (!is_list_empty(dev_space_list)) {
        auto& s = containing_record<chunk_space>(dev_space_list.next);

        remove_entry_list(s.le);
        free(&s);
    }
}

static nt_status find_last_inode_number(btrfs_filesystem& fs, root& r) {
    btrfs_path p(fs.sb->nodesize);
    bool done = false;

    auto status = fs.find_item(r, {BTRFS_LAST_FREE_OBJECTID, key_type::INODE_ITEM, 0},
                               p, false);
    if (!success(status))
        return status;

    do {
        auto& th = *(btrfs_header*)p.data[0];
        auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

        if (th.nritems == 0)
            break;

        if (p.slots[0] == th.nritems)
            p.slots[0] = th.nritems - 1;

        const auto& it = items[p.slots[0]];
        const auto& k = it.key;

        if (k.type == key_type::INODE_ITEM) {
            r.next_inode_number = k.objectid + 1;
            return nt_status::SUCCESS;
        }

        status = fs.move_path_back(p, r, false);
        if (status == nt_status::NOT_FOUND)
            break;
        else if (!success(status))
            return status;
    } while (true);

    if (!done)
        r.next_inode_number = BTRFS_FIRST_FREE_OBJECTID;

    return nt_status::SUCCESS;
}

bool btrfs_filesystem::load_roots() {
    list_entry new_root_list = { &new_root_list, &new_root_list };

    btrfs_path p(sb->nodesize);

    auto status = find_item(*root_root, {0, key_type::ROOT_ITEM, 0}, p, false);
    if (!success(status))
        return false;

    bool fail = false;

    do {
        auto& th = *(btrfs_header*)p.data[0];
        auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

        if (p.slots[0] >= th.nritems)
            break;

        const auto& it = items[p.slots[0]];
        const auto& k = it.key;

        if (k.type != key_type::ROOT_ITEM) {
            status = move_path_forward(p, *root_root, false);

            if (status == nt_status::NOT_FOUND)
                break;
            else if (!success(status))
                return false; // FIXME

            continue;
        }

        auto data = span((const uint8_t*)&th + sizeof(btrfs_header) + it.offset, it.size);

        if (data.size() < sizeof(btrfs_root_item)) {
            print("Root item truncated\n");
            fail = true;
            break;
        }

        auto rptr = new root;
        if (!rptr) {
            print("new failed\n");
            fail = true;
            break;
        }

        auto& r = *rptr;

        r.id = k.objectid;
        memcpy(&r.ri, data.data(), sizeof(btrfs_root_item));
        insert_tail_list(new_root_list, r.le);

        if (r.id == BTRFS_EXTENT_TREE_OBJECTID)
            extent_root = &r;
        else if (r.id == BTRFS_DEV_TREE_OBJECTID)
            dev_root = &r;
        else if (r.id == BTRFS_FS_TREE_OBJECTID)
            fs_root = &r;
        else if (r.id == BTRFS_CSUM_TREE_OBJECTID)
            csum_root = &r;
        else if (r.id == BTRFS_FREE_SPACE_TREE_OBJECTID)
            free_space_root = &r;
        else if (sb->compat_ro_flags & BTRFS_FEATURE_COMPAT_RO_BLOCK_GROUP_TREE &&
                 r.id == BTRFS_BLOCK_GROUP_TREE_OBJECTID) {
            block_group_root = &r;
        }

        status = move_path_forward(p, *root_root, false);

        if (status == nt_status::NOT_FOUND)
            break;
        else if (!success(status)) {
            fail = true;
            break;
        }
    } while (true);

    if (!extent_root) {
        print("no extent root\n");
        fail = true;
    } else if (!dev_root) {
        print("no dev root\n");
        fail = true;
    } else if (!fs_root) {
        print("no fs root\n");
        fail = true;
    } else if (!csum_root) {
        print("no csum root\n");
        fail = true;
    } else if (!free_space_root) {
        print("no free space root\n");
        fail = true;
    } else if (!block_group_root && sb->compat_ro_flags & BTRFS_FEATURE_COMPAT_RO_BLOCK_GROUP_TREE) {
        print("no block group root\n");
        fail = true;
    }

    if (fail) {
        swap_lists(root_list, new_root_list);
        free_root_list();
        swap_lists(root_list, new_root_list);
        return false;
    }

    if (!(sb->compat_ro_flags & BTRFS_FEATURE_COMPAT_RO_BLOCK_GROUP_TREE))
        block_group_root = extent_root;

    remove_entry_list(root_root->le);
    insert_head_list(new_root_list, root_root->le);

    remove_entry_list(chunk_root->le);
    insert_head_list(new_root_list, chunk_root->le);

    swap_lists(root_list, new_root_list);

    return true;
}

btrfs_inode::btrfs_inode(btrfs_filesystem& fs, uint64_t subvol, uint64_t inode, enum btrfs_inode_type inode_type) : ::inode(fs), fs(fs) {
    this->tree.key = inode;

    r = fs.find_root(subvol);
    if (!r)
        return;

    switch (inode_type) {
        case btrfs_inode_type::file:
            this->inode_type = inode_type::file;
            break;

        case btrfs_inode_type::directory:
            this->inode_type = inode_type::directory;
            break;

        case btrfs_inode_type::chardev:
            this->inode_type = inode_type::chardev;
            break;

        case btrfs_inode_type::blockdev:
            this->inode_type = inode_type::blockdev;
            break;

        case btrfs_inode_type::fifo:
            this->inode_type = inode_type::fifo;
            break;

        case btrfs_inode_type::socket:
            this->inode_type = inode_type::socket;
            break;

        case btrfs_inode_type::symlink:
            this->inode_type = inode_type::symlink;
            break;

        case btrfs_inode_type::ea:
            this->inode_type = inode_type::ea;
            break;

        default:
            this->inode_type = inode_type::unknown;
            break;
    }
}

static uint64_t btrfs_time_to_system_time(const btrfs_timespec& t) {
    uint64_t ret;

    ret = t.sec;
    ret += 11644473600;
    ret *= 10000000;
    ret += (uint64_t)t.nsec / 100;

    return ret;
}

static btrfs_timespec system_time_to_btrfs_time(uint64_t t) {
    btrfs_timespec ret;

    ret.nsec = (uint32_t)((t % 10000000) * 100);
    t /= 10000000;

    ret.sec = t - 11644473600;

    return ret;
}

bool btrfs_inode::load_inode() {
    nt_status status;
    bool found_inode_item = false;
    btrfs_path p(fs.sb->nodesize);

    status = fs.find_item(*r, {tree.key, key_type::INODE_ITEM, 0}, p, false);
    if (!success(status)) // FIXME - return this somehow
        return false;

    do {
        auto& th = *(btrfs_header*)p.data[0];
        auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

        if (p.slots[0] >= th.nritems)
            break;

        const auto& it = items[p.slots[0]];
        const auto& k = it.key;

        if (k.objectid != tree.key)
            break;

        auto data = span((const uint8_t*)&th + sizeof(btrfs_header) + it.offset, it.size);

        switch (k.type) {
            case key_type::INODE_ITEM:
                if (data.size() < sizeof(btrfs_inode_item)) {
                    print("INODE_ITEM was short\n");
                    break;
                }

                memcpy(&ii, data.data(), min(data.size(), sizeof(btrfs_inode_item)));
                found_inode_item = true;
            break;

            case key_type::EXTENT_DATA: {
                auto& ed = *(btrfs_file_extent_item*)data.data();

                if (data.size() < offsetof(btrfs_file_extent_item, disk_bytenr)) {
                    print("EXTENT_DATA was short\n");
                    break;
                }

                if (ed.compression != 0) {
                    print("Compression not supported\n");
                    break;
                }

                if (ed.encryption != 0) {
                    print("Encryption not supported\n");
                    break;
                }

                if (ed.other_encoding != 0) {
                    print("Other encodings not supported\n");
                    break;
                }

                if (ed.type == btrfs_extent_type::regular || ed.type == btrfs_extent_type::prealloc) {
                    if (data.size() < sizeof(btrfs_file_extent_item)) {
                        print("EXTENT_DATA was short\n");
                        break;
                    }
                } else if (ed.type == btrfs_extent_type::inline_extent) {
                    if (data.size() < offsetof(btrfs_file_extent_item, disk_bytenr) + ed.ram_bytes) {
                        print("EXTENT_DATA was short\n");
                        break;
                    }
                } else {
                    print("Invalid extent type\n");
                    break;
                }

                if (ed.type == btrfs_extent_type::regular) {
                    if (ed.disk_bytenr == 0 || ed.disk_num_bytes == 0) // skip sparse extents
                        break;
                }

                auto ieptr = (inode_extent*)malloc(offsetof(inode_extent, ed) + data.size());
                if (!ieptr) {
                    print("malloc failed\n");
                    break;
                }

                auto& ie = *ieptr;

                ie.offset = k.offset;
                memcpy(&ie.ed, data.data(), data.size());
                insert_tail_list(extents, ie.le);

                break;
            }

            default:
                break;
        }

        status = fs.move_path_forward(p, *r, false);
        if (status == nt_status::NOT_FOUND)
            break;
        else if (!success(status))
            return false; // FIXME
    } while (true);

    if (!found_inode_item) {
        print("INODE_ITEM for subvol {:x}, inode {:x} not found.\n", r->id, tree.key);
        return false;
    }

    file_size = ii.size;
    mtime = btrfs_time_to_system_time(ii.mtime);
    ctime = btrfs_time_to_system_time(ii.ctime);

    if (this->inode_type == inode_type::unknown) {
        switch ((ii.mode & 0xf000) >> 12) {
            case 1:
                this->inode_type = inode_type::fifo;
                break;

            case 2:
                this->inode_type = inode_type::chardev;
                break;

            case 4:
                this->inode_type = inode_type::directory;
                break;

            case 6:
                this->inode_type = inode_type::blockdev;
                break;

            case 8:
                this->inode_type = inode_type::file;
                break;

            case 0xa:
                this->inode_type = inode_type::symlink;
                break;

            case 0xc:
                this->inode_type = inode_type::socket;
                break;

            default:
                break;
        }
    }

    return true;
}

btrfs_inode::~btrfs_inode() {
    while (!is_list_empty(extents)) {
        auto& ie = containing_record<inode_extent>(extents.next);

        remove_entry_list(ie.le);
        free(&ie);
    }
}

void btrfs_inode::release() {
    {
        lock_guard lock(r->inode_lock);

        if (--refcount != 0)
            return;

        remove_from_rbtree(r->inodes, tree);
    }

    delete this;
}

root* btrfs_filesystem::find_root(uint64_t root_num) {
    auto le = root_list.next;
    while (le != &root_list) {
        auto& r = containing_record<root>(le);

        if (r.id == root_num)
            return &r;

        le = le->next;
    }

    return nullptr;
}

static nt_status remove_from_space_list(chunk& c, uint64_t addr, uint64_t size) {
    for (auto le = c.space_list.next; le != &c.space_list; le = le->next) {
        auto& s = containing_record<chunk_space>(le);

        if (s.offset + s.length <= addr)
            continue;

        if (s.offset >= addr + size)
            break;

        if (addr <= s.offset && addr + size >= s.offset + s.length) { // remove whole thing
            auto prev = le->prev;

            remove_entry_list(s.le);

            if (addr + size == s.offset + s.length) {
                delete &s;
                break;
            }

            size -= s.offset + s.length - addr;
            addr = s.offset + s.length;

            delete &s;

            le = prev;
            continue;
        } else if (addr <= s.offset) { // remove start
            s.length = s.offset + s.length - addr - size;
            s.offset = addr + size;

            break;
        } else if (addr + size >= s.offset + s.length) { // remove end
            auto end = s.offset + s.length;

            s.length = addr - s.offset;

            size -= end - addr;
            addr = end;

            if (size == 0)
                break;
        } else { // poke a hole
            auto cs = new chunk_space;
            if (!cs) {
                print("new failed\n");
                return nt_status::INSUFFICIENT_RESOURCES;
            }

            cs->offset = addr + size;
            cs->length = s.offset + s.length - addr - size;

            s.length = addr - s.offset;

            insert_head_list(s.le, cs->le);

            break;
        }
    }

    return nt_status::SUCCESS;
}

static nt_status protect_superblocks(chunk& c) {
    nt_status status;

    for (auto addr : superblock_addrs) {
        for (uint16_t i = 0; i < c.ci.num_stripes; i++) {
            auto& s = c.ci.stripe[i];

            if (s.offset <= addr + 0x10000 && s.offset + c.ci.length >= addr) {
                uint64_t start = max(addr, s.offset) - s.offset + c.offset;
                uint64_t end = min(addr + 0x10000, s.offset + c.ci.length) - s.offset + c.offset;

                status = remove_from_space_list(c, start, end - start);
                if (!success(status))
                    return status;
            }
        }
    }

    return nt_status::SUCCESS;
}

static nt_status load_space_bitmap(btrfs_filesystem& fs, uint64_t start,
                                   span<const uint8_t> data, list_entry& list) {
    bool in_run = false;
    size_t run_start, run_length;

    if (!is_list_empty(list)) {
        auto& s = containing_record<chunk_space>(list.prev);

        if (s.offset + s.length == start && !data.empty() && data[0] & 1) {
            in_run = true;
            run_start = s.offset;
            run_length = s.length / fs.sb->sectorsize;

            remove_entry_list(s.le);
            free(&s);
        }
    }

    for (size_t i = 0; i < data.size(); i++) {
        if (!in_run && data[i] == 0)
            continue;
        else if (in_run && data[i] == 0xff) {
            run_length += 8;
            continue;
        }

        auto v = data[i];
        for (unsigned int j = 0; j < 8; j++) {
            if (in_run) {
                if (v & 1)
                    run_length++;
                else {
                    auto s = (chunk_space*)malloc(sizeof(chunk_space));
                    if (!s) {
                        print("malloc failed\n");
                        return nt_status::INSUFFICIENT_RESOURCES;
                    }

                    s->offset = run_start;
                    s->length = run_length * fs.sb->sectorsize;
                    insert_tail_list(list, s->le);

                    in_run = false;
                }
            } else {
                if (v & 1) {
                    in_run = true;
                    run_start = start + (((i * 8) + j) * fs.sb->sectorsize);
                    run_length = 1;
                }
            }

            v >>= 1;
        }
    }

    if (in_run) {
        auto s = (chunk_space*)malloc(sizeof(chunk_space));
        if (!s) {
            print("malloc failed\n");
            return nt_status::INSUFFICIENT_RESOURCES;
        }

        s->offset = run_start;
        s->length = run_length * fs.sb->sectorsize;
        insert_tail_list(list, s->le);
    }

    return nt_status::SUCCESS;
}

static nt_status load_space(btrfs_filesystem& fs, chunk& c) {
    lock_guard lock(c.space_loading_lock);

    if (c.space_loaded)
        return nt_status::SUCCESS;

    auto& r = *fs.free_space_root;
    auto search_key = btrfs_key{c.offset, key_type::FREE_SPACE_INFO, c.ci.length};
    btrfs_path p(fs.sb->nodesize);

    auto status = fs.find_item(r, search_key, p, false);
    if (!success(status))
        return status;

    {
        auto& th = *(btrfs_header*)p.data[0];
        auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

        if (p.slots[0] >= th.nritems) {
            print("Could not find free_space_info for chunk {:x}.\n", c.offset);
            return nt_status::DISK_CORRUPT_ERROR;
        }

        const auto& it = items[p.slots[0]];
        const auto& k = it.key;

        if (k != search_key) {
            print("Could not find free_space_info for chunk {:x}.\n", c.offset);
            return nt_status::DISK_CORRUPT_ERROR;
        }

        auto data = span((const uint8_t*)&th + sizeof(btrfs_header) + it.offset, it.size);
        auto& fsi = *(btrfs_free_space_info*)data.data();

        c.using_fst_bitmaps = !!(fsi.flags & BTRFS_FREE_SPACE_USING_BITMAPS);
    }

    do {
        status = fs.move_path_forward(p, r, false);

        if (status == nt_status::NOT_FOUND)
            break;
        else if (!success(status))
            return status;

        auto& th = *(btrfs_header*)p.data[0];
        auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

        const auto& it = items[p.slots[0]];
        const auto& k = it.key;

        if (k.objectid >= c.offset + c.ci.length)
            break;

        switch (k.type) {
            case key_type::FREE_SPACE_EXTENT: {
                bool done = false;

                if (!is_list_empty(c.space_list)) {
                    auto& s = containing_record<chunk_space>(c.space_list.prev);

                    if (s.offset + s.length == k.objectid) {
                        s.length += k.offset;
                        done = true;
                    }
                }

                if (!done) {
                    auto s = (chunk_space*)malloc(sizeof(chunk_space));
                    if (!s) {
                        print("malloc failed\n");
                        return nt_status::INSUFFICIENT_RESOURCES;
                    }

                    // FIXME - these should be in some sort of tree rather than a linked list

                    s->offset = k.objectid;
                    s->length = k.offset;
                    insert_tail_list(c.space_list, s->le);
                }

                break;
            }

            case key_type::FREE_SPACE_BITMAP: {
                auto data = span((const uint8_t*)&th + sizeof(btrfs_header) + it.offset, it.size);

                status = load_space_bitmap(fs, k.objectid, data, c.space_list);
                if (!success(status))
                    return status;

                break;
            }

            default:
                break;
        }
    } while (true);

    status = protect_superblocks(c);
    if (!success(status))
        return status;

    c.space_loaded = true;

    return nt_status::SUCCESS;
}

static nt_status allocate_in_chunk(btrfs_filesystem& fs, chunk& c,
                                   uint64_t size, uint64_t& addr) {
    if (!c.space_loaded) {
        auto status = load_space(fs, c);
        if (!success(status))
            return status;
    }

    // avoid overwriting previous metadata by following on from last allocation,
    // wherever possible
    if (c.last_allocation_end != 0) {
        for (auto le = c.space_list.next; le != &c.space_list; le = le->next) {
            auto& s = containing_record<chunk_space>(le);

            if (s.offset + s.length < c.last_allocation_end)
                continue;

            if (s.offset <= c.last_allocation_end && s.offset + s.length >= c.last_allocation_end + size) {
                if (s.offset == c.last_allocation_end && s.length == size) {
                    // remove whole thing
                    remove_entry_list(s.le);
                    free(&s);
                } else if (s.offset == c.last_allocation_end) {
                    // remove beginning
                    s.offset += size;
                    s.length -= size;
                } else if (s.offset + s.length == c.last_allocation_end + size) {
                    // remove end
                    s.length -= size;
                } else {
                    // remove middle

                    auto s2 = new chunk_space;
                    if (!s2)
                        return nt_status::INSUFFICIENT_RESOURCES;

                    s2->offset = c.last_allocation_end + size;
                    s2->length = s.offset + s.length - s2->offset;
                    insert_head_list(s.le, s2->le);

                    s.length = c.last_allocation_end - s.offset;
                }

                addr = c.last_allocation_end;
                c.last_allocation_end = addr + size;

                return nt_status::SUCCESS;
            }

            if (s.length < size)
                continue;

            addr = s.offset;

            if (s.length == size) {
                remove_entry_list(s.le);
                free(&s);
            } else {
                s.offset += size;
                s.length -= size;
            }

            c.last_allocation_end = addr + size;

            return nt_status::SUCCESS;
        }
    }

    for (auto le = c.space_list.next; le != &c.space_list; le = le->next) {
        auto& s = containing_record<chunk_space>(le);

        if (s.length >= size) {
            addr = s.offset;

            if (s.length == size) {
                remove_entry_list(s.le);
                free(&s);
            } else {
                s.offset += size;
                s.length -= size;
            }

            c.last_allocation_end = addr + size;

            return nt_status::SUCCESS;
        }
    }

    return nt_status::DISK_FULL;
}

static uint32_t leaf_nritems(const btrfs_path& p) {
    auto& th = *(btrfs_header*)p.data[0];

    return th.nritems;
}

static nt_status next_leaf(btrfs_filesystem& fs, btrfs_path& p, root& r,
                           bool cow) {
    nt_status status;

    uint8_t level = 1;

    while (level < BTRFS_MAX_LEVEL) {
        if (!p.data[level])
            return nt_status::NOT_FOUND;

        auto& th = *(btrfs_header*)p.data[level];

        if (p.slots[level] + 1 >= th.nritems) {
            level++;
            if (level == BTRFS_MAX_LEVEL)
                return nt_status::NOT_FOUND;

            continue;
        }

        p.slots[level]++;

        break;
    }

    while (true) {
        auto& th = *(btrfs_header*)p.data[level];
        auto items = span((btrfs_key_ptr*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

        status = fs.read_metadata(items[p.slots[level]].blockptr,
                                  span(p.data[level - 1], fs.sb->nodesize));
        if (!success(status))
            return status;

        level--;
        p.slots[level] = 0;

        if (level == 0)
            break;
    }

    if (cow) {
        status = cow_tree(fs, r, p);
        if (!success(status))
            return status;
    }

    return nt_status::SUCCESS;
}

static nt_status find_single_hole_for_chunk(btrfs_filesystem& fs, uint64_t type,
                                            uint64_t& addr, uint64_t& hole_size) {
    // FIXME - allow short allocations

    uint64_t desired_size;

    if (type & BTRFS_BLOCK_GROUP_SYSTEM)
        desired_size = 0x2000000; // 32 MB
    else
        desired_size = 0x40000000; // 1 GB (FIXME - Linux caps at 10% of volume)

    for (auto le = fs.dev_space_list.next; le != &fs.dev_space_list; le = le->next) {
        auto& s = containing_record<chunk_space>(le);

        if (s.length < desired_size)
            continue;

        if (s.length == desired_size) {
            addr = s.offset;
            hole_size = desired_size;

            remove_entry_list(s.le);
            free(&s);
        } else {
            addr = s.offset;
            hole_size = desired_size;

            s.offset += hole_size;
            s.length -= hole_size;
        }

        return nt_status::SUCCESS;
    }

    return nt_status::DISK_FULL;
}

static nt_status find_dup_holes_for_chunk(btrfs_filesystem& fs, uint64_t type,
                                          uint64_t& addr1, uint64_t& addr2,
                                          uint64_t& hole_size) {
    // FIXME - allow short allocations

    uint64_t desired_size;

    if (type & BTRFS_BLOCK_GROUP_SYSTEM)
        desired_size = 0x2000000; // 32 MB
    else
        desired_size = 0x40000000; // 1 GB (FIXME - Linux caps at 10% of volume)

    // find one big hole

    for (auto le = fs.dev_space_list.next; le != &fs.dev_space_list; le = le->next) {
        auto& s = containing_record<chunk_space>(le);

        if (s.length < 2 * desired_size)
            continue;

        if (s.length == 2 * desired_size) {
            addr1 = s.offset;
            addr2 = s.offset + desired_size;
            hole_size = desired_size;

            remove_entry_list(s.le);
            free(&s);
        } else {
            addr1 = s.offset;
            addr2 = s.offset + desired_size;
            hole_size = desired_size;

            s.offset += 2 * hole_size;
            s.length -= 2 * hole_size;
        }

        return nt_status::SUCCESS;
    }

    // find two separate holes

    chunk_space* s1 = nullptr;

    for (auto le = fs.dev_space_list.next; le != &fs.dev_space_list; le = le->next) {
        auto& s = containing_record<chunk_space>(le);

        if (s.length < desired_size)
            continue;

        if (!s1) {
            s1 = &s;
            continue;
        }

        addr1 = s1->offset;
        addr2 = s.offset;
        hole_size = desired_size;

        if (s1->length == desired_size) {
            remove_entry_list(s1->le);
            free(s1);
        } else {
            s1->offset += hole_size;
            s1->length -= hole_size;
        }

        if (s.length == desired_size) {
            remove_entry_list(s.le);
            free(&s);
        } else {
            s.offset += hole_size;
            s.length -= hole_size;
        }

        return nt_status::SUCCESS;
    }

    return nt_status::DISK_FULL;
}

static uint64_t get_next_chunk_offset(btrfs_filesystem& fs) {
    auto& c = containing_record<chunk>(fs.chunk_list.prev);

    return c.offset + c.ci.length;
}

static nt_status update_dev_item_bytes_used(btrfs_filesystem& fs, uint64_t devid,
                                            int64_t delta) {
    nt_status status;
    btrfs_path p(fs.sb->nodesize);

    status = fs.find_item(*fs.chunk_root, {1, key_type::DEV_ITEM, devid},
                          p, true);
    if (!success(status))
        return status;

    auto& th = *(btrfs_header*)p.data[0];
    auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

    if (p.slots[0] == th.nritems)
        return nt_status::DISK_CORRUPT_ERROR;

    auto& it = items[p.slots[0]];
    const auto& found_key = it.key;

    if (found_key.objectid != 1 || found_key.type != key_type::DEV_ITEM || found_key.offset != devid)
        return nt_status::DISK_CORRUPT_ERROR;

    auto data = span((const uint8_t*)&th + sizeof(btrfs_header) + it.offset, it.size);

    if (data.size() < sizeof(btrfs_dev_item))
        return nt_status::DISK_CORRUPT_ERROR;

    auto& di = *(btrfs_dev_item*)data.data();

    di.bytes_used += delta;

    if (!fs.cache.write(span(p.data[0], fs.sb->nodesize), th.bytenr)) // FIXME - return status
        return nt_status::INTERNAL_ERROR;

    return nt_status::SUCCESS;
}

static nt_status allocate_new_chunk(btrfs_filesystem& fs, chunk*& c,
                                    uint64_t type) {
    nt_status status;
    uint64_t phys_addr1, phys_addr2, chunk_size;
    auto& di = fs.sb->dev_item;

    if (type & (BTRFS_BLOCK_GROUP_RAID0 | BTRFS_BLOCK_GROUP_RAID10 | BTRFS_BLOCK_GROUP_RAID5 | BTRFS_BLOCK_GROUP_RAID6))
        return nt_status::DISK_FULL;

    if (type & BTRFS_BLOCK_GROUP_DUP) {
        status = find_dup_holes_for_chunk(fs, type, phys_addr1, phys_addr2,
                                          chunk_size);
        if (!success(status))
            return status;
    } else {
        status = find_single_hole_for_chunk(fs, type, phys_addr1, chunk_size);
        if (!success(status))
            return status;
    }

    // add new chunk

    uint16_t num_stripes = type & BTRFS_BLOCK_GROUP_DUP ? 2 : 1;

    c = (chunk*)malloc(offsetof(chunk, ci) + offsetof(btrfs_chunk, stripe) + (num_stripes * sizeof(btrfs_stripe)));
    if (!c) {
        print("malloc failed\n");
        // FIXME - put holes back
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    new (c) chunk;

    c->offset = get_next_chunk_offset(fs);

    // FIXME - chunk_list should be in order
    insert_tail_list(fs.chunk_list, c->le);

    c->ci.length = chunk_size;
    c->ci.owner = fs.extent_root->id;
    c->ci.stripe_len = BTRFS_STRIPE_LEN;
    c->ci.type = type;
    c->ci.io_align = (uint32_t)c->ci.stripe_len;
    c->ci.io_width = (uint32_t)c->ci.stripe_len;
    c->ci.sector_size = fs.sb->sectorsize;
    c->ci.num_stripes = num_stripes;
    c->ci.sub_stripes = 1;

    c->ci.stripe[0].devid = di.devid;
    c->ci.stripe[0].offset = phys_addr1;
    c->ci.stripe[0].dev_uuid = di.uuid;

    if (type & BTRFS_BLOCK_GROUP_DUP) {
        c->ci.stripe[1].devid = di.devid;
        c->ci.stripe[1].offset = phys_addr2;
        c->ci.stripe[1].dev_uuid = di.uuid;
    }

    // populate space list for chunk

    auto s = (chunk_space*)malloc(sizeof(chunk_space));
    if (!s) {
        print("malloc failed\n");
        // FIXME - put holes back
        remove_entry_list(c->le);
        c->chunk::~chunk();
        free(c);
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    s->offset = c->offset;
    s->length = chunk_size;
    insert_tail_list(c->space_list, s->le);
    c->space_loaded = true;
    c->using_fst_bitmaps = false;

    status = protect_superblocks(*c);
    if (!success(status)) {
        // FIXME - put holes back
        remove_entry_list(c->le);
        c->chunk::~chunk();
        free(c);
        return status;
    }

    insert_tail_list(fs.new_chunks, c->new_le);

    // FIXME - this should be done by flush thread when multi-device support is in place
    fs.sb->dev_item.bytes_used += num_stripes * chunk_size;

    return nt_status::SUCCESS;
}

static nt_status allocate_metadata(btrfs_filesystem& fs, uint64_t root,
                                   uint64_t& addr) {
    nt_status status;
    bool system = root == BTRFS_CHUNK_TREE_OBJECTID;

    for (auto le = fs.chunk_list.next; le != &fs.chunk_list; le = le->next) {
        auto& c = containing_record<chunk>(le);

        // FIXME - skip if chunk is readonly

        if (!(c.ci.type & (system ? BTRFS_BLOCK_GROUP_SYSTEM : BTRFS_BLOCK_GROUP_METADATA)))
            continue;

        // skip unsupported RAID types
        if (c.ci.type & (BTRFS_BLOCK_GROUP_RAID0 | BTRFS_BLOCK_GROUP_RAID10 | BTRFS_BLOCK_GROUP_RAID5 | BTRFS_BLOCK_GROUP_RAID6))
            continue;

        status = allocate_in_chunk(fs, c, fs.sb->nodesize, addr);
        if (status != nt_status::DISK_FULL)
            return status;
    }

    chunk* c;

    status = allocate_new_chunk(fs, c, system ? fs.system_profile : fs.metadata_profile);
    if (!success(status))
        return status;

    return allocate_in_chunk(fs, *c, fs.sb->nodesize, addr);
}

static void add_to_space_list(chunk& c, uint64_t addr, uint64_t size) {
    for (auto le = c.space_list.next; le != &c.space_list; le = le->next) {
        auto& s = containing_record<chunk_space>(le);

        if (s.offset >= addr + size) {
            if (s.offset == addr + size) { // move beginning backwards
                s.length += size;
                s.offset = addr;

                while (s.le.prev != &c.space_list) {
                    auto& prev = containing_record<chunk_space>(s.le.prev);

                    if (prev.offset + prev.length == s.offset) {
                        s.length += prev.length;
                        s.offset = prev.offset;
                        remove_entry_list(prev.le);
                        delete &prev;
                    } else
                        break;
                }

                return;
            }

            // insert at very beginning
            if (s.le.prev == &c.space_list) {
                auto cs = new chunk_space;
                if (!cs)
                    return; // FIXME - return nt_status::INSUFFICIENT_RESOURCES

                cs->offset = addr;
                cs->length = size;
                insert_head_list(c.space_list, cs->le);
                return;
            }

            auto& prev = containing_record<chunk_space>(s.le.prev);

            // extend previous, and bridge if we can
            if (prev.offset + prev.length == addr) {
                prev.length += size;

                if (prev.offset + prev.length == s.offset) {
                    prev.length += s.length;
                    remove_entry_list(s.le);
                    delete &s;
                }

                return;
            }

            // insert new entry
            auto cs = new chunk_space;
            if (!cs)
                return; // FIXME - return nt_status::INSUFFICIENT_RESOURCES

            cs->offset = addr;
            cs->length = size;
            insert_head_list(prev.le, cs->le);

            return;
        }
    }

    // add at very end

    auto cs = new chunk_space;
    if (!cs)
        return; // FIXME - return nt_status::INSUFFICIENT_RESOURCES

    cs->offset = addr;
    cs->length = size;
    insert_tail_list(c.space_list, cs->le);
}

static void handle_pending_space(chunk& c) {
    while (!is_list_empty(c.pending_space_list)) {
        auto& cs = containing_record<chunk_space>(c.pending_space_list.next);

        add_to_space_list(c, cs.offset, cs.length);

        remove_entry_list(cs.le);
        delete &cs;
    }
}

static nt_status add_metadata_change(btrfs_filesystem& fs, uint64_t address,
                                     uint8_t level, uint64_t root, bool remove) {
    int32_t count = remove ? -1 : 1;

    {
        lock_guard lock(fs.ref_changes_lock);

        if (fs.ref_changes) {
            auto n = find_rbtree_node(fs.ref_changes, address);

            if (n && n->key == address) {
                auto& rc = rbtree_containing_record<ref_change>(n);

                assert(rc.is_metadata);
                assert(rc.metadata.root == root); // FIXME?
                assert(rc.metadata.level == level);

                rc.metadata.count += count;

                return nt_status::SUCCESS;
            }
        }

        auto rc = (ref_change*)malloc(sizeof(ref_change));
        if (!rc)
            return nt_status::INSUFFICIENT_RESOURCES;

        rc->tree.key = address;
        rc->is_metadata = true;
        rc->metadata.root = root;
        rc->metadata.level = level;
        rc->metadata.count = count;

        insert_into_rbtree(fs.ref_changes, rc->tree);
    }

    return nt_status::SUCCESS;
}

static void change_metadata_level(btrfs_filesystem& fs, uint64_t address,
                                  uint8_t new_level) {
    lock_guard lock(fs.ref_changes_lock);

    assert(fs.ref_changes);

    auto n = find_rbtree_node(fs.ref_changes, address);

    assert(n && n->key == address);

    auto& rc = rbtree_containing_record<ref_change>(n);

    assert(rc.is_metadata);

    rc.metadata.level = new_level;
}

static nt_status cow_tree(btrfs_filesystem& fs, root& r, btrfs_path& p) {
    // FIXME - updating free space tree

    for (uint8_t i = 0; i < BTRFS_MAX_LEVEL; i++) {
        uint64_t addr;

        if (!p.data[i])
            break;

        auto& th = *(btrfs_header*)p.data[i];

        if (th.flags & HEADER_FLAG_WRITTEN) {
            if (auto status = allocate_metadata(fs, r.id, addr); !success(status))
                return status;

            auto old_addr = th.bytenr;

            th.flags &= ~HEADER_FLAG_WRITTEN;
            th.bytenr = addr;
            th.generation = fs.sb->generation;

            if (!fs.cache.write(span(p.data[i], fs.sb->nodesize), th.bytenr)) // FIXME - return status
                return nt_status::INTERNAL_ERROR;

            if (i == BTRFS_MAX_LEVEL - 1 || !p.data[i + 1]) { // top
                r.ri.bytenr = addr;
                r.ri.generation = fs.sb->generation;
                r.ri.generation_v2 = fs.sb->generation;

                if (r.id != BTRFS_ROOT_TREE_OBJECTID && !r.is_dirty) {
                    insert_tail_list(fs.dirty_root_list, r.dirty_le);
                    r.is_dirty = true;
                }
            }

            if (auto status = add_metadata_change(fs, addr, i, r.id, false); !success(status))
                return status;

            if (auto status = add_metadata_change(fs, old_addr, i, r.id, true); !success(status))
                return status;
        }

        if (i > 0) {
            auto& prev_th = *(btrfs_header*)p.data[i - 1];
            auto items = span((btrfs_key_ptr*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

            if (items[p.slots[i]].blockptr == prev_th.bytenr)
                break;

            items[p.slots[i]].blockptr = prev_th.bytenr;
            items[p.slots[i]].generation = fs.sb->generation;

            if (!fs.cache.write(span(p.data[i], fs.sb->nodesize), th.bytenr)) // FIXME - return status
                return nt_status::INTERNAL_ERROR;
        }
    }

    return nt_status::SUCCESS;
}

void btrfs_path::reset() {
    for (unsigned int i = 0; i < BTRFS_MAX_LEVEL; i++) {
        if (data[i]) {
            free_folio(phys_memory_pfn(data[i]) << PAGE_SHIFT);
            data[i] = nullptr;
        }
    }
}

nt_status btrfs_filesystem::find_item(root& r, const btrfs_key& k, btrfs_path& p,
                                      bool cow) {
    uint64_t addr = r.ri.bytenr;
    uint8_t level = r.ri.level;
    nt_status status;

    // FIXME - error if level >= BTRFS_MAX_LEVEL

    p.reset();

    for (unsigned int i = 0; i <= level; i++) {
        auto pa = allocate_folio(node_folio_order);
        if (!pa)
            return nt_status::INSUFFICIENT_RESOURCES;

        p.data[i] = (uint8_t*)phys_address(pa);
    }

    do {
        status = read_metadata(addr, span(p.data[level], sb->nodesize));
        if (!success(status))
            return status;

        auto& th = *(btrfs_header*)p.data[level];

        if (th.bytenr != addr) {
            print("btrfs_filesystem::find_item: metadata address was {:x}, expected {:x}\n",
                  +th.bytenr, addr);
            return nt_status::DISK_CORRUPT_ERROR;
        }

        if (th.level != level) {
            print("btrfs_filesystem::find_item: metadata level at {:x} was {:x}, expected {:x}\n",
                  addr, th.level, level);
            return nt_status::DISK_CORRUPT_ERROR;
        }

        if (th.level == 0) {
            auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

            p.slots[level] = (uint32_t)items.size();

            for (uint32_t i = 0; i < items.size(); i++) {
                if (items[i].key >= k) {
                    p.slots[level] = i;
                    break;
                }
            }

            break;
        } else {
            auto items = span((btrfs_key_ptr*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

            p.slots[level] = th.nritems - 1;

            for (uint32_t i = 1; i < items.size(); i++) {
                if (items[i].key == k) {
                    p.slots[level] = i;
                    break;
                } else if (items[i].key > k) {
                    p.slots[level] = i - 1;
                    break;
                }
            }

            addr = items[p.slots[level]].blockptr;
            level--;
        }
    } while (true);

    if (cow) {
        status = cow_tree(*this, r, p);
        if (!success(status))
            return status;
    }

    return nt_status::SUCCESS;
}

static unsigned int find_midpoint(const btrfs_header& th, size_t total,
                                  size_t& sum) {
    auto* items = (btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header));

    sum = 0;

    for (unsigned int i = 0; i < th.nritems; i++) {
        if (sum + sizeof(btrfs_item) + items[i].size > total / 2)
            return i;

        sum += sizeof(btrfs_item) + items[i].size;
    }

    return th.nritems - 1;
}

static nt_status split_internal_tree(btrfs_filesystem& fs, btrfs_path& p,
                                     root& r, uint8_t level) {
    if (level >= BTRFS_MAX_LEVEL - 2)
        return nt_status::INTERNAL_ERROR;

    auto& th = *(btrfs_header*)p.data[level];
    unsigned int split_point = th.nritems / 2;
    uint64_t new_addr;
    nt_status status;

    auto new_tree = (uint8_t*)phys_address(allocate_folio(fs.node_folio_order));
    if (!new_tree)
        return nt_status::INSUFFICIENT_RESOURCES;

    status = allocate_metadata(fs, r.id, new_addr);
    if (!success(status)) {
        free_folio(phys_memory_pfn(new_tree) << PAGE_SHIFT);
        return status;
    }

    auto& th2 = *(btrfs_header*)new_tree;

    memcpy(&th2, &th, sizeof(btrfs_header));
    th2.bytenr = new_addr;
    th2.flags &= ~HEADER_FLAG_WRITTEN;
    th2.nritems = th.nritems - split_point;

    auto* items = (btrfs_key_ptr*)((uint8_t*)&th + sizeof(btrfs_header));
    auto* items2 = (btrfs_key_ptr*)((uint8_t*)&th2 + sizeof(btrfs_header));

    memcpy(items2, &items[split_point], th2.nritems * sizeof(btrfs_key_ptr));
    th.nritems = split_point;

    status = add_metadata_change(fs, new_addr, level, th.owner, false);
    if (!success(status)) {
        free_folio(phys_memory_pfn(new_tree) << PAGE_SHIFT);
        return status;
    }

    if (!fs.cache.write(span((uint8_t*)&th, fs.sb->nodesize), th.bytenr)) { // FIXME - return status
        free_folio(phys_memory_pfn(new_tree) << PAGE_SHIFT);
        return nt_status::INTERNAL_ERROR;
    }

    if (!fs.cache.write(span((uint8_t*)&th2, fs.sb->nodesize), th2.bytenr)) { // FIXME - return status
        free_folio(phys_memory_pfn(new_tree) << PAGE_SHIFT);
        return nt_status::INTERNAL_ERROR;
    }

    if (!p.data[level + 1]) {
        status = add_new_level(fs, p, r, level + 1);
        if (!success(status)) {
            free_folio(phys_memory_pfn(new_tree) << PAGE_SHIFT);
            return status;
        }
    }

    p.slots[level + 1]++;
    status = insert_internal_node(fs, p, r, level + 1, items2[0].key, new_addr);
    if (!success(status)) {
        p.slots[level + 1]--;
        free_folio(phys_memory_pfn(new_tree) << PAGE_SHIFT);
        return status;
    }

    if (p.slots[level] < split_point) {
        p.slots[level + 1]--;
        free_folio(phys_memory_pfn(new_tree) << PAGE_SHIFT);
    } else {
        p.slots[level] -= split_point;
        free_folio(phys_memory_pfn(p.data[level]) << PAGE_SHIFT);
        p.data[level] = new_tree;
    }

    return nt_status::SUCCESS;
}

static nt_status insert_internal_node(btrfs_filesystem& fs, btrfs_path& p,
                                      root& r, uint8_t level, const btrfs_key& k,
                                      uint64_t address) {
    nt_status status;
    size_t max_items = (fs.sb->nodesize - sizeof(btrfs_header)) / sizeof(btrfs_key_ptr);

    {
        auto& th = *(btrfs_header*)p.data[level];

        if (th.nritems == max_items) {
            status = split_internal_tree(fs, p, r, level);
            if (!success(status))
                return status;
        }
    }

    auto& th = *(btrfs_header*)p.data[level];

    auto* items = (btrfs_key_ptr*)((uint8_t*)&th + sizeof(btrfs_header));

    memmove(&items[p.slots[level] + 1], &items[p.slots[level]],
            (th.nritems - p.slots[level]) * sizeof(btrfs_key_ptr));

    items[p.slots[level]].key = k;
    items[p.slots[level]].blockptr = address;
    items[p.slots[level]].generation = fs.sb->generation;

    th.nritems++;

    if (!fs.cache.write(span(p.data[level], fs.sb->nodesize), th.bytenr)) // FIXME - return status
        return nt_status::INTERNAL_ERROR;

    if (p.slots[level] == 0 && level < BTRFS_MAX_LEVEL - 1 && p.data[level + 1]) {
        for (uint8_t i = level; i < BTRFS_MAX_LEVEL - 1; i++) {
            if (p.slots[i] != 0 || !p.data[i + 1])
                break;

            auto& th2 = *(btrfs_header*)p.data[i + 1];
            auto items2 = span((btrfs_key_ptr*)((uint8_t*)&th2 + sizeof(btrfs_header)), th2.nritems);

            items2[p.slots[i + 1]].key = items[0].key;

            if (!fs.cache.write(span(p.data[i + 1], fs.sb->nodesize), th2.bytenr)) // FIXME - return status
                return nt_status::INTERNAL_ERROR;
        }
    }

    return nt_status::SUCCESS;
}

static nt_status add_new_level(btrfs_filesystem& fs, btrfs_path& p, root& r,
                               uint8_t level) {
    uint64_t new_addr;
    btrfs_key first_key;

    auto new_tree = (uint8_t*)phys_address(allocate_folio(fs.node_folio_order));
    if (!new_tree)
        return nt_status::INSUFFICIENT_RESOURCES;

    if (auto status = allocate_metadata(fs, r.id, new_addr); !success(status)) {
        free_folio(phys_memory_pfn(new_tree) << PAGE_SHIFT);
        return status;
    }

    memcpy(new_tree, p.data[level - 1], sizeof(btrfs_header));

    auto& th = *(btrfs_header*)p.data[level - 1];

    if (level - 1 == 0) {
        auto* items = (btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header));

        first_key = items[0].key;
    } else {
        auto* items = (btrfs_key_ptr*)((uint8_t*)&th + sizeof(btrfs_header));

        first_key = items[0].key;
    }

    auto& th2 = *(btrfs_header*)new_tree;
    th2.bytenr = new_addr;
    th2.flags &= ~HEADER_FLAG_WRITTEN;
    th2.level = level;
    th2.nritems = 1;

    if (auto status = add_metadata_change(fs, new_addr, level, th.owner, false); !success(status)) {
        free_folio(phys_memory_pfn(new_tree) << PAGE_SHIFT);
        return status;
    }

    auto* items2 = (btrfs_key_ptr*)((uint8_t*)&th2 + sizeof(btrfs_header));

    items2[0].key = first_key;
    items2[0].blockptr = th.bytenr;
    items2[0].generation = fs.sb->generation;

    memset(&items2[1], 0, fs.sb->nodesize - sizeof(btrfs_header) - sizeof(btrfs_key_ptr));

    if (!fs.cache.write(span(new_tree, fs.sb->nodesize), th2.bytenr)) { // FIXME - return status
        free_folio(phys_memory_pfn(new_tree) << PAGE_SHIFT);
        return nt_status::INTERNAL_ERROR;
    }

    p.data[level] = new_tree;
    p.slots[level] = 0;

    r.ri.bytenr = new_addr;
    r.ri.level = level;
    r.ri.generation = fs.sb->generation;
    r.ri.generation_v2 = fs.sb->generation;

    if (r.id != BTRFS_ROOT_TREE_OBJECTID && !r.is_dirty) {
        insert_tail_list(fs.dirty_root_list, r.dirty_le);
        r.is_dirty = true;
    }

    return nt_status::SUCCESS;
}

static nt_status split_tree_at(btrfs_filesystem& fs, btrfs_path& p,
                               root& r, unsigned int split_point) {
    uint64_t new_addr;
    nt_status status;

    auto new_tree = (uint8_t*)phys_address(allocate_folio(fs.node_folio_order));
    if (!new_tree)
        return nt_status::INSUFFICIENT_RESOURCES;

    if (auto status = allocate_metadata(fs, r.id, new_addr); !success(status)) {
        free_folio(phys_memory_pfn(new_tree) << PAGE_SHIFT);
        return status;
    }

    auto& th = *(btrfs_header*)p.data[0];
    auto* items = (btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header));
    size_t to_copy = 0, total_data = 0;

    for (unsigned int i = 0; i < th.nritems; i++) {
        total_data += items[i].size;

        if (i >= split_point)
            to_copy += items[i].size;
    }

    memcpy(new_tree, p.data[0], sizeof(btrfs_header));

    auto& th2 = *(btrfs_header*)new_tree;
    th2.bytenr = new_addr;
    th2.flags &= ~HEADER_FLAG_WRITTEN;
    th2.nritems = th.nritems - split_point;

    // copy btrfs_item items
    auto* items2 = (btrfs_item*)((uint8_t*)&th2 + sizeof(btrfs_header));
    memcpy(items2, &items[split_point], sizeof(btrfs_item) * (th.nritems - split_point));

    // move trailing entries to new tree
    memcpy(new_tree + fs.sb->nodesize - to_copy,
           p.data[0] + fs.sb->nodesize - total_data, to_copy);

    for (unsigned int i = 0; i < th2.nritems; i++) {
        items2[i].offset += (uint32_t)(total_data - to_copy);
    }

    th.nritems = split_point;

    memset(new_tree + sizeof(btrfs_header) + (th2.nritems * sizeof(btrfs_item)),
           0, fs.sb->nodesize - to_copy - sizeof(btrfs_header) - (th2.nritems * sizeof(btrfs_item)));

    if (auto status = add_metadata_change(fs, new_addr, 0, th.owner, false); !success(status)) {
        free_folio(phys_memory_pfn(new_tree) << PAGE_SHIFT);
        return status;
    }

    // write both trees into cache

    if (!fs.cache.write(span(p.data[0], fs.sb->nodesize), th.bytenr)) { // FIXME - return status
        free_folio(phys_memory_pfn(new_tree) << PAGE_SHIFT);
        return nt_status::INTERNAL_ERROR;
    }

    if (!fs.cache.write(span(new_tree, fs.sb->nodesize), th2.bytenr)) { // FIXME - return status
        free_folio(phys_memory_pfn(new_tree) << PAGE_SHIFT);
        return nt_status::INTERNAL_ERROR;
    }

    if (!p.data[1]) {
        status = add_new_level(fs, p, r, 1);
        if (!success(status)) {
            free_folio(phys_memory_pfn(new_tree) << PAGE_SHIFT);
            return status;
        }
    }

    p.slots[1]++;
    status = insert_internal_node(fs, p, r, 1, items2[0].key, new_addr);
    if (!success(status)) {
        p.slots[1]--;
        free_folio(phys_memory_pfn(new_tree) << PAGE_SHIFT);
        return status;
    }

    if (p.slots[0] < split_point || split_point == 0) {
        p.slots[1]--;
        free_folio(phys_memory_pfn(new_tree) << PAGE_SHIFT);
    } else {
        p.slots[0] -= split_point;
        free_folio(phys_memory_pfn(p.data[0]) << PAGE_SHIFT);
        p.data[0] = new_tree;
    }

    return nt_status::SUCCESS;
}

static nt_status split_tree(btrfs_filesystem& fs, btrfs_path& p, root& r,
                            size_t size_used, size_t space_needed) {
    // FIXME - first try to push left and right

    auto& th = *(btrfs_header*)p.data[0];
    auto* items = (btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header));

    size_t sum;
    auto split_point = find_midpoint(th, size_used, sum);
    bool need_double_split = false;

    if (p.slots[0] >= split_point)
        sum = size_used - sum;

    if (fs.sb->nodesize - sizeof(btrfs_header) - sum < space_needed) {
        if (p.slots[0] < split_point) {
            while (split_point > p.slots[0]) {
                sum -= sizeof(btrfs_item) + items[split_point - 1].size;
                split_point--;

                if (fs.sb->nodesize - sizeof(btrfs_header) - sum >= space_needed)
                    break;
            }
        } else {
            while (split_point < p.slots[0]) {
                sum -= sizeof(btrfs_item) + items[split_point].size;
                split_point++;

                if (fs.sb->nodesize - sizeof(btrfs_header) - sum >= space_needed)
                    break;
            }
        }

        if (split_point == p.slots[0])
            need_double_split = true;
    }

    if (!need_double_split)
        return split_tree_at(fs, p, r, split_point);

    if (p.slots[0] != 0) {
        if (auto status = split_tree_at(fs, p, r, p.slots[0]); !success(status))
            return status;
    }

    auto num_items = ((btrfs_header*)p.data[0])->nritems;

    if (num_items != 0 && p.slots[0] < num_items) {
        if (auto status = split_tree_at(fs, p, r, p.slots[0]); !success(status))
            return status;
    }

    return nt_status::SUCCESS;
}

static nt_status insert_item2(btrfs_filesystem& fs, root& r, const btrfs_key& k,
                              uint32_t item_size, btrfs_path& p) {
    nt_status status;

    // check if data too big for any tree
    if (item_size > fs.sb->nodesize - sizeof(btrfs_header) - sizeof(btrfs_item))
        return nt_status::INTERNAL_ERROR;

    status = fs.find_item(r, k, p, true);
    if (!success(status))
        return status;

    {
        auto& th = *(btrfs_header*)p.data[0];
        auto* items = (btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header));

        if (p.slots[0] < th.nritems && items[p.slots[0]].key == k)
            return nt_status::OBJECT_NAME_COLLISION;

        unsigned int data_size = 0;

        for (unsigned int i = 0; i < th.nritems; i++) {
            data_size += items[i].size;
        }

        unsigned int size_used = data_size + (unsigned int)(th.nritems * sizeof(btrfs_item));

        if (size_used + sizeof(btrfs_item) + item_size > fs.sb->nodesize - sizeof(btrfs_header)) {
            status = split_tree(fs, p, r, size_used, sizeof(btrfs_item) + item_size);
            if (!success(status))
                return status;
        }
    }

    auto& th = *(btrfs_header*)p.data[0];
    auto* items = (btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header));

    // insert new btrfs_item

    if (p.slots[0] < th.nritems) {
        memmove(&items[p.slots[0] + 1], &items[p.slots[0]],
                (th.nritems - p.slots[0]) * sizeof(btrfs_item));
    }

    th.nritems++;

    items[p.slots[0]].key = k;

    if (item_size > 0 && p.slots[0] != th.nritems - 1) {
        unsigned int to_move = 0;

        // move data around

        uint32_t off = sizeof(btrfs_header) + items[th.nritems - 1].offset;

        for (unsigned int i = p.slots[0] + 1; i < th.nritems; i++) {
            to_move += items[i].size;
            items[i].offset -= item_size;
        }

        assert(off >= item_size + sizeof(btrfs_header));

        memmove(p.data[0] + off - item_size, p.data[0] + off, to_move);
    }

    if (p.slots[0] == 0)
        items[p.slots[0]].offset = (uint32_t)(fs.sb->nodesize - sizeof(btrfs_header) - item_size);
    else
        items[p.slots[0]].offset = (uint32_t)(items[p.slots[0] - 1].offset - item_size);

    items[p.slots[0]].size = item_size;

    // update parents if new first item

    if (p.slots[0] == 0 && p.data[1]) {
        for (uint8_t i = 0; i < BTRFS_MAX_LEVEL - 1; i++) {
            if (p.slots[i] != 0 || !p.data[i + 1])
                break;

            auto& th2 = *(btrfs_header*)p.data[i + 1];
            auto items2 = span((btrfs_key_ptr*)((uint8_t*)&th2 + sizeof(btrfs_header)), th2.nritems);

            items2[p.slots[i + 1]].key = items[0].key;

            if (!fs.cache.write(span(p.data[i + 1], fs.sb->nodesize), th2.bytenr)) // FIXME - return status
                return nt_status::INTERNAL_ERROR;
        }
    }

    return nt_status::SUCCESS;
}

static nt_status insert_item(btrfs_filesystem& fs, root& r, const btrfs_key& k,
                             span<const uint8_t> data) {
    btrfs_path p(fs.sb->nodesize);

    auto status = insert_item2(fs, r, k, (uint32_t)data.size(), p);
    if (!success(status))
        return status;

    auto& th = *(btrfs_header*)p.data[0];
    auto* items = (btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header));

    if (!data.empty()) {
        memcpy(p.data[0] + sizeof(btrfs_header) + items[p.slots[0]].offset,
               data.data(), data.size());
    }

    // copy tree back into page cache and mark dirty

    if (!fs.cache.write(span(p.data[0], fs.sb->nodesize), th.bytenr)) // FIXME - return status
        return nt_status::INTERNAL_ERROR;

    return nt_status::SUCCESS;
}

static nt_status remove_internal_node(btrfs_filesystem& fs, const btrfs_path& p,
                                      root& r, uint8_t level) {
    assert(p.data[level]);

    auto& th = *(btrfs_header*)p.data[level];
    auto items = (btrfs_key_ptr*)((uint8_t*)&th + sizeof(btrfs_header));
    auto slot = p.slots[level];

    assert(!(th.flags & HEADER_FLAG_WRITTEN));
    assert(th.level == level);
    assert(slot < th.nritems);

    auto status = add_metadata_change(fs, items[slot].blockptr, level - 1,
                                      th.owner, true);
    if (!success(status)) {
        print("add_metadata_change returned {}\n", status);
        return status;
    }

    memmove(&items[slot], &items[slot + 1],
            sizeof(btrfs_key_ptr) * (th.nritems - slot - 1));
    th.nritems--;

    if (th.nritems == 0) {
        if (level < BTRFS_MAX_LEVEL - 1 && p.data[level + 1]) // not the top
            return remove_internal_node(fs, p, r, level + 1);
        else {
            // tree has been completely emptied, and this is the top

            th.level = 0;

            if (!fs.cache.write(span(p.data[level], fs.sb->nodesize), th.bytenr)) // FIXME - return status
                return nt_status::INTERNAL_ERROR;

            change_metadata_level(fs, th.bytenr, 0);

            r.ri.level = 0;

            if (r.id != BTRFS_ROOT_TREE_OBJECTID && !r.is_dirty) {
                insert_tail_list(fs.dirty_root_list, r.dirty_le);
                r.is_dirty = true;
            }
        }
    } else {
        if (p.slots[level] == 0 && level < BTRFS_MAX_LEVEL - 1 && p.data[level + 1]) {
            for (uint8_t i = level; i < BTRFS_MAX_LEVEL - 1; i++) {
                if (p.slots[i] != 0 || !p.data[i + 1])
                    break;

                auto& th2 = *(btrfs_header*)p.data[i + 1];
                auto items2 = span((btrfs_key_ptr*)((uint8_t*)&th2 + sizeof(btrfs_header)), th2.nritems);

                items2[p.slots[i + 1]].key = items[0].key;

                if (!fs.cache.write(span(p.data[i + 1], fs.sb->nodesize), th2.bytenr)) // FIXME - return status
                    return nt_status::INTERNAL_ERROR;
            }
        }

        // copy tree back into page cache and mark dirty

        if (!fs.cache.write(span(p.data[level], fs.sb->nodesize), th.bytenr)) // FIXME - return status
            return nt_status::INTERNAL_ERROR;
    }

    return nt_status::SUCCESS;
}

nt_status btrfs_filesystem::remove_item2(const btrfs_path& p, root& r) {
    auto& th = *(btrfs_header*)p.data[0];
    auto* items = (btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header));

    // move data

    if (items[p.slots[0]].size != 0) {
        unsigned int data_size = 0, after_item = 0;

        for (unsigned int i = 0; i < th.nritems; i++) {
            data_size += items[i].size;

            if (i > p.slots[0])
                after_item += items[i].size;
        }

        memmove(p.data[0] + sb->nodesize - data_size + items[p.slots[0]].size,
                p.data[0] + sb->nodesize - data_size,
                after_item);

        for (unsigned int i = p.slots[0] + 1; i < th.nritems; i++) {
            items[i].offset += items[p.slots[0]].size;
        }
    }

    // adjust items

    memmove(&items[p.slots[0]], &items[p.slots[0] + 1],
            sizeof(btrfs_item) * (th.nritems - p.slots[0] - 1));
    th.nritems--;

    // update parents if deleting first item

    if (p.slots[0] == 0 && p.data[1]) {
        for (uint8_t i = 0; i < BTRFS_MAX_LEVEL - 1; i++) {
            if (p.slots[i] != 0 || !p.data[i + 1])
                break;

            auto& th2 = *(btrfs_header*)p.data[i + 1];
            auto items2 = span((btrfs_key_ptr*)((uint8_t*)&th2 + sizeof(btrfs_header)), th2.nritems);

            items2[p.slots[i + 1]].key = items[0].key;

            if (!cache.write(span(p.data[i + 1], sb->nodesize), th2.bytenr)) // FIXME - return status
                return nt_status::INTERNAL_ERROR;
        }
    }

    // FIXME - merging trees?

    if (th.nritems == 0 && p.data[1]) {
        auto status = remove_internal_node(*this, p, r, 1);
        if (!success(status)) {
            print("remove_internal_node returned {}\n", status);
            return status;
        }
    } else {
        // copy tree back into page cache and mark dirty

        if (!cache.write(span(p.data[0], sb->nodesize), th.bytenr)) // FIXME - return status
            return nt_status::INTERNAL_ERROR;
    }

    return nt_status::SUCCESS;
}

nt_status btrfs_filesystem::remove_item(root& r, const btrfs_key& k) {
    btrfs_path p(sb->nodesize);
    nt_status status;

    status = find_item(r, k, p, true);
    if (!success(status))
        return status;

    const auto& th = *(btrfs_header*)p.data[0];
    const auto* items = (btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header));

    if (items[p.slots[0]].key != k || p.slots[0] >= th.nritems)
        return nt_status::NOT_FOUND;

    return remove_item2(p, r);
}

nt_status btrfs_filesystem::change_key(btrfs_path& p, const btrfs_key& k) {
    auto& th = *(btrfs_header*)p.data[0];
    auto* items = (btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header));

    items[p.slots[0]].key = k;

    // update parents if changing first item

    if (p.slots[0] == 0 && p.data[1]) {
        for (uint8_t i = 0; i < BTRFS_MAX_LEVEL - 1; i++) {
            if (p.slots[i] != 0 || !p.data[i + 1])
                break;

            auto& th2 = *(btrfs_header*)p.data[i + 1];
            auto items2 = span((btrfs_key_ptr*)((uint8_t*)&th2 + sizeof(btrfs_header)), th2.nritems);

            items2[p.slots[i + 1]].key = items[0].key;

            if (!cache.write(span(p.data[i + 1], sb->nodesize), th2.bytenr)) // FIXME - return status
                return nt_status::INTERNAL_ERROR;
        }
    }

    // copy tree back into page cache and mark dirty

    if (!cache.write(span(p.data[0], sb->nodesize), th.bytenr)) // FIXME - return status
        return nt_status::INTERNAL_ERROR;

    return nt_status::SUCCESS;
}

void btrfs_filesystem::shorten_item(btrfs_path& p, uint32_t delta) {
    auto& th = *(btrfs_header*)p.data[0];
    auto* items = (btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header));
    auto& it = items[p.slots[0]];

    it.size -= delta;

    size_t to_move = 0;

    for (unsigned int i = p.slots[0]; i < th.nritems; i++) {
        items[i].offset += delta;
        to_move += items[i].size;
    }

    memmove(p.data[0] + sizeof(btrfs_header) + items[th.nritems - 1].offset,
            p.data[0] + sizeof(btrfs_header) + items[th.nritems - 1].offset - delta,
            to_move);

    cache.write(span(p.data[0], sb->nodesize), th.bytenr); // FIXME - return status
}

nt_status btrfs_filesystem::extend_item(btrfs_path& p, uint32_t delta) {
    auto& th = *(btrfs_header*)p.data[0];
    auto* items = (btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header));
    auto& it = items[p.slots[0]];

    if (it.size + delta > sb->nodesize - sizeof(btrfs_header) - sizeof(btrfs_item)) {
        print("btrfs_filesystem::extend_item: key {} would be {} bytes, too big for any tree",
              it.key, it.size + delta);
        return nt_status::INTERNAL_ERROR;
    }

    size_t used = sizeof(btrfs_header) + (sizeof(btrfs_item) * th.nritems);

    for (unsigned int i = 0; i < th.nritems; i++) {
        used += items[i].size;
    }

    if (used + delta > sb->nodesize) {
        // FIXME - if now too large for node, move items left or right (share this logic with insert_item)
        print("btrfs_filesystem::extend_item: FIXME - move items left or right\n");
        return nt_status::INTERNAL_ERROR;
    }

    // adjust item offsets and move data

    {
        unsigned int to_move = 0;

        // move data around

        uint32_t off = sizeof(btrfs_header) + items[th.nritems - 1].offset;

        for (unsigned int i = p.slots[0]; i < th.nritems; i++) {
            to_move += items[i].size;
            items[i].offset -= delta;
        }

        assert(off >= it.size + delta + sizeof(btrfs_header));

        memmove(p.data[0] + off - delta, p.data[0] + off, to_move);
    }

    // change item size

    it.size += delta;

    return nt_status::SUCCESS;
}

nt_status btrfs_filesystem::move_path_back(btrfs_path& p, root& r, bool cow) {
    if (p.slots[0] != 0) {
        p.slots[0]--;
        return nt_status::SUCCESS;
    }

    for (uint8_t i = 1; i < BTRFS_MAX_LEVEL; i++) {
        if (!p.data[i])
            return nt_status::NOT_FOUND;

        if (p.slots[i] == 0)
            continue;

        p.slots[i]--;

        for (auto j = (int8_t)i; j > 0; j--) {
            const auto& th = *(btrfs_header*)p.data[j];
            auto items = span((btrfs_key_ptr*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

            auto status = read_metadata(items[p.slots[j]].blockptr, span(p.data[j - 1], sb->nodesize));
            if (!success(status))
                return status;

            const auto& th2 = *(btrfs_header*)p.data[j - 1];

            p.slots[j - 1] = th2.nritems - 1;
        }

        if (cow) {
            auto status = cow_tree(*this, r, p);
            if (!success(status))
                return status;
        }

        return nt_status::SUCCESS;
    }

    return nt_status::NOT_FOUND;
}

nt_status btrfs_filesystem::move_path_forward(btrfs_path& p, root& r, bool cow) {
    if (p.slots[0] < ((btrfs_header*)p.data[0])->nritems - 1) {
        p.slots[0]++;
        return nt_status::SUCCESS;
    }

    for (uint8_t i = 1; i < BTRFS_MAX_LEVEL; i++) {
        if (!p.data[i])
            return nt_status::NOT_FOUND;

        if (p.slots[i] == ((btrfs_header*)p.data[i])->nritems - 1)
            continue;

        p.slots[i]++;

        for (auto j = (int8_t)i; j > 0; j--) {
            const auto& th = *(btrfs_header*)p.data[j];
            auto items = span((btrfs_key_ptr*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

            auto status = read_metadata(items[p.slots[j]].blockptr, span(p.data[j - 1], sb->nodesize));
            if (!success(status))
                return status;

            p.slots[j - 1] = 0;
        }

        if (cow) {
            auto status = cow_tree(*this, r, p);
            if (!success(status))
                return status;
        }

        return nt_status::SUCCESS;
    }

    return nt_status::NOT_FOUND;
}

nt_status btrfs_inode::get_last_dir_index(uint64_t& ret) {
    nt_status status;
    btrfs_path p(fs.sb->nodesize);

    status = fs.find_item(*r, {tree.key, key_type::DIR_INDEX, 0xffffffffffffffff},
                          p, false);
    if (!success(status))
        return status;

    status = fs.move_path_back(p, *r, false);

    if (status == nt_status::NOT_FOUND) {
        ret = 1;
        return nt_status::SUCCESS;
    } else if (!success(status))
        return status;

    auto& th = *(btrfs_header*)p.data[0];
    auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

    if (items[p.slots[0]].key.objectid != tree.key || items[p.slots[0]].key.type != key_type::DIR_INDEX) {
        ret = 1;
        return nt_status::SUCCESS;
    }

    ret = items[p.slots[0]].key.offset;

    return nt_status::SUCCESS;
}

static bool validate_name(string_view name) {
    if (name.empty())
        return false;

    if (name == "." || name == "..")
        return false;

    if (name.size() >= 256)
        return false;

    for (auto c : name) {
        if (c == '\0' || c == '/' || c == '\\')
            return false;
    }

    return true;
}

static nt_status add_dir_item(btrfs_filesystem& fs, root& r, btrfs_inode& parent,
                              string_view name, uint64_t inode,
                              btrfs_inode_type type, uint64_t& index) {
    nt_status status;
    btrfs_path p(fs.sb->nodesize);

    auto hash = calc_crc32c(0xfffffffe, name);

    btrfs_key key{parent.tree.key, key_type::DIR_ITEM, hash};

    // look for existing DIR_ITEM

    status = fs.find_item(r, key, p, true);
    if (!success(status))
        return status;

    size_t di_size = offsetof(btrfs_dir_item, name) + name.size();
    auto di = (btrfs_dir_item*)malloc(di_size);
    if (!di)
        return nt_status::INSUFFICIENT_RESOURCES;

    di->location.objectid = inode;
    di->location.type = key_type::INODE_ITEM;
    di->location.offset = 0;
    di->transid = fs.sb->generation;
    di->data_len = 0;
    di->name_len = (uint16_t)name.size();
    di->type = type;
    memcpy(di->name, name.data(), name.size());

    bool found = false;

    {
        auto& th = *(btrfs_header*)p.data[0];
        auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

        if (p.slots[0] < th.nritems && items[p.slots[0]].key == key) {
            status = fs.extend_item(p, (uint32_t)di_size);
            if (!success(status)) {
                free(di);
                return status;
            }

            found = true;
        }
    }

    if (found) {
        auto& th = *(btrfs_header*)p.data[0];
        auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);
        const auto& it = items[p.slots[0]];
        auto data = span((const uint8_t*)&th + sizeof(btrfs_header) + it.offset, it.size);

        memcpy((uint8_t*)data.data() + data.size() - di_size, di, di_size);

        if (!fs.cache.write(span(p.data[0], fs.sb->nodesize), th.bytenr)) { // FIXME - return status
            free(di);
            return nt_status::INTERNAL_ERROR;
        }
    } else {
        status = insert_item(fs, r, key, span((uint8_t*)di, di_size));

        if (!success(status)) {
            free(di);

            if (status == nt_status::OBJECT_NAME_COLLISION)
                status = nt_status::INTERNAL_ERROR;

            return status;
        }
    }

    // create DIR_INDEX in parent

    status = parent.get_last_dir_index(index);
    if (!success(status)) {
        free(di);
        return status;
    }

    index++;

    status = insert_item(fs, r, {parent.tree.key, key_type::DIR_INDEX, index},
                         span((uint8_t*)di, di_size));
    if (!success(status)) {
        free(di);

        if (status == nt_status::OBJECT_NAME_COLLISION)
            status = nt_status::INTERNAL_ERROR;

        return status;
    }

    free(di);

    return nt_status::SUCCESS;
}

static nt_status add_inode_ref(btrfs_filesystem& fs, root& r, string_view name,
                               uint64_t index, uint64_t inode,
                               uint64_t parent) {
    nt_status status;
    btrfs_key key{inode, key_type::INODE_REF, parent};

    size_t ir_size = offsetof(btrfs_inode_ref, name) + name.size();
    auto ir = (btrfs_inode_ref*)malloc(ir_size);
    if (!ir)
        return nt_status::INSUFFICIENT_RESOURCES;

    ir->index = index;
    ir->name_len = (uint16_t)name.size();
    memcpy(ir->name, name.data(), name.size());

    status = insert_item(fs, r, key, span((uint8_t*)ir, ir_size));

    if (status != nt_status::OBJECT_NAME_COLLISION) {
        free(ir);
        return status;
    }

    // FIXME - insert INODE_EXTREF instead if INODE_REF would be too large

    btrfs_path p(fs.sb->nodesize);

    status = fs.find_item(r, key, p, true);
    if (!success(status)) {
        free(ir);
        return status;
    }

    status = fs.extend_item(p, (uint32_t)ir_size);
    if (!success(status)) {
        print("btrfs_filesystem::extend_item returned {}\n", status);
        free(ir);
        return status;
    }

    {
        auto& th = *(btrfs_header*)p.data[0];
        auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);
        const auto& it = items[p.slots[0]];
        auto data = span((const uint8_t*)&th + sizeof(btrfs_header) + it.offset, it.size);

        memcpy((uint8_t*)data.data() + data.size() - ir_size, ir, ir_size);

        if (!fs.cache.write(span(p.data[0], fs.sb->nodesize), th.bytenr)) { // FIXME - return status
            free(ir);
            return nt_status::INTERNAL_ERROR;
        }
    }

    free(ir);

    return nt_status::SUCCESS;
};

nt_status btrfs_inode::create_child(string_view name, uint32_t create_options,
                                    optional<reference_wrapper<::inode>>& obj) {
    nt_status status;
    btrfs_inode_item child_ii;
    btrfs_path p(fs.sb->nodesize);
    bool is_dir = create_options & FILE_DIRECTORY_FILE;
    uint64_t new_inode;

    if (fs.readonly)
        return nt_status::MEDIA_WRITE_PROTECTED;

    if (!validate_name(name))
        return nt_status::OBJECT_NAME_INVALID;

    auto now = (uint64_t)kernel_time_page.system_time + kernel_time_page.system_time_offset;

    // FIXME - make sure this can't race with rename's directory empty check

    {
        lock_guard lock(fs.trans_lock);

        // FIXME - use finer-grained locking rather than trans_lock (would need directory lock)
        //         (i.e. make sure we can't race with directory removal)

        if (r->next_inode_number == 0) {
            status = find_last_inode_number(fs, *r);
            if (!success(status))
                return status;
        }

        new_inode = r->next_inode_number.fetch_add(1);

        // create INODE_ITEM

        memset(&child_ii, 0, sizeof(btrfs_inode_item));
        child_ii.generation = fs.sb->generation;
        child_ii.transid = fs.sb->generation;
        child_ii.nlink = 1;
        child_ii.uid = 0; // FIXME
        child_ii.gid = 0; // FIXME

        // FIXME - do we inherit mode from parent?
        if (is_dir)
            child_ii.mode = S_IFDIR | 0755;
        else
            child_ii.mode = S_IFREG | 0644;

        child_ii.sequence = 0; // FIXME?
        child_ii.ctime = child_ii.mtime = child_ii.otime = system_time_to_btrfs_time(now);

        status = insert_item(fs, *r, {new_inode, key_type::INODE_ITEM, 0},
                             span((uint8_t*)&child_ii, sizeof(child_ii)));
        if (!success(status)) {
            if (status == nt_status::OBJECT_NAME_COLLISION)
                status = nt_status::INTERNAL_ERROR;
            return status;
        }

        // create DIR_ITEM and DIR_INDEX in parent

        uint64_t index;

        status = add_dir_item(fs, *r, *this, name, new_inode,
                              is_dir ? btrfs_inode_type::directory : btrfs_inode_type::file,
                              index);
        if (!success(status)) {
            print("add_dir_item returned {}\n", status);
            return status;
        }

        // create INODE_REF

        status = add_inode_ref(fs, *r, name, index, new_inode, tree.key);
        if (!success(status)) {
            print("add_inode_ref returned {}\n", status);
            return status;
        }

        // adjust size etc. of parent directory

        {
            lock_guard lg(inode_lock);

            ii.transid = fs.sb->generation;
            ii.size += name.size() * 2;
            ii.sequence++;
            ctime = now;
            mtime = now;
        }

        status = flush_inode_item(p);
        if (!success(status)) {
            print("btrfs_inode::flush_inode_item returned {}\n", status);
            return status;
        }

        // adjust root_item

        r->ri.ctime = system_time_to_btrfs_time(now);
        r->ri.ctransid = fs.sb->generation;

        // create btrfs_inode object and return

        auto bi = new btrfs_inode(fs, r->id, new_inode,
                                  is_dir ? btrfs_inode_type::directory : btrfs_inode_type::file);
        if (!bi) {
            print("new failed\n");
            return nt_status::INSUFFICIENT_RESOURCES;
        }

        memcpy(&bi->ii, &child_ii, sizeof(btrfs_inode_item));
        bi->file_size = 0;
        bi->mtime = bi->ctime = now;

        {
            lock_guard lock(r->inode_lock);
            insert_into_rbtree(r->inodes, bi->tree);
        }

        obj = *bi;
    }

    return nt_status::SUCCESS;
}

static nt_status search_dir_item(btrfs_filesystem& fs, root& r, uint64_t inode,
                                 uint32_t hash, string_view name,
                                 btrfs_key& dir_item_key) {
    bool found = false;
    auto k = btrfs_key{inode, key_type::DIR_ITEM, hash};
    btrfs_path p(fs.sb->nodesize);

    auto status = fs.find_item(r, k, p, false);
    if (!success(status))
        return status;

    auto& th = *(btrfs_header*)p.data[0];
    auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

    if (p.slots[0] >= th.nritems)
        return nt_status::OBJECT_NAME_NOT_FOUND;

    const auto& it = items[p.slots[0]];
    const auto& found_key = it.key;

    if (found_key != k)
        return nt_status::OBJECT_NAME_NOT_FOUND;

    auto data = span((const uint8_t*)&th + sizeof(btrfs_header) + it.offset, it.size);

    while (data.size() >= offsetof(btrfs_dir_item, name)) {
        const auto& di = *(btrfs_dir_item*)data.data();
        size_t exp_len = offsetof(btrfs_dir_item, name) + di.data_len + di.name_len;

        if (data.size() < exp_len)
            return nt_status::OBJECT_NAME_NOT_FOUND;

        if (string_view(di.name, di.name_len) == name) {
            dir_item_key = di.location;
            found = true;
            break;
        }

        data = data.subspan(exp_len, data.size() - exp_len);
    }

    if (!found)
        return nt_status::OBJECT_NAME_NOT_FOUND;

    return nt_status::SUCCESS;
};

nt_status btrfs_inode::open_child(string_view name, object_ref& obj) {
    if (inode_type != inode_type::directory)
        return nt_status::INVALID_PARAMETER;

    auto hash = calc_crc32c(0xfffffffe, name);

    btrfs_key dir_item_key;

    {
        shared_lock lg(fs.trans_lock);

        auto status = search_dir_item(fs, *r, tree.key, hash, name, dir_item_key);
        if (!success(status))
            return status;
    }

    if (dir_item_key.type == key_type::ROOT_ITEM)
        return fs.open_inode(dir_item_key.objectid, SUBVOL_ROOT_INODE, obj);
    else
        return fs.open_inode(r->id, dir_item_key.objectid, obj);
}

nt_status btrfs_filesystem::open_inode(uint64_t subvol, uint64_t inode,
                                       object_ref& obj) {
    auto r = find_root(subvol);
    if (!r)
        return nt_status::INTERNAL_ERROR;

    {
        lock_guard lock(r->inode_lock);

        auto* n = find_rbtree_node(r->inodes, inode);

        if (n && n->key == inode) {
            auto& in = rbtree_containing_record<btrfs_inode>(n);

            in.acquire();
            obj = in;
            return nt_status::SUCCESS;
        }
    }

    auto ino = new btrfs_inode(*this, subvol, inode, btrfs_inode_type::unknown);

    if (!ino) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    if (!ino->load_inode()) {
        delete ino;
        return nt_status::INTERNAL_ERROR;
    }

    {
        optional<lock_guard<spinlock>> lock(r->inode_lock);

        auto* n = find_rbtree_node(r->inodes, inode);

        if (n && n->key == inode) {
            auto& in = rbtree_containing_record<btrfs_inode>(n);

            in.acquire();
            lock.reset();

            delete ino;

            obj = in;
            return nt_status::SUCCESS;
        }

        insert_into_rbtree(r->inodes, ino->tree);
    }

    obj = *ino;
    return nt_status::SUCCESS;
}

nt_status btrfs_inode::read_zeroes_into_cache(uint64_t page_num, uint64_t num_pages,
                                              uint64_t& read_pages) {
    page_cache_entry* dest_p;

    for (uint64_t i = 0; i < num_pages; i++) {
        {
            lock_guard lg(cache.base.cache_lock);

            if (i == 0)
                dest_p = cache.find_page_cache_entry(page_num);
            else
                dest_p = cache.next_page_cache_entry(*dest_p);
        }

        increase_page_refcount(zero_pfn);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
        dest_p->pfn = zero_pfn;
#pragma GCC diagnostic pop
    }

    read_pages = num_pages;
    return nt_status::SUCCESS;
}

nt_status btrfs_inode::read_inline_extent_into_cache(uint64_t page_num,
                                                     uint64_t num_pages,
                                                     const inode_extent& ie,
                                                     uint64_t& read_pages) {
    if (ie.ed.compression != 0) {
        print("FIXME - compression\n"); // FIXME
        return nt_status::NOT_SUPPORTED;
    }

    if (page_num >= bytes_to_pages(ie.ed.ram_bytes)) {
        read_pages = 0;
        return nt_status::SUCCESS;
    }

    num_pages = min(num_pages, (uint64_t)bytes_to_pages(ie.ed.ram_bytes) - page_num);

    if (num_pages == 0) {
        read_pages = 0;
        return nt_status::SUCCESS;
    }

    page_cache_entry* dest_p;

    for (uint64_t i = 0; i < num_pages; i++) {
        {
            lock_guard lg(cache.base.cache_lock);

            if (i == 0)
                dest_p = cache.find_page_cache_entry(page_num);
            else
                dest_p = cache.next_page_cache_entry(*dest_p);

            if (dest_p->pfn == 0) {
                auto pa = allocate_folio(0, page_state::cache);
                if (!pa) {
                    if (i == 0)
                        return nt_status::INSUFFICIENT_RESOURCES;
                    else {
                        read_pages = i;
                        return nt_status::SUCCESS;
                    }
                }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
                dest_p->pfn = pa >> PAGE_SHIFT;
#pragma GCC diagnostic pop
            }
        }

        auto dest_va = get_phys_address(*dest_p);

        uint64_t offset = (page_num + i) << PAGE_SHIFT;
        auto to_copy = min((uint64_t)PAGE_SIZE, ie.ed.ram_bytes - offset);

        memcpy(dest_va, (uint8_t*)&ie.ed.disk_bytenr + offset, to_copy);

        memset((uint8_t*)dest_va + to_copy, 0, PAGE_SIZE - to_copy);
    }

    read_pages = num_pages;
    return nt_status::SUCCESS;
}

static nt_status check_csums(btrfs_inode& ino, uint64_t page_num, uint64_t addr,
                             uint64_t num_pages) {
    auto& fs = ino.fs;
    btrfs_path p(fs.sb->nodesize);
    page_cache_entry* src_p;
    bool first = true;

    auto k = btrfs_key{BTRFS_EXTENT_CSUM_OBJECTID, key_type::EXTENT_CSUM, addr};

    auto status = fs.find_item(*fs.csum_root, k, p, false);
    if (!success(status))
        return status;

    {
        auto& th = *(btrfs_header*)p.data[0];
        auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

        if (p.slots[0] == th.nritems) {
            p.slots[0]--;
        } else if (items[p.slots[0]].key != k) {
            status = fs.move_path_back(p, *fs.csum_root, false);
            if (!success(status))
                return status;
        }
    }

    do {
        auto& th = *(btrfs_header*)p.data[0];
        auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

        const auto& it = items[p.slots[0]];
        const auto& found_key = it.key;

        if (found_key.objectid != BTRFS_EXTENT_CSUM_OBJECTID || found_key.type != key_type::EXTENT_CSUM)
            return nt_status::NOT_FOUND;

        auto data = span((uint32_t*)((const uint8_t*)&th + sizeof(btrfs_header) + it.offset),
                        it.size / sizeof(uint32_t));

        auto index = (addr - found_key.offset) / fs.sb->sectorsize;
        if (index >= data.size())
            return nt_status::NOT_FOUND;

        while (index < data.size() && num_pages > 0) {
            {
                lock_guard lg(ino.cache.base.cache_lock);

                if (first) {
                    src_p = ino.cache.find_page_cache_entry(page_num);
                    first = false;
                } else
                    src_p = ino.cache.next_page_cache_entry(*src_p);
            }

            auto found = ~calc_crc32c(0xffffffff, span((const uint8_t*)get_phys_address(*src_p), fs.sb->sectorsize));

            if (found != data[index]) {
                print("csum at {:x} was {:08x}, expected {:08x}\n", addr, found,
                      data[index]);
                return nt_status::DISK_CORRUPT_ERROR;
            }

            addr += fs.sb->sectorsize;
            page_num++;
            num_pages--;
            index++;
        }

        if (num_pages == 0)
            break;

        p.slots[0]++;
        if (p.slots[0] >= leaf_nritems(p)) {
            status = next_leaf(fs, p, *fs.csum_root, false);
            if (!success(status))
                return status;
        }
    } while (true);

    return nt_status::SUCCESS;
}

nt_status btrfs_inode::read_extent_into_cache(uint64_t page_num, uint64_t num_pages,
                                              const inode_extent& ie,
                                              uint64_t& read_pages) {
    if (ie.ed.compression != 0) {
        print("FIXME - compression\n"); // FIXME
        return nt_status::NOT_SUPPORTED;
    }

    uint64_t addr = ie.ed.disk_bytenr + ie.ed.offset + (page_num << PAGE_SHIFT) - ie.offset;

    num_pages = min(num_pages, ((ie.offset + ie.ed.num_bytes) >> PAGE_SHIFT) - page_num);

    uint64_t pages_read;

    auto status = fs.cache.read_into_cache_and_pin(addr >> PAGE_SHIFT,
                                                   num_pages, pages_read);
    if (!success(status))
        return status;

    {
        rangelock_req rlr(fs.cache.base.rl, addr >> PAGE_SHIFT, num_pages, rangelock_mode::exclusive);

        page_cache_entry* src_p;
        page_cache_entry* dest_p;

        for (uint64_t i = 0; i < pages_read; i++) {
            {
                lock_guard lg(cache.base.cache_lock);

                if (i == 0)
                    dest_p = cache.find_page_cache_entry(page_num);
                else
                    dest_p = cache.next_page_cache_entry(*dest_p);
            }

            {
                lock_guard lg(fs.cache.base.cache_lock);

                if (i == 0)
                    src_p = fs.cache.find_page_cache_entry(addr >> PAGE_SHIFT);
                else
                    src_p = fs.cache.next_page_cache_entry(*src_p);
            }

            increase_page_refcount(src_p->pfn);
            dest_p->pfn = src_p->pfn;
        }
    }

    fs.cache.unpin(addr >> PAGE_SHIFT, pages_read);

    if (!(ii.flags & BTRFS_INODE_NODATASUM)) {
        if (auto status = check_csums(*this, page_num, addr, pages_read); !success(status)) {
            print("check_csums returned {}\n", status);
            return status;
        }
    }

    // FIXME - read from another copy (and replace) if csum is bad

    read_pages = pages_read;
    return nt_status::SUCCESS;
}

nt_status btrfs_inode::read_into_cache(uint64_t page_num, uint64_t num_pages,
                                       uint64_t& read_pages) {
    uint64_t lastoff = page_num << PAGE_SHIFT;

    // FIXME - extents should be an RB tree

    for (auto le = extents.next; le != &extents; le = le->next) {
        auto& ie = containing_record<inode_extent>(le);

        if (ie.ed.type == btrfs_extent_type::inline_extent) {
            if (ie.offset + ie.ed.ram_bytes <= page_num << PAGE_SHIFT)
                continue;
        } else {
            if (ie.offset + ie.ed.num_bytes <= page_num << PAGE_SHIFT)
                continue;
        }

        if (ie.offset > lastoff) {
            auto pages_to_zero = min(num_pages, (ie.offset - lastoff) >> PAGE_SHIFT);
            return read_zeroes_into_cache(page_num, pages_to_zero, read_pages);
        }

        switch (ie.ed.type) {
            case btrfs_extent_type::inline_extent:
                return read_inline_extent_into_cache(page_num, num_pages, ie,
                                                     read_pages);

            case btrfs_extent_type::regular:
                return read_extent_into_cache(page_num, num_pages, ie,
                                              read_pages);

            case btrfs_extent_type::prealloc: {
                auto pages_to_zero = min(num_pages, ((ie.offset + ie.ed.num_bytes) >> PAGE_SHIFT) - page_num);
                return read_zeroes_into_cache(page_num, pages_to_zero,
                                              read_pages);
            }

            default:
                print("unknown btrfs extent type {:x}\n", (unsigned int)ie.ed.type);
                return nt_status::DISK_CORRUPT_ERROR;
        }
    }

    return read_zeroes_into_cache(page_num, num_pages, read_pages);
}

static nt_status allocate_data(btrfs_filesystem& fs, uint64_t size,
                               uint64_t& addr) {
    nt_status status;

    for (auto le = fs.chunk_list.next; le != &fs.chunk_list; le = le->next) {
        auto& c = containing_record<chunk>(le);

        // FIXME - skip if chunk is readonly

        if (!(c.ci.type & BTRFS_BLOCK_GROUP_DATA))
            continue;

        // skip unsupported RAID types
        if (c.ci.type & (BTRFS_BLOCK_GROUP_RAID0 | BTRFS_BLOCK_GROUP_RAID10 | BTRFS_BLOCK_GROUP_RAID5 | BTRFS_BLOCK_GROUP_RAID6))
            continue;

        status = allocate_in_chunk(fs, c, size, addr);
        if (status != nt_status::DISK_FULL)
            return status;
    }

    chunk* c;

    status = allocate_new_chunk(fs, c, fs.data_profile);
    if (!success(status))
        return status;

    return allocate_in_chunk(fs, *c, size, addr);
}

static nt_status add_data_change(btrfs_filesystem& fs, uint64_t extent_address,
                                 uint64_t extent_length, uint64_t root,
                                 uint64_t objectid, uint64_t offset,
                                 int32_t count) {
    {
        lock_guard lock(fs.ref_changes_lock);

        if (fs.ref_changes) {
            auto n = find_rbtree_node(fs.ref_changes, extent_address);

            if (n && n->key == extent_address) {
                auto& rc = rbtree_containing_record<ref_change>(n);

                assert(!rc.is_metadata);
                assert(rc.data.extent_length == extent_length);

                for (auto le2 = rc.data.rcds.next; le2 != &rc.data.rcds; le2 = le2->next) {
                    auto& rcd = containing_record<ref_change_data>(le2);

                    if (rcd.root == root && rcd.objectid == objectid && rcd.offset == offset) {
                        rcd.count += count;
                        return nt_status::SUCCESS;
                    }
                }

                auto rcd = (ref_change_data*)malloc(sizeof(ref_change_data));
                if (!rcd)
                    return nt_status::INSUFFICIENT_RESOURCES;

                rcd->root = root;
                rcd->objectid = objectid;
                rcd->offset = offset;
                rcd->count = count;

                insert_tail_list(rc.data.rcds, rcd->le);

                return nt_status::SUCCESS;
            }
        }

        auto rc = (ref_change*)malloc(sizeof(ref_change));
        if (!rc)
            return nt_status::INSUFFICIENT_RESOURCES;

        rc->tree.key = extent_address;
        rc->is_metadata = false;
        rc->data.extent_length = extent_length;
        initialize_list(rc->data.rcds);

        auto rcd = (ref_change_data*)malloc(sizeof(ref_change_data));
        if (!rcd) {
            free(rc);
            return nt_status::INSUFFICIENT_RESOURCES;
        }

        rcd->root = root;
        rcd->objectid = objectid;
        rcd->offset = offset;
        rcd->count = count;

        insert_tail_list(rc->data.rcds, rcd->le);

        insert_into_rbtree(fs.ref_changes, rc->tree);
    }

    return nt_status::SUCCESS;
}

nt_status btrfs_inode::flush_inode_item(btrfs_path& p) {
    nt_status status;
    btrfs_key search_key{tree.key, key_type::INODE_ITEM, 0};

    status = fs.find_item(*r, search_key, p, true);
    if (!success(status))
        return status;

    auto& th = *(btrfs_header*)p.data[0];
    auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

    auto& it = items[p.slots[0]];
    const auto& found_key = it.key;

    if (found_key.objectid != tree.key || found_key.type != key_type::INODE_ITEM) {
        print("btrfs_inode::flush_inode_item: could not find {}\n", search_key);
        return nt_status::DISK_CORRUPT_ERROR;
    }

    auto data = span((const uint8_t*)&th + sizeof(btrfs_header) + it.offset, it.size);

    if (it.size < sizeof(btrfs_inode_item))
        return nt_status::DISK_CORRUPT_ERROR;

    auto& dest_ii = *(btrfs_inode_item*)data.data();

    memcpy(&dest_ii, &ii, sizeof(btrfs_inode_item));

    dest_ii.mtime = system_time_to_btrfs_time(mtime);
    dest_ii.ctime = system_time_to_btrfs_time(ctime);

    if (!fs.cache.write(span(p.data[0], fs.sb->nodesize), th.bytenr)) // FIXME - return status
        return nt_status::INTERNAL_ERROR;

    return nt_status::SUCCESS;
}

static nt_status punch_hole(btrfs_inode& inode, uint64_t start, uint64_t length) {
    auto& fs = inode.fs;
    btrfs_key search_key = { inode.tree.key, key_type::EXTENT_DATA, start };
    btrfs_path p(fs.sb->nodesize);
    nt_status status;

    // FIXME - rwsem?

    status = fs.find_item(*inode.r, search_key, p, true);
    if (!success(status))
        return status;

    {
        auto& th = *(btrfs_header*)p.data[0];
        auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

        if (p.slots[0] >= th.nritems) {
            if (p.slots[0] == 0)
                return nt_status::SUCCESS;

            p.slots[0]--;
        } else {
            const auto& k = items[p.slots[0]].key;

            if (k.objectid > inode.tree.key || k.type > key_type::EXTENT_DATA ||
                k.offset > start) {
                if (p.slots[0] == 0)
                    return nt_status::SUCCESS;

                p.slots[0]--;
            }
        }
    }

    bool changed_inode = false;

    while (true) {
        if (p.slots[0] == leaf_nritems(p)) {
            status = next_leaf(fs, p, *inode.r, true);
            if (status == nt_status::NOT_FOUND)
                break;
            else if (!success(status))
                return status;
        }

        auto& th = *(btrfs_header*)p.data[0];
        auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

        const auto& it = items[p.slots[0]];
        auto k = it.key;

        if (k.objectid != inode.tree.key || k.type != key_type::EXTENT_DATA ||
            k.offset >= start + length)
            break;

        // FIXME - inline extents

        auto data = span((const uint8_t*)&th + sizeof(btrfs_header) + it.offset, it.size);

        if (it.size < offsetof(btrfs_file_extent_item, disk_bytenr)) {
            print("short item at {} ({} bytes, expected at least {})\n",
                  k, it.size, offsetof(btrfs_file_extent_item, disk_bytenr));
            return nt_status::DISK_CORRUPT_ERROR;
        }

        auto& ed = *(btrfs_file_extent_item*)data.data();

        if (ed.type == btrfs_extent_type::inline_extent) {
            status = fs.remove_item2(p, *inode.r);
            if (!success(status)) {
                print("btrfs_filesystem::remove_item2 returned {}\n", status);
                return status;
            }

            inode.ii.nbytes = 0;
            changed_inode = true;

            break;
        } else if (ed.type != btrfs_extent_type::regular && ed.type != btrfs_extent_type::prealloc) {
            print("unexpected file extent type {:x}\n", (unsigned int)ed.type);
            return nt_status::DISK_CORRUPT_ERROR;
        }

        if (ed.compression != 0) {
            print("FIXME - compression\n"); // FIXME
            return nt_status::NOT_IMPLEMENTED;
        }

        if (it.size < sizeof(btrfs_file_extent_item)) {
            print("short item at {} ({} bytes, expected {})\n",
                  k, it.size, sizeof(btrfs_file_extent_item));
            return nt_status::DISK_CORRUPT_ERROR;
        }

        auto size = ed.num_bytes; // FIXME - different if compressed(?)

        if (ed.disk_bytenr == 0) {
            // explicit hole

            status = fs.remove_item2(p, *inode.r);
            if (!success(status)) {
                print("btrfs_filesystem::remove_item2 returned {}\n", status);
                return status;
            }

            search_key.offset = k.offset;

            status = fs.find_item(*inode.r, search_key, p, true);
            if (status == nt_status::NOT_FOUND)
                break;
            else if (!success(status))
                return status;

            continue;
        } else if (start <= k.offset && start + length >= k.offset + size) {
            // remove entire file extent

            status = add_data_change(fs, ed.disk_bytenr, ed.disk_num_bytes,
                                     inode.r->id, inode.tree.key,
                                     k.offset - ed.offset, -1);
            if (!success(status)) {
                print("add_data_change returned {}\n", status);
                return status;
            }

            status = fs.remove_item2(p, *inode.r);
            if (!success(status)) {
                print("btrfs_filesystem::remove_item2 returned {}\n", status);
                return status;
            }

            search_key.offset = k.offset;

            status = fs.find_item(*inode.r, search_key, p, true);
            if (status == nt_status::NOT_FOUND)
                break;
            else if (!success(status))
                return status;

            inode.ii.nbytes -= size;
            changed_inode = true;

            continue;
        } else if (start > k.offset && start + length < k.offset + size) {
            // remove middle

            // FIXME - handle explicit holes

            status = add_data_change(fs, ed.disk_bytenr, ed.disk_num_bytes,
                                     inode.r->id, inode.tree.key,
                                     k.offset - ed.offset, 1);
            if (!success(status)) {
                print("add_data_change returned {}\n", status);
                return status;
            }

            btrfs_file_extent_item new_ed;

            memcpy(&new_ed, &ed, sizeof(btrfs_file_extent_item));

            ed.num_bytes = start - k.offset;

            if (!fs.cache.write(span(p.data[0], fs.sb->nodesize), th.bytenr)) // FIXME - return status
                return nt_status::INTERNAL_ERROR;

            new_ed.offset += start + length - k.offset;
            new_ed.num_bytes -= start + length - k.offset;

            // add new file extent
            status = insert_item(fs, *inode.r, {inode.tree.key, key_type::EXTENT_DATA, start + length},
                                 span((uint8_t*)&new_ed, sizeof(new_ed)));
            if (!success(status)) {
                print("insert_item returned {}\n", status);
                return status;
            }

            inode.ii.nbytes -= length;
            changed_inode = true;

            break;
        } else if (start + length > k.offset && start + length < k.offset + size) {
            // remove beginning

            auto new_key = k;

            new_key.offset += start + length - k.offset;

            ed.offset += start + length - k.offset;
            ed.num_bytes -= start + length - k.offset;

            inode.ii.nbytes -= start + length - k.offset;
            changed_inode = true;

            status = fs.change_key(p, new_key);
            if (!success(status)) {
                print("btrfs_filesystem:::change_key returned {}\n", status);
                return status;
            }
        } else if (start > k.offset && start < k.offset + size) {
            // remove end

            ed.num_bytes -= k.offset + size - start;

            inode.ii.nbytes -= k.offset + size - start;
            changed_inode = true;
        }

        if (!fs.cache.write(span(p.data[0], fs.sb->nodesize), th.bytenr)) // FIXME - return status
            return nt_status::INTERNAL_ERROR;

        p.slots[0]++;
    }

    if (changed_inode) {
        inode.ii.transid = fs.sb->generation;
        inode.ii.sequence++;

        status = inode.flush_inode_item(p);
        if (!success(status)) {
            print("btrfs_inode::flush_inode_item returned {}\n", status);
            return status;
        }
    }

    return nt_status::SUCCESS;
}

nt_status btrfs_inode::add_file_extent(uint64_t extent_address, uint64_t extent_length,
                                       uint64_t extent_offset, uint64_t file_offset,
                                       uint64_t length) {
    btrfs_file_extent_item fei;
    nt_status status;

    fei.generation = fs.sb->generation;
    fei.ram_bytes = extent_length;
    fei.compression = 0;
    fei.encryption = 0;
    fei.other_encoding = 0;
    fei.type = btrfs_extent_type::regular;
    fei.disk_bytenr = extent_address;
    fei.disk_num_bytes = extent_length;
    fei.offset = extent_offset;
    fei.num_bytes = length;

    status = insert_item(fs, *r, {tree.key, key_type::EXTENT_DATA, file_offset},
                         span((uint8_t*)&fei, sizeof(fei)));
    if (!success(status)) {
        print("insert_item returned {}\n", status);
        return status;
    }

    status = add_data_change(fs, extent_address, extent_length, r->id, tree.key,
                             file_offset, 1);
    if (!success(status)) {
        print("add_data_change returned {}\n", status);
        return status;
    }

    ii.nbytes += length;

    return nt_status::SUCCESS;
}

static nt_status add_csums2(btrfs_filesystem& fs, uint64_t addr, uint64_t sectors) {
    static const size_t CSUM_SIZE = sizeof(uint32_t); // FIXME

    btrfs_path p(fs.sb->nodesize);

    // FIXME - merge with prev or next csum items if contiguous and small enough

    auto status = insert_item2(fs, *fs.csum_root, { BTRFS_EXTENT_CSUM_OBJECTID, key_type::EXTENT_CSUM, addr },
                               (uint32_t)(sectors * CSUM_SIZE), p);
    if (!success(status))
        return status;

    auto& th = *(btrfs_header*)p.data[0];
    auto* items = (btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header));

    auto csums = span((uint32_t*)(p.data[0] + sizeof(btrfs_header) + items[p.slots[0]].offset),
                      items[p.slots[0]].size / CSUM_SIZE);

    page_cache_entry* src_p;
    for (uint64_t i = 0; i < sectors; i++) {

        {
            lock_guard lock(fs.cache.base.cache_lock);

            if (i == 0)
                src_p = fs.cache.find_page_cache_entry(addr >> PAGE_SHIFT);
            else
                src_p = fs.cache.next_page_cache_entry(*src_p);
        }

        csums[i] = ~calc_crc32c(0xffffffff, span((const uint8_t*)get_phys_address(*src_p),
                                                 fs.sb->sectorsize));
    }

    // copy tree back into page cache and mark dirty

    if (!fs.cache.write(span(p.data[0], fs.sb->nodesize), th.bytenr)) // FIXME - return status
        return nt_status::INTERNAL_ERROR;

    return nt_status::SUCCESS;
}

static nt_status add_csums(btrfs_filesystem& fs, uint64_t addr, uint64_t length) {
    static const size_t CSUM_SIZE = sizeof(uint32_t); // FIXME

    uint64_t num_pages = length >> PAGE_SHIFT;
    auto max_run = (uint64_t)((fs.sb->nodesize - sizeof(btrfs_header) - (sizeof(btrfs_item) * 2)) / CSUM_SIZE) - 1;

    do {
        auto status = add_csums2(fs, addr, min(num_pages, max_run));
        if (!success(status))
            return status;

        if (num_pages <= max_run)
            break;

        addr += max_run << PAGE_SHIFT;
        num_pages -= max_run;
    } while (true);

    return nt_status::SUCCESS;
}

nt_status flush_inline_inode(btrfs_inode& ino) {
    btrfs_filesystem& fs = ino.fs;
    auto size = (uint32_t)(offsetof(btrfs_file_extent_item, disk_bytenr) + ino.file_size);

    // FIXME - compression?

    btrfs_path p(fs.sb->nodesize);

    auto status = insert_item2(fs, *ino.r, { ino.tree.key, key_type::EXTENT_DATA, 0 },
                               size, p);
    if (!success(status))
        return status;

    auto& th = *(btrfs_header*)p.data[0];
    auto* items = (btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header));

    auto& ed = *(btrfs_file_extent_item*)(p.data[0] + sizeof(btrfs_header) + items[p.slots[0]].offset);

    ed.generation = fs.sb->generation;
    ed.ram_bytes = ino.file_size;
    ed.compression = 0;
    ed.encryption = 0;
    ed.other_encoding = 0;
    ed.type = btrfs_extent_type::inline_extent;

    page_cache_entry* src_p;

    ino.cache.pin(0, 1, nullptr);

    {
        lock_guard lock(ino.cache.base.cache_lock);
        src_p = ino.cache.find_page_cache_entry(0);
    }

    memcpy(&ed.disk_bytenr, get_phys_address(*src_p), ino.file_size);

    ino.cache.unpin(0, 1);

    // copy tree back into page cache and mark dirty

    if (!fs.cache.write(span(p.data[0], fs.sb->nodesize), th.bytenr)) // FIXME - return status
        return nt_status::INTERNAL_ERROR;

    ino.ii.nbytes = ino.file_size;

    return nt_status::SUCCESS;
}

nt_status btrfs_inode::flush_cache_holding_trans_lock(uint64_t page_num,
                                                      uint64_t num_pages) {
    nt_status status;
    btrfs_path p(fs.sb->nodesize);
    uint64_t addr;

    status = punch_hole(*this, page_num << PAGE_SHIFT, num_pages << PAGE_SHIFT);
    if (!success(status)) {
        print("punch_hole returned {}\n", status);
        return status;
    }

    if (file_size <= MAX_INLINE || inode_type == inode_type::symlink) {
        status = flush_inline_inode(*this);
        if (!success(status)) {
            print("flush_inline_inode returned {}\n", status);
            // FIXME - make sure hole-punching is undone
            return status;
        }
    } else {
        static const uint64_t MAX_EXTENT_SIZE = 0x8000000; // 128 MB

        // FIXME - max extent size should be 128 KB if compressed

        while (true) {
            auto pages = min(num_pages, MAX_EXTENT_SIZE >> PAGE_SHIFT);

            status = allocate_data(fs, pages << PAGE_SHIFT, addr);
            if (!success(status)) {
                print("allocate_data returned {}\n", status);
                // FIXME - make sure hole-punching is undone
                return status;
            }

            // FIXME - compression
            // FIXME - nodatacow

            // FIXME - allocate new chunk if we need to

            status = add_file_extent(addr, pages << PAGE_SHIFT, 0,
                                     page_num << PAGE_SHIFT, pages << PAGE_SHIFT);
            if (!success(status)) {
                print("btrfs_inode::add_file_extent returned {}\n", status);
                // FIXME - free addr
                // FIXME - make sure hole-punch doesn't get committed (go readonly?)
                return status;
            }

            status = fs.cache.pin(addr >> PAGE_SHIFT, pages, nullptr);
            if (!success(status)) {
                print("page_cache::pin returned {}\n", status);
                return status;
            }

            // reflink cache to filesystem cache and mark dirty

            {
                rangelock_req rlr(fs.cache.base.rl, addr >> PAGE_SHIFT, pages, rangelock_mode::exclusive);
                page_cache_entry* src_p;
                page_cache_entry* dest_p;

                for (uint64_t i = 0; i < pages; i++) {
                    {
                        lock_guard lock(cache.base.cache_lock);

                        if (i == 0)
                            src_p = cache.find_page_cache_entry(page_num);
                        else
                            src_p = cache.next_page_cache_entry(*src_p);
                    }

                    {
                        lock_guard lock(fs.cache.base.cache_lock);

                        if (i == 0)
                            dest_p = fs.cache.find_page_cache_entry(addr >> PAGE_SHIFT);
                        else
                            dest_p = fs.cache.next_page_cache_entry(*dest_p);
                    }

                    if (dest_p->pfn)
                        free_folio(dest_p->pfn << PAGE_SHIFT);

                    increase_page_refcount(src_p->pfn);

                    dest_p->pfn = src_p->pfn;
                    dest_p->dirty = 1;
                }
            }

            fs.cache.unpin(addr >> PAGE_SHIFT, pages);

            if (!(ii.flags & BTRFS_INODE_NODATASUM)) {
                status = add_csums(fs, addr, pages << PAGE_SHIFT);
                if (!success(status)) {
                    print("add_csums returned {}\n", status);
                    // FIXME - free addr
                    // FIXME - go readonly?
                    return status;
                }
            }

            if (pages == num_pages)
                break;

            page_num += pages;
            num_pages -= pages;
        }
    }

    ii.sequence++;

    status = flush_inode_item(p);
    if (!success(status)) {
        print("btrfs_inode::flush_inode_item returned {}\n", status);
        // FIXME - free addr (if not inline)
        // FIXME - go readonly?
        return status;
    }

    return nt_status::SUCCESS;
}

nt_status btrfs_inode::flush_cache(uint64_t page_num, uint64_t num_pages) {
    lock_guard lock(fs.trans_lock);

    return flush_cache_holding_trans_lock(page_num, num_pages);
}

static int64_t unix_to_win_time(const btrfs_timespec& t) {
    return (int64_t)((t.sec * 10000000) + (t.nsec / 100) + 116444736000000000);
}

static btrfs_timespec win_to_unix_time(int64_t t) {
    btrfs_timespec ret;

    t -= 116444736000000000;

    ret.sec = (uint64_t)(t / 10000000);
    ret.nsec = (uint32_t)((t % 10000000) * 100);

    return ret;
}

static NTSTATUS query_directory_dirent(btrfs_filesystem& fs, root& r, const btrfs_key& k,
                                       string_view sv, btrfs_inode_type di_type,
                                       span<uint8_t> buf, FILE_INFORMATION_CLASS file_information_class) {
    auto func = []<typename T>(btrfs_filesystem& fs, root& r, const btrfs_key& k,
                               string_view sv, btrfs_inode_type di_type,
                               span<uint8_t> buf) -> nt_status {
        if (buf.size() < offsetof(T, FileName))
            return nt_status::BUFFER_TOO_SMALL;

        auto utf16len = utf8_to_utf16_len(sv);
        auto len = offsetof(T, FileName) + (utf16len * sizeof(char16_t));

        if (buf.size() < len)
            return nt_status::BUFFER_TOO_SMALL;

        auto& fdi = *(T*)buf.data();

        // FIXME - subvolumes

        btrfs_inode* ino = nullptr;

        // FIXME - also look for root of ROOT_ITEM in inode cache
        if (k.type == key_type::INODE_ITEM) {
            lock_guard lock(r.inode_lock);

            auto* n = find_rbtree_node(r.inodes, k.objectid);

            if (n && n->key == k.objectid) {
                auto& ino2 = rbtree_containing_record<btrfs_inode>(n);

                ino2.acquire();
                ino = &ino2;
            }
        }

        if (ino) {
            fdi.CreationTime.QuadPart = unix_to_win_time(ino->ii.otime);
            fdi.LastAccessTime.QuadPart = unix_to_win_time(ino->ii.atime);
            fdi.LastWriteTime.QuadPart = ino->mtime;
            fdi.ChangeTime.QuadPart = ino->ctime;
            fdi.EndOfFile.QuadPart = (int64_t)ino->file_size;

            ino->release();
        } else {
            btrfs_key search_key;
            btrfs_path p(fs.sb->nodesize);

            switch (k.type) {
                case key_type::INODE_ITEM: {
                    search_key = k;

                    auto status = fs.find_item(r, search_key, p, false);
                    if (!success(status))
                        return status;

                    break;
                }

                case key_type::ROOT_ITEM: {
                    auto* r2 = fs.find_root(k.objectid);
                    if (!r2)
                        return nt_status::DISK_CORRUPT_ERROR;

                    search_key = btrfs_key{r2->ri.root_dirid, key_type::INODE_ITEM, 0};

                    auto status = fs.find_item(*r2, search_key, p, false);
                    if (!success(status))
                        return status;

                    break;
                }

                default:
                    return nt_status::DISK_CORRUPT_ERROR;
            }

            auto& th = *(btrfs_header*)p.data[0];
            auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

            if (p.slots[0] >= th.nritems)
                return nt_status::DISK_CORRUPT_ERROR;

            const auto& it = items[p.slots[0]];
            const auto& found_key = it.key;

            if (found_key.objectid != search_key.objectid || found_key.type != search_key.type)
                return nt_status::DISK_CORRUPT_ERROR;

            if (it.size < sizeof(btrfs_inode_item))
                return nt_status::DISK_CORRUPT_ERROR;

            auto& ii = *(btrfs_inode_item*)((const uint8_t*)&th + sizeof(btrfs_header) + it.offset);

            fdi.CreationTime.QuadPart = unix_to_win_time(ii.otime);
            fdi.LastAccessTime.QuadPart = unix_to_win_time(ii.atime);
            fdi.LastWriteTime.QuadPart = unix_to_win_time(ii.mtime);
            fdi.ChangeTime.QuadPart = unix_to_win_time(ii.ctime);
            fdi.EndOfFile.QuadPart = (int64_t)ii.size;
        }

        fdi.NextEntryOffset = 0;
        fdi.FileIndex = 0;
        fdi.AllocationSize.QuadPart = 0; // FIXME

        switch (di_type) {
            case btrfs_inode_type::directory:
                fdi.FileAttributes = FILE_ATTRIBUTE_DIRECTORY;
                break;

            case btrfs_inode_type::symlink:
                fdi.FileAttributes = FILE_ATTRIBUTE_REPARSE_POINT;
                break;

            default:
                fdi.FileAttributes = 0;
                break;
        }

        // FIXME - EaSize should contain reparse tag code?

        if constexpr (requires { T::EaSize; })
            fdi.EaSize = 0;

        if constexpr (requires { T::FileId; }) {
            if (k.type == key_type::INODE_ITEM)
                fdi.FileId.QuadPart = k.objectid;
            else
                fdi.FileId.QuadPart = BTRFS_FIRST_FREE_OBJECTID;
        }

        fdi.FileNameLength = (uint32_t)(utf16len * sizeof(char16_t));
        utf8_to_utf16(span(fdi.FileName, utf16len), sv);

        buf = buf.subspan(0, len);

        return nt_status::SUCCESS;
    };

    switch (file_information_class) {
        using enum FILE_INFORMATION_CLASS;

        case DirectoryInformation:
            return func.template operator()<FILE_DIRECTORY_INFORMATION>(fs, r, k, sv, di_type, buf);

        case IdFullDirectoryInformation:
            return func.template operator()<FILE_ID_FULL_DIR_INFORMATION>(fs, r, k, sv, di_type, buf);

        default:
            print("btrfs_filesystem::query_directory_dirent: unhandled info class {}\n",
                  file_information_class);
            return nt_status::INVALID_INFO_CLASS;
    }
}

nt_status btrfs_inode::query_directory_inode(string_view name, span<uint8_t> buf,
                                             FILE_INFORMATION_CLASS file_information_class) {
    btrfs_inode_type di_type;

    switch (inode_type) {
        case inode_type::directory:
            di_type = btrfs_inode_type::directory;
            break;

        case inode_type::symlink:
            di_type = btrfs_inode_type::symlink;
            break;

        default:
            di_type = btrfs_inode_type::file;
            break;
    }

    return query_directory_dirent(fs, *r, {tree.key, key_type::INODE_ITEM, 0 },
                                  name, di_type, buf, file_information_class);
}

NTSTATUS btrfs_inode::query_directory(span<uint8_t> buf, FILE_INFORMATION_CLASS file_information_class,
                                      uint64_t& index) {
    if (inode_type != inode_type::directory)
        return nt_status::NOT_A_DIRECTORY;

    char name[256]; // FIXME - reference page cache directly
    size_t name_len;

    shared_lock lg(fs.trans_lock);

    auto k = btrfs_key{tree.key, key_type::DIR_INDEX, index};
    btrfs_path p(fs.sb->nodesize);

    auto status = fs.find_item(*r, k, p, false);
    if (!success(status))
        return status;

    {
        auto& th = *(btrfs_header*)p.data[0];

        if (p.slots[0] >= th.nritems) {
            status = next_leaf(fs, p, *r, false);
            if (status == nt_status::NOT_FOUND)
                return nt_status::NO_MORE_FILES;
            else if (!success(status)) {
                print("btrfs_inode::query_directory: next_leaf returned {}\n", status);
                return status;
            }
        }
    }

    auto& th = *(btrfs_header*)p.data[0];
    auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

    const auto& it = items[p.slots[0]];
    const auto& found_key = it.key;

    if (found_key.objectid != k.objectid || found_key.type != k.type)
        return nt_status::NO_MORE_FILES;

    if (found_key.offset < index)
        return nt_status::NO_MORE_FILES;

    auto data = span((const uint8_t*)&th + sizeof(btrfs_header) + it.offset, it.size);

    // FIXME - return actual error if data malformed

    if (data.size() < offsetof(btrfs_dir_item, name))
        return nt_status::NO_MORE_FILES;

    const auto& di = *(btrfs_dir_item*)data.data();

    if (data.size() < offsetof(btrfs_dir_item, name) + di.data_len + di.name_len)
        return nt_status::NO_MORE_FILES;

    if (di.name_len > sizeof(name))
        return nt_status::NO_MORE_FILES;

    memcpy(name, di.name, di.name_len);
    name_len = di.name_len;

    auto ret = query_directory_dirent(fs, *r, di.location,
                                      string_view(name, name_len),
                                      di.type, buf, file_information_class);

    index = found_key.offset + 1;

    return ret;
}

static void write_superblocks(btrfs_filesystem& fs) {
    // FIXME - write the superblocks asynchronously and wait (will need multiple copies of superblock in memory, for the different addresses)

    for (auto addr : superblock_addrs) {
        if (addr + sizeof(btrfs_super_block) >= fs.vol->size)
            break;

        auto& sb = *fs.sb;

        sb.bytenr = addr;
        *(uint32_t*)&sb= ~calc_crc32c(0xffffffff, span((const uint8_t*)&sb.fsid, sizeof(btrfs_super_block) - sizeof(sb.csum)));

        auto iter = folio_iter(phys_memory_pfn(fs.sb) << PAGE_SHIFT);

        uint64_t written_pages;

        auto status = fs.vol->dio_write(addr >> PAGE_SHIFT,
                                        bytes_to_pages(sizeof(btrfs_super_block)),
                                        iter, written_pages, true);
        if (!success(status)) // FIXME - go readonly if this happens?
            print("volume::dio_write returned {}\n", status);
    }
}

static nt_status flush_root(btrfs_filesystem& fs, root& r) {
    if (r.id == BTRFS_ROOT_TREE_OBJECTID || r.id == BTRFS_CHUNK_TREE_OBJECTID)
        return nt_status::SUCCESS;

    btrfs_path p(fs.sb->nodesize);

    auto status = fs.find_item(*fs.root_root, {r.id, key_type::ROOT_ITEM, 0}, p, true);
    if (!success(status))
        return status;

    auto& th = *(btrfs_header*)p.data[0];
    auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

    if (p.slots[0] >= th.nritems) {
        print("Root item {:x} not found\n", r.id);
        return nt_status::DISK_CORRUPT_ERROR;
    }

    const auto& it = items[p.slots[0]];
    const auto& k = it.key;

    if (k.objectid != r.id || k.type != key_type::ROOT_ITEM) {
        print("Root item {:x} not found\n", r.id);
        return nt_status::DISK_CORRUPT_ERROR;
    }

    auto data = span((const uint8_t*)&th + sizeof(btrfs_header) + it.offset, it.size);

    if (data.size() < sizeof(btrfs_root_item)) {
        print("Root item truncated\n");
        return nt_status::DISK_CORRUPT_ERROR;
    }

    auto& ri = *(btrfs_root_item*)data.data();

    memcpy(&ri, &r.ri, sizeof(btrfs_root_item));

    // copy tree back into page cache and mark dirty

    if (!fs.cache.write(span(p.data[0], fs.sb->nodesize), th.bytenr)) // FIXME - return status
        return nt_status::INTERNAL_ERROR;

    return nt_status::SUCCESS;
}

static nt_status adjust_free_space_info(btrfs_filesystem& fs, uint64_t address,
                                        int delta) {
    auto& r = *fs.free_space_root;
    nt_status status;
    btrfs_path p(fs.sb->nodesize);
    chunk* c = find_chunk(fs, address);

    if (!c) {
        print("Could not find chunk for logical address {:x}\n", address);
        return nt_status::INTERNAL_ERROR;
    }

    status = fs.find_item(r, {c->offset, key_type::FREE_SPACE_INFO, c->ci.length},
                          p, true);
    if (!success(status))
        return status;

    auto& th = *(btrfs_header*)p.data[0];
    auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

    auto& it = items[p.slots[0]];
    const auto& found_key = it.key;

    if (found_key.objectid != c->offset || found_key.type != key_type::FREE_SPACE_INFO)
        return nt_status::INTERNAL_ERROR;

    auto data = span((const uint8_t*)&th + sizeof(btrfs_header) + it.offset, it.size);

    if (it.size < sizeof(btrfs_free_space_info))
        return nt_status::DISK_CORRUPT_ERROR;

    auto& fsi = *(btrfs_free_space_info*)data.data();

    fsi.extent_count = (uint32_t)((int32_t)fsi.extent_count + delta);

    // copy tree back into page cache and mark dirty

    if (!fs.cache.write(span(p.data[0], fs.sb->nodesize), th.bytenr)) // FIXME - return status
        return nt_status::INTERNAL_ERROR;

    return nt_status::SUCCESS;
}

static nt_status clear_free_space_info_bitmap_flag(btrfs_filesystem& fs,
                                                   chunk& c) {
    auto& r = *fs.free_space_root;
    nt_status status;
    btrfs_path p(fs.sb->nodesize);

    status = fs.find_item(r, {c.offset, key_type::FREE_SPACE_INFO, c.ci.length},
                          p, true);
    if (!success(status))
        return status;

    auto& th = *(btrfs_header*)p.data[0];
    auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

    auto& it = items[p.slots[0]];
    const auto& found_key = it.key;

    if (found_key.objectid != c.offset || found_key.type != key_type::FREE_SPACE_INFO)
        return nt_status::INTERNAL_ERROR;

    auto data = span((const uint8_t*)&th + sizeof(btrfs_header) + it.offset, it.size);

    if (it.size < sizeof(btrfs_free_space_info))
        return nt_status::DISK_CORRUPT_ERROR;

    auto& fsi = *(btrfs_free_space_info*)data.data();

    fsi.flags &= ~BTRFS_FREE_SPACE_USING_BITMAPS;

    // copy tree back into page cache and mark dirty

    if (!fs.cache.write(span(p.data[0], fs.sb->nodesize), th.bytenr)) // FIXME - return status
        return nt_status::INTERNAL_ERROR;

    return nt_status::SUCCESS;
}

static nt_status convert_fst_bitmaps_to_extents(btrfs_filesystem& fs, chunk& c,
                                                btrfs_path& p) {
    auto& r = *fs.free_space_root;
    nt_status status;
    list_entry list = { &list, &list };

    // remove bitmaps

    btrfs_key search_key = { c.offset, key_type::FREE_SPACE_BITMAP, 0 };

    status = fs.find_item(r, search_key, p, true);
    if (!success(status))
        return status;

    while (true) {
        auto& th = *(btrfs_header*)p.data[0];
        auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

        if (p.slots[0] == leaf_nritems(p)) {
            status = next_leaf(fs, p, r, true);
            if (status == nt_status::NOT_FOUND)
                break;
            else if (!success(status)) {
                while (!is_list_empty(list)) {
                    auto& s = containing_record<chunk_space>(list.next);

                    remove_entry_list(s.le);
                    free(&s);
                }

                return status;
            }
        }

        const auto& it = items[p.slots[0]];
        auto k = it.key;

        if (k.objectid >= c.offset + c.ci.length)
            break;

        if (k.objectid >= c.offset && k.type == key_type::FREE_SPACE_BITMAP) {
            auto data = span((const uint8_t*)&th + sizeof(btrfs_header) + it.offset, it.size);

            status = load_space_bitmap(fs, k.objectid, data, list);
            if (!success(status)) {
                while (!is_list_empty(list)) {
                    auto& s = containing_record<chunk_space>(list.next);

                    remove_entry_list(s.le);
                    free(&s);
                }

                return status;
            }

            status = fs.remove_item2(p, r);
            if (!success(status)) {
                while (!is_list_empty(list)) {
                    auto& s = containing_record<chunk_space>(list.next);

                    remove_entry_list(s.le);
                    free(&s);
                }

                return status;
            }

            search_key.objectid = k.objectid + k.offset;

            if (search_key.objectid >= c.offset + c.ci.length)
                break;

            status = fs.find_item(r, search_key, p, true);
            if (!success(status))
                return status;
        } else
            p.slots[0]++;
    }

    // insert extents

    while (!is_list_empty(list)) {
        auto& s = containing_record<chunk_space>(list.next);

        status = insert_item(fs, r, {s.offset, key_type::FREE_SPACE_EXTENT, s.length},
                             span<const uint8_t>());
        if (!success(status)) {
            while (!is_list_empty(list)) {
                auto& s2 = containing_record<chunk_space>(list.next);

                remove_entry_list(s2.le);
                free(&s2);
            }

            return status;
        }

        remove_entry_list(s.le);
        free(&s);
    }

    // clear flag

    status = clear_free_space_info_bitmap_flag(fs, c);
    if (!success(status))
        return status;

    c.using_fst_bitmaps = false;

    return nt_status::SUCCESS;
}

static nt_status remove_free_space(btrfs_filesystem& fs, chunk& c,
                                   uint64_t address, uint64_t length) {
    auto& r = *fs.free_space_root;
    nt_status status;
    btrfs_path p(fs.sb->nodesize);

    if (c.using_fst_bitmaps) {
        status = convert_fst_bitmaps_to_extents(fs, c, p);
        if (!success(status))
            return status;
    }

    status = fs.find_item(r, {address, key_type::FREE_SPACE_EXTENT, 0xffffffffffffffff},
                          p, true);
    if (!success(status))
        return status;

    while (true) {
        auto& th = *(btrfs_header*)p.data[0];
        auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

        if (p.slots[0] >= th.nritems) {
            status = fs.move_path_back(p, r, true);
            if (!success(status))
                return status;

            continue;
        }

        const auto& it = items[p.slots[0]];
        const auto& k = it.key;

        if (k.objectid > address) {
            status = fs.move_path_back(p, r, true);
            if (!success(status))
                return status;

            continue;
        }

        if (k.objectid > address || k.objectid + k.offset < address + length || k.type != key_type::FREE_SPACE_EXTENT)
            return nt_status::NOT_FOUND;

        break;
    }

    auto& th = *(btrfs_header*)p.data[0];
    auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

    const auto& it = items[p.slots[0]];
    auto k = it.key;

    if (k.objectid == address && k.offset == length) {
        // removing whole entry
        status = fs.remove_item2(p, r);
        if (!success(status))
            return status;

        status = adjust_free_space_info(fs, address, -1);
        if (!success(status))
            return status;
    } else if (k.objectid == address) {
        // removing start
        status = fs.change_key(p, {k.objectid + length, key_type::FREE_SPACE_EXTENT, k.offset - length});
        if (!success(status))
            return status;
    } else if (k.objectid + k.offset == address + length) {
        // removing end
        status = fs.change_key(p, {k.objectid, key_type::FREE_SPACE_EXTENT, k.offset - length});
        if (!success(status))
            return status;
    } else {
        // removing middle

        status = fs.change_key(p, {k.objectid, key_type::FREE_SPACE_EXTENT, address - k.objectid});
        if (!success(status))
            return status;

        status = insert_item(fs, r, {address + length, key_type::FREE_SPACE_EXTENT, k.objectid + k.offset - address - length},
                             span<const uint8_t>());
        if (!success(status))
            return status;

        status = adjust_free_space_info(fs, address, 1);
        if (!success(status))
            return status;
    }

    return nt_status::SUCCESS;
}

static nt_status add_free_space(btrfs_filesystem& fs, chunk& c, uint64_t address,
                                uint64_t length) {
    auto& r = *fs.free_space_root;
    nt_status status;
    btrfs_path p(fs.sb->nodesize);

    if (c.using_fst_bitmaps) {
        status = convert_fst_bitmaps_to_extents(fs, c, p);
        if (!success(status))
            return status;
    }

    status = fs.find_item(r, {address, key_type::FREE_SPACE_EXTENT, 0xffffffffffffffff},
                          p, true);
    if (!success(status))
        return status;

    auto& th = *(btrfs_header*)p.data[0];
    auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

    btrfs_key next_k;
    btrfs_key prev_k{0, (enum key_type)0, 0};

    if (p.slots[0] < th.nritems)
        next_k = items[p.slots[0]].key;
    else {
        status = fs.move_path_forward(p, r, false);
        if (success(status)) {
            auto& th = *(btrfs_header*)p.data[0];
            auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

            next_k = items[p.slots[0]].key;
        } else if (status == nt_status::NOT_FOUND)
            next_k = {0, (enum key_type)0, 0};
        else
            return status;
    }

    status = fs.move_path_back(p, r, true);

    if (success(status)) {
        auto& th2 = *(btrfs_header*)p.data[0];
        auto items2 = span((btrfs_item*)((uint8_t*)&th2 + sizeof(btrfs_header)), th2.nritems);

        if (items2[p.slots[0]].key.type == key_type::FREE_SPACE_EXTENT)
            prev_k = items2[p.slots[0]].key;
    } else if (status != nt_status::NOT_FOUND)
        return status;

    if (prev_k.type == key_type::FREE_SPACE_EXTENT &&
        next_k.type == key_type::FREE_SPACE_EXTENT &&
        prev_k.objectid + prev_k.offset == address &&
        next_k.objectid == address + length) {
        // bridge

        status = fs.change_key(p, {prev_k.objectid, key_type::FREE_SPACE_EXTENT, prev_k.offset + length + next_k.offset});
        if (!success(status))
            return status;

        status = fs.remove_item(r, next_k);
        if (!success(status))
            return status;

        status = adjust_free_space_info(fs, address, -1);
        if (!success(status))
            return status;

        return nt_status::SUCCESS;
    }

    if (prev_k.type == key_type::FREE_SPACE_EXTENT &&
        prev_k.objectid + prev_k.offset == address) {
        // extend previous
        return fs.change_key(p, {prev_k.objectid, key_type::FREE_SPACE_EXTENT, prev_k.offset + length});
    }

    if (next_k.type == key_type::FREE_SPACE_EXTENT &&
        next_k.objectid == address + length) {
        // extend next

        status = fs.move_path_forward(p, r, true);
        if (!success(status))
            return status;

        return fs.change_key(p, {address, key_type::FREE_SPACE_EXTENT, next_k.offset + length});
    }

    // new entry

    status = insert_item(fs, r, {address, key_type::FREE_SPACE_EXTENT, length},
                         span<const uint8_t>());
    if (!success(status))
        return status;

    status = adjust_free_space_info(fs, address, 1);
    if (!success(status))
        return status;

    return nt_status::SUCCESS;
}

static nt_status adjust_chunk_usage(btrfs_filesystem& fs, chunk& c, int64_t delta) {
    btrfs_path p(fs.sb->nodesize);
    btrfs_key search_key = {c.offset, key_type::BLOCK_GROUP_ITEM, c.ci.length};

    auto status = fs.find_item(*fs.block_group_root, search_key, p, true);
    if (!success(status))
        return status;

    auto& th = *(btrfs_header*)p.data[0];
    auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

    if (p.slots[0] >= th.nritems)
        return nt_status::DISK_CORRUPT_ERROR;

    const auto& it = items[p.slots[0]];
    const auto& k = it.key;

    if (k != search_key || it.size < sizeof(btrfs_block_group_item))
        return nt_status::DISK_CORRUPT_ERROR;

    auto& bgi = *(btrfs_block_group_item*)((uint8_t*)&th + sizeof(btrfs_header) + it.offset);

    bgi.used = (uint64_t)((int64_t)bgi.used + delta);

    if (!fs.cache.write(span(p.data[0], fs.sb->nodesize), th.bytenr)) // FIXME - return status
        return nt_status::INTERNAL_ERROR;

    fs.sb->bytes_used = (uint64_t)((int64_t)fs.sb->bytes_used + delta);

    return nt_status::SUCCESS;
}

static nt_status do_ref_change_metadata(btrfs_filesystem& fs, const ref_change& rc,
                                        chunk& c) {
    if (!c.space_loaded) {
        auto status = load_space(fs, c);
        if (!success(status)) {
            print("load_space returned {}\n", status);
            return status;
        }
    }

    if (rc.metadata.count == -1) {
        // FIXME - snapshotted metadata

        auto status = fs.remove_item(*fs.extent_root, {rc.tree.key, key_type::METADATA_ITEM, rc.metadata.level});
        if (!success(status)) {
            print("remove_item returned {}\n", status);
            return status;
        }

        status = add_free_space(fs, c, rc.tree.key, fs.sb->nodesize);
        if (!success(status)) {
            print("add_free_space returned {}\n", status);
            return status;
        }

        c.delta -= fs.sb->nodesize;

        auto sc = new chunk_space;
        if (!sc)
            return nt_status::INSUFFICIENT_RESOURCES;

        if (is_list_empty(c.pending_space_list))
            insert_tail_list(fs.space_pending_chunks, c.space_pending_le);

        sc->offset = rc.tree.key;
        sc->length = fs.sb->nodesize;
        insert_tail_list(c.pending_space_list, sc->le);
    } else if (rc.metadata.count == 1) {
        extent_item_tbr eitbr;

        eitbr.ei.refs = 1;
        eitbr.ei.generation = fs.sb->generation;
        eitbr.ei.flags = BTRFS_EXTENT_FLAG_TREE_BLOCK;
        eitbr.type = key_type::TREE_BLOCK_REF;
        eitbr.tbr.offset = rc.metadata.root;

        auto status = insert_item(fs, *fs.extent_root, {rc.tree.key, key_type::METADATA_ITEM, rc.metadata.level},
                                    span((uint8_t*)&eitbr, sizeof(eitbr)));
        if (!success(status)) {
            print("insert_item returned {}\n", status);
            return status;
        }

        status = remove_free_space(fs, c, rc.tree.key, fs.sb->nodesize);
        if (!success(status)) {
            print("remove_free_space returned {}\n", status);
            return status;
        }

        c.delta += fs.sb->nodesize;
    } else {
        print("invalid metadata ref change count {}\n", rc.metadata.count);
        return nt_status::INTERNAL_ERROR;
    }

    return nt_status::SUCCESS;
}

static nt_status remove_csums(btrfs_filesystem& fs, uint64_t address,
                              uint64_t length) {
    btrfs_path p(fs.sb->nodesize);
    btrfs_key search_key = { BTRFS_EXTENT_CSUM_OBJECTID, key_type::EXTENT_CSUM, address };

    static const size_t CSUM_SIZE = sizeof(uint32_t); // FIXME

    auto status = fs.find_item(*fs.csum_root, search_key, p, true);
    if (!success(status))
        return status;

    {
        auto& th = *(btrfs_header*)p.data[0];
        auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

        if (th.nritems == 0)
            return nt_status::SUCCESS;

        if (p.slots[0] == th.nritems)
            p.slots[0]--;
        else if (items[p.slots[0]].key.offset > address && p.slots[0] != 0)
            p.slots[0]--;
    }

    while (length > 0) {
        if (p.slots[0] == leaf_nritems(p)) {
            status = next_leaf(fs, p, *fs.csum_root, true);
            if (!success(status)) {
                print("next_leaf returned {}\n", status);
                return status;
            }
        }

        auto& th = *(btrfs_header*)p.data[0];
        auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

        const auto& it = items[p.slots[0]];
        const auto& k = it.key;

        if (k.offset >= address + length)
            break;

        if (k.type > key_type::EXTENT_CSUM)
            break;

        if (k.objectid > BTRFS_EXTENT_CSUM_OBJECTID)
            break;

        auto data = span((uint8_t*)&th + sizeof(btrfs_header) + it.offset, it.size);
        auto size = (it.size / CSUM_SIZE) * fs.sb->sectorsize; // FIXME - different if not crc32

        if (address <= k.offset && address + length >= k.offset + size) {
            // remove whole entry

            status = fs.remove_item2(p, *fs.csum_root);
            if (!success(status)) {
                print("btrfs_filesystem::remove_item2 returned {}\n", status);
                return status;
            }

            address += size;
            length -= size;

            if (length > 0) {
                search_key.offset = address;

                status = fs.find_item(*fs.csum_root, search_key, p, true);
                if (!success(status))
                    return status;
            }

            continue;
        } else if (address > k.offset && address + length < k.offset + size) {
            // remove middle of entry

            uint64_t first_sectors = (address - k.offset) / fs.sb->sectorsize;
            uint64_t second_sectors = (k.offset + size - address - length) / fs.sb->sectorsize;

            // FIXME - won't need this if we can modify page cache directly
            auto buf = (uint8_t*)vmalloc(second_sectors * CSUM_SIZE);
            if (!buf) {
                print("out of memory\n");
                return nt_status::INSUFFICIENT_RESOURCES;
            }

            memcpy(buf, data.data() + data.size() - (second_sectors * CSUM_SIZE),
                   second_sectors * CSUM_SIZE);

            fs.shorten_item(p, (uint32_t)(data.size() - (first_sectors * CSUM_SIZE)));

            status = insert_item(fs, *fs.csum_root,
                                 { BTRFS_EXTENT_CSUM_OBJECTID, key_type::EXTENT_CSUM, address + length },
                                 span(buf, second_sectors * CSUM_SIZE));
            if (!success(status)) {
                print("insert_item returned {}\n", status);
                vfree(buf, second_sectors * CSUM_SIZE);
                return status;
            }

            vfree(buf, second_sectors * CSUM_SIZE);

            length = 0;
        } else if (address + length > k.offset && address + length < k.offset + size) {
            // remove beginning

            auto new_key = k;

            auto delta_sectors = (address + length - k.offset) / fs.sb->sectorsize;

            new_key.offset += delta_sectors * fs.sb->sectorsize;

            // move item data backwards
            memmove(data.data(), data.data() + (delta_sectors * CSUM_SIZE),
                    data.size() - (delta_sectors * CSUM_SIZE));

            fs.shorten_item(p, (uint32_t)(delta_sectors * CSUM_SIZE));

            status = fs.change_key(p, new_key);
            if (!success(status)) {
                print("btrfs_filesystem:::change_key returned {}\n", status);
                return status;
            }

            length = 0;
        } else if (address > k.offset && address < k.offset + size) {
            // remove end

            auto delta_sectors = (k.offset + size - address) / fs.sb->sectorsize;

            fs.shorten_item(p, (uint32_t)(delta_sectors * CSUM_SIZE));

            length -= k.offset + size - address;
            address = k.offset + size;
        }

        p.slots[0]++;
    }

    return nt_status::SUCCESS;
}

static nt_status do_ref_change_data(btrfs_filesystem& fs, const ref_change& rc,
                                    chunk& c, const ref_change_data& rcd) {
    btrfs_path p(fs.sb->nodesize);
    btrfs_key key;
    nt_status status;

    if (!c.space_loaded) {
        status = load_space(fs, c);
        if (!success(status)) {
            print("load_space returned {}\n", status);
            return status;
        }
    }

    key = {rc.tree.key, key_type::EXTENT_ITEM, rc.data.extent_length};

    status = fs.find_item(*fs.extent_root, key, p, true);
    if (!success(status))
        return status;

    bool found;

    auto& th = *(btrfs_header*)p.data[0];
    auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

    found = p.slots[0] < th.nritems && items[p.slots[0]].key == key;

    // FIXME - if removing extent, remove from csum tree
    // FIXME - if new extent, remove from FST, add to BGI, and adjust superblock

    if (!found) {
        if (rcd.count > 0) {
            extent_item_edr eiedr;

            eiedr.ei.refs = (uint32_t)rcd.count;
            eiedr.ei.generation = fs.sb->generation;
            eiedr.ei.flags = BTRFS_EXTENT_FLAG_DATA;
            eiedr.type = key_type::EXTENT_DATA_REF;
            eiedr.edr.root = rcd.root;
            eiedr.edr.objectid = rcd.objectid;
            eiedr.edr.offset = rcd.offset;
            eiedr.edr.count = (uint32_t)rcd.count;

            status = insert_item(fs, *fs.extent_root, key,
                                 span((const uint8_t*)&eiedr, sizeof(eiedr)));
            if (!success(status)) {
                print("insert_item returned {} for {}\n", status, key);
                return status;
            }

            status = remove_free_space(fs, c, rc.tree.key, rc.data.extent_length);
            if (!success(status)) {
                print("remove_free_space returned {}\n", status);
                return status;
            }

            c.delta += rc.data.extent_length;

            return nt_status::SUCCESS;
        } else {
            print("do_ref_change_data: {:x}, {:x}: trying to decrease refcount on non-existent extent\n",
                  rc.tree.key, rc.data.extent_length);
            return nt_status::DISK_CORRUPT_ERROR;
        }
    }

    auto& it = items[p.slots[0]];

    // FIXME - handle non-inline extent items
    // FIXME - make sure inline ref items are in correct order

    auto data = span((const uint8_t*)&th + sizeof(btrfs_header) + it.offset, it.size);
    if (data.size() < sizeof(btrfs_extent_item)) {
        print("do_ref_change_data: extent item {} was {} bytes, expected at least {}\n",
              key, data.size(), sizeof(btrfs_extent_item));
        return nt_status::DISK_CORRUPT_ERROR;
    }

    auto& ei = *(btrfs_extent_item*)data.data();

    auto inl = span(data.data() + sizeof(btrfs_extent_item),
                    data.size() - sizeof(btrfs_extent_item));

    // FIXME - inline refs are in order, we might be able to give up early

    while (!inl.empty()) {
        auto type = (key_type)inl[0];

        inl = span(inl.data() + sizeof(key_type), inl.size() - sizeof(key_type));

        switch (type) {
            case key_type::TREE_BLOCK_REF:
                if (inl.size() < sizeof(tree_block_ref)) {
                    print("do_ref_change_data: extent item {} was truncated\n",
                          key);
                    return nt_status::DISK_CORRUPT_ERROR;
                }

                inl = span(inl.data() + sizeof(tree_block_ref),
                           inl.size() - sizeof(tree_block_ref));
            break;

            case key_type::SHARED_BLOCK_REF:
                if (inl.size() < sizeof(shared_block_ref)) {
                    print("do_ref_change_data: extent item {} was truncated\n",
                          key);
                    return nt_status::DISK_CORRUPT_ERROR;
                }

                inl = span(inl.data() + sizeof(shared_block_ref),
                           inl.size() - sizeof(shared_block_ref));
            break;

            case key_type::SHARED_DATA_REF:
                if (inl.size() < sizeof(btrfs_shared_data_ref)) {
                    print("do_ref_change_data: extent item {} was truncated\n",
                          key);
                    return nt_status::DISK_CORRUPT_ERROR;
                }

                inl = span(inl.data() + sizeof(btrfs_shared_data_ref),
                           inl.size() - sizeof(btrfs_shared_data_ref));
            break;

            case key_type::EXTENT_OWNER_REF:
                if (inl.size() < sizeof(btrfs_extent_owner_ref)) {
                    print("do_ref_change_data: extent item {} was truncated\n",
                          key);
                    return nt_status::DISK_CORRUPT_ERROR;
                }

                inl = span(inl.data() + sizeof(btrfs_extent_owner_ref),
                           inl.size() - sizeof(btrfs_extent_owner_ref));
            break;

            case key_type::EXTENT_DATA_REF: {
                if (inl.size() < sizeof(btrfs_extent_data_ref)) {
                    print("do_ref_change_data: extent item {} was truncated\n",
                          key);
                    return nt_status::DISK_CORRUPT_ERROR;
                }

                auto& edr = *(btrfs_extent_data_ref*)inl.data();

                if (edr.root != rcd.root ||
                    edr.objectid != rcd.objectid ||
                    edr.offset != rcd.offset) {
                    inl = span(inl.data() + sizeof(btrfs_extent_data_ref),
                               inl.size() - sizeof(btrfs_extent_data_ref));
                    break;
                }

                if (rcd.count > 0) {
                    edr.count += (uint32_t)rcd.count;
                    ei.refs += (uint32_t)rcd.count;

                    if (!fs.cache.write(span(p.data[0], fs.sb->nodesize), th.bytenr)) // FIXME - return status
                        return nt_status::INTERNAL_ERROR;

                    return nt_status::SUCCESS;
                }

                if (-rcd.count > (int32_t)edr.count) {
                    print("do_ref_change_data: trying to reduce refcount of ref in {} to negative value\n",
                          key);
                    return nt_status::DISK_CORRUPT_ERROR;
                }

                if (-rcd.count < (int32_t)edr.count) {
                    edr.count = (uint32_t)((int32_t)edr.count + rcd.count);
                    ei.refs = (uint64_t)((int64_t)ei.refs + rcd.count);

                    if (!fs.cache.write(span(p.data[0], fs.sb->nodesize), th.bytenr)) // FIXME - return status
                        return nt_status::INTERNAL_ERROR;

                    return nt_status::SUCCESS;
                }

                if (-rcd.count == (int32_t)ei.refs) {
                    status = add_free_space(fs, c, rc.tree.key, rc.data.extent_length);
                    if (!success(status)) {
                        print("add_free_space returned {}\n", status);
                        return status;
                    }

                    status = fs.remove_item2(p, *fs.extent_root);
                    if (!success(status)) {
                        print("btrfs_filesystem::remove_item2 returned {} for {}\n",
                              status, key);
                        return status;
                    }

                    c.delta -= rc.data.extent_length;

                    auto sc = new chunk_space;
                    if (!sc)
                        return nt_status::INSUFFICIENT_RESOURCES;

                    if (is_list_empty(c.pending_space_list))
                        insert_tail_list(fs.space_pending_chunks, c.space_pending_le);

                    sc->offset = rc.tree.key;
                    sc->length = rc.data.extent_length;
                    insert_tail_list(c.pending_space_list, sc->le);

                    status = remove_csums(fs, rc.tree.key, rc.data.extent_length);
                    if (!success(status)) {
                        print("remove_csums returned {}\n", status);
                        return status;
                    }

                    // FIXME - TRIM

                    return nt_status::SUCCESS;
                }

                // FIXME - if edr.count == -rc.data.count && ei.refs != edr.count, remove inline ref but keep extent item

                print("do_ref_change_data: FIXME - reduce refcount of ref {}\n", key);

                return nt_status::INTERNAL_ERROR;
            }

            default:
                print("do_ref_change_data: unexpected inline extent type {} in {}\n",
                      type, key);
                return nt_status::DISK_CORRUPT_ERROR;
        }
    }

    if (rcd.count < 0) {
        print("do_ref_change_data: trying to decrease refcount for non-existent ref in {}\n",
              key);
        return nt_status::DISK_CORRUPT_ERROR;
    }

    print("do_ref_change_data: FIXME - add new ref ({})\n", key);
    // FIXME - add new ref and increase overall refcount

    return nt_status::INTERNAL_ERROR;
}

void free_ref_change(rbtree_node<uint64_t>* tree) {
    auto& r = rbtree_containing_record<ref_change>(tree);

    // FIXME - is recursion going to be a problem here?

    if (tree->left)
        free_ref_change(tree->left);

    if (tree->right)
        free_ref_change(tree->right);

    free(&r);
}

static nt_status flush_new_chunk(btrfs_filesystem& fs, chunk& c) {
    nt_status status;
    btrfs_dev_extent de;
    btrfs_free_space_info fsi;
    btrfs_block_group_item bgi;
    btrfs_path p(fs.sb->nodesize);

    // insert DEV_EXTENT

    for (uint16_t i = 0; i < c.ci.num_stripes; i++) {
        auto& s = c.ci.stripe[i];

        de.chunk_tree = fs.chunk_root->id;
        de.chunk_objectid = BTRFS_FIRST_FREE_OBJECTID;
        de.chunk_offset = c.offset;
        de.length = c.ci.length;
        de.chunk_tree_uuid = fs.chunk_uuid;

        status = insert_item(fs, *fs.dev_root, { s.devid, key_type::DEV_EXTENT, s.offset },
                            span((uint8_t*)&de, sizeof(de)));
        if (!success(status))
            return status;
    }

    // insert CHUNK_ITEM

    status = insert_item(fs, *fs.chunk_root, { de.chunk_objectid, key_type::CHUNK_ITEM, c.offset },
                         span((uint8_t*)&c.ci, offsetof(btrfs_chunk, stripe) + (c.ci.num_stripes * sizeof(btrfs_stripe))));
    if (!success(status))
        return status;

    // insert FREE_SPACE_INFO

    fsi.extent_count = 1;
    fsi.flags = 0;

    status = insert_item(fs, *fs.free_space_root, { c.offset, key_type::FREE_SPACE_INFO, c.ci.length },
                         span((uint8_t*)&fsi, sizeof(fsi)));
    if (!success(status))
        return status;

    // insert FREE_SPACE_EXTENT

    status = insert_item(fs, *fs.free_space_root, { c.offset, key_type::FREE_SPACE_EXTENT, c.ci.length },
                         span((uint8_t*)nullptr, 0));
    if (!success(status))
        return status;

    // insert BLOCK_GROUP_ITEM

    bgi.used = 0;
    bgi.chunk_objectid = de.chunk_objectid;
    bgi.flags = c.ci.type;

    status = insert_item(fs, *fs.block_group_root, { c.offset, key_type::BLOCK_GROUP_ITEM, c.ci.length },
                         span((uint8_t*)&bgi, sizeof(bgi)));
    if (!success(status))
        return status;

    // update DEV_ITEM's bytes_used

    for (uint16_t i = 0; i < c.ci.num_stripes; i++) {
        auto& s = c.ci.stripe[i];

        status = update_dev_item_bytes_used(fs, s.devid, c.ci.length);
        if (!success(status))
            return status;
    }

    if (c.ci.type & BTRFS_BLOCK_GROUP_SYSTEM) {
        auto new_k = btrfs_key{BTRFS_FIRST_FREE_OBJECTID, key_type::CHUNK_ITEM, c.offset};

        size_t new_chunk_size = offsetof(btrfs_chunk, stripe) + (c.ci.num_stripes * sizeof(btrfs_stripe));

        if (fs.sb->sys_chunk_array.size() - fs.sb->sys_chunk_array_size < sizeof(btrfs_key) + new_chunk_size) {
            print("not enough room in superblock for new SYSTEM chunk\n");
            return nt_status::INTERNAL_ERROR;
        }

        auto chunk_array = span(fs.sb->sys_chunk_array.data(), fs.sb->sys_chunk_array_size);
        bool done = false;

        while (!chunk_array.empty()) {
            if (chunk_array.size() < sizeof(btrfs_key)) {
                print("Chunk array truncated\n");
                return nt_status::DISK_CORRUPT_ERROR;
            }

            auto& k = *(btrfs_key*)chunk_array.data();

            if (k > new_k) {
                memmove(chunk_array.data() + sizeof(btrfs_key) + new_chunk_size,
                        chunk_array.data(), chunk_array.size());

                k = new_k;
                memcpy(chunk_array.data() + sizeof(btrfs_key), &c.ci,
                       new_chunk_size);

                done = true;
                break;
            }

            if (k.type != key_type::CHUNK_ITEM) {
                print("Unexpected item in chunk array\n");
                return nt_status::DISK_CORRUPT_ERROR;
            }

            chunk_array = chunk_array.subspan(sizeof(btrfs_key));

            if (chunk_array.size() < offsetof(btrfs_chunk, stripe)) {
                print("Chunk array truncated\n");
                return nt_status::DISK_CORRUPT_ERROR;
            }

            auto& ci = *(btrfs_chunk*)chunk_array.data();
            size_t chunk_size = offsetof(btrfs_chunk, stripe) + (sizeof(btrfs_stripe) * ci.num_stripes);

            if (chunk_array.size() < chunk_size) {
                print("Chunk array truncated\n");
                return nt_status::DISK_CORRUPT_ERROR;
            }

            chunk_array = chunk_array.subspan(chunk_size);
        }

        if (!done) {
            auto& k = *(btrfs_key*)(fs.sb->sys_chunk_array.data() + fs.sb->sys_chunk_array_size);

            k = new_k;
            memcpy((uint8_t*)&k + sizeof(btrfs_key), &c.ci, new_chunk_size);
        }

        fs.sb->sys_chunk_array_size += (uint32_t)(sizeof(btrfs_key) + new_chunk_size);
    }

    return nt_status::SUCCESS;
}

static nt_status do_ref_changes(btrfs_filesystem& fs) {
    list_entry changed_chunks = { &changed_chunks, &changed_chunks };
    rbtree_node<uint64_t>* changes;

    while (!is_list_empty(fs.new_chunks)) {
        auto& c = *(chunk*)((uint8_t*)fs.new_chunks.next - offsetof(chunk, new_le));

        auto status = flush_new_chunk(fs, c);
        if (!success(status)) {
            print("flush_new_chunk returned {}\n", status);
            return status;
        }

        remove_entry_list(c.new_le);
    }

    {
        lock_guard lock(fs.ref_changes_lock);

        changes = fs.ref_changes;
        fs.ref_changes = nullptr;
    }

    if (!changes)
        return nt_status::SUCCESS;

    auto n = get_tree_minimum(changes);

    while (n) {
        // auto& rc = containing_record<ref_change>(changes.next);
        auto& rc = rbtree_containing_record<ref_change>(n);

        auto c = find_chunk(fs, rc.tree.key);
        if (!c) {
            print("no chunk found for address {:x}\n", rc.tree.key);
            return nt_status::INTERNAL_ERROR;
        }

        if (!c->changed) {
            insert_tail_list(changed_chunks, c->changed_le);
            c->changed = true;
            c->delta = 0;
        }

        if (rc.is_metadata) {
            if (rc.metadata.count != 0) {
                auto status = do_ref_change_metadata(fs, rc, *c);
                if (!success(status)) {
                    print("do_ref_change_metadata returned {}\n", status);
                    return status;
                }
            }
        } else {
            while (!is_list_empty(rc.data.rcds)) {
                auto& rcd = containing_record<ref_change_data>(rc.data.rcds.next);

                if (rcd.count != 0) {
                    auto status = do_ref_change_data(fs, rc, *c, rcd);
                    if (!success(status)) {
                        print("do_ref_change_data returned {}\n", status);
                        return status;
                    }
                }

                remove_entry_list(rcd.le);
                free(&rcd);
            }
        }

        n = next_rbtree_node(*n);
    }

    free_ref_change(changes);

    while (!is_list_empty(changed_chunks)) {
        auto& c = *(chunk*)((uint8_t*)changed_chunks.next - offsetof(chunk, changed_le));

        c.changed = false;
        remove_entry_list(c.changed_le);

        if (c.delta == 0)
            continue;

        if (auto status = adjust_chunk_usage(fs, c, c.delta); !success(status)) {
            print("adjust_chunk_usage returned {}\n", status);
            return status;
        }
    }

    return nt_status::SUCCESS;
}

static void update_superblock_backup(btrfs_filesystem& fs) {
    memmove(&fs.sb->super_roots[1], &fs.sb->super_roots[0],
            sizeof(btrfs_root_backup) * (fs.sb->super_roots.size() - 1));

    fs.sb->super_roots[0].tree_root = fs.sb->root;
    fs.sb->super_roots[0].tree_root_gen = fs.sb->generation;
    fs.sb->super_roots[0].tree_root_level = fs.sb->root_level;
    fs.sb->super_roots[0].chunk_root = fs.sb->chunk_root;
    fs.sb->super_roots[0].chunk_root_gen = fs.sb->chunk_root_generation;
    fs.sb->super_roots[0].chunk_root_level = fs.sb->chunk_root_level;
    fs.sb->super_roots[0].extent_root = fs.extent_root->ri.bytenr;
    fs.sb->super_roots[0].extent_root_gen = fs.extent_root->ri.generation;
    fs.sb->super_roots[0].extent_root_level = fs.extent_root->ri.level;
    fs.sb->super_roots[0].fs_root = fs.fs_root->ri.bytenr;
    fs.sb->super_roots[0].fs_root_gen = fs.fs_root->ri.generation;
    fs.sb->super_roots[0].fs_root_level = fs.fs_root->ri.level;
    fs.sb->super_roots[0].dev_root = fs.dev_root->ri.bytenr;
    fs.sb->super_roots[0].dev_root_gen = fs.dev_root->ri.generation;
    fs.sb->super_roots[0].dev_root_level = fs.dev_root->ri.level;
    fs.sb->super_roots[0].csum_root = fs.csum_root->ri.bytenr;
    fs.sb->super_roots[0].csum_root_gen = fs.csum_root->ri.generation;
    fs.sb->super_roots[0].csum_root_level = fs.csum_root->ri.level;
    fs.sb->super_roots[0].total_bytes = fs.sb->total_bytes;
    fs.sb->super_roots[0].bytes_used = fs.sb->bytes_used;
    fs.sb->super_roots[0].num_devices = fs.sb->num_devices;
}

static nt_status do_writeback_inode(btrfs_filesystem& fs, btrfs_inode& ino) {
    nt_status status;

    lock_guard lock(ino.inode_lock);

    if (ino.dead)
        return nt_status::SUCCESS;

    auto func = [&ino](uint64_t page_num, uint64_t num_pages) {
        return ino.flush_cache_holding_trans_lock(page_num, num_pages);
    };

    status = ino.cache.base.flush(0, 0xffffffffffffffff, true, func);
    if (!success(status)) {
        print("page_cache::flush returned {}\n", status);
        return status;
    }

    if (ino.file_size != ino.ii.size) {
        if (ino.file_size < ino.ii.size) {
            auto old_num_sectors = (ino.ii.size + fs.sb->sectorsize - 1) / fs.sb->sectorsize;
            auto new_num_sectors = (ino.file_size + fs.sb->sectorsize - 1) / fs.sb->sectorsize;

            if (new_num_sectors < old_num_sectors) {
                status = punch_hole(ino, new_num_sectors * fs.sb->sectorsize,
                                    (old_num_sectors - new_num_sectors) * fs.sb->sectorsize);
                if (!success(status)) {
                    print("punch_hole returned {}\n", status);
                    return status;
                }
            }
        }

        btrfs_path p(fs.sb->nodesize);

        ino.ii.size = ino.file_size;
        ino.ii.sequence++;

        status = ino.flush_inode_item(p);
        if (!success(status)) {
            print("btrfs_inode::flush_inode_item returned {}\n", status);
            return status;
        }
    }

    return nt_status::SUCCESS;
}

static nt_status do_writeback(btrfs_filesystem& fs) {
    list_entry writeback_local = { &writeback_local, &writeback_local };

    {
        lock_guard lock(fs.writeback_lock);

        while (!is_list_empty(fs.writeback_list)) {
            auto& ino = *(inode*)((uint8_t*)fs.writeback_list.next - offsetof(inode, writeback_le));

            remove_entry_list(ino.writeback_le);
            insert_tail_list(writeback_local, ino.writeback_le);
        }
    }

    while (!is_list_empty(writeback_local)) {
        auto& ino = *(inode*)((uint8_t*)writeback_local.next - offsetof(inode, writeback_le));

        remove_entry_list(ino.writeback_le);

        auto status = do_writeback_inode(fs, static_cast<btrfs_inode&>(ino));
        if (!success(status)) {
            print("do_writeback_inode returned {}\n", status);

            // avoid memory corruption by putting remaining issues back on list
            {
                lock_guard lock(fs.writeback_lock);
                insert_tail_list(fs.writeback_list, ino.writeback_le);

                while (!is_list_empty(writeback_local)) {
                    auto& ino2 = *(inode*)((uint8_t*)writeback_local.next - offsetof(inode, writeback_le));

                    remove_entry_list(ino2.writeback_le);
                    insert_tail_list(fs.writeback_list, ino2.writeback_le);
                }
            }

            return status;
        }

        ino.release();
    }

    return nt_status::SUCCESS;
}

nt_status btrfs_filesystem::flush(bool make_readonly) {
    lock_guard lock(trans_lock);

    nt_status status;

    // FIXME - should have rwsem held exclusively

    status = do_writeback(*this);
    if (!success(status)) {
        print("do_writeback returned {}, going readonly\n", status);
        go_readonly();
        return status;
    }

    if (is_list_empty(dirty_root_list) || readonly) { // no changes made
        if (make_readonly)
            go_readonly();

        return nt_status::SUCCESS;
    }

    do {
        status = do_ref_changes(*this);
        if (!success(status)) {
            print("do_ref_changes returned {}, going readonly\n", status);
            go_readonly();
            return status;
        }

        while (!is_list_empty(dirty_root_list)) {
            auto& r = *(root*)((uint8_t*)dirty_root_list.next - offsetof(root, dirty_le));

            auto status = flush_root(*this, r);
            if (!success(status)) {
                print("flush_root returned {}, going readonly\n", status);
                go_readonly();
                return status;
            }

            remove_entry_list(r.dirty_le);
            r.is_dirty = false;
        }
    } while (ref_changes || !is_list_empty(new_chunks));

    // loop through cache, and call flush_cache for dirty ranges

    while (true) {
        page_cache_entry* start;
        uint64_t num_pages;

        if (!cache.find_dirty_range(start, num_pages))
            break;

        auto first_page = start->tree.key;

        status = flush_cache(first_page, num_pages);

        if (!success(status)) {
            print("btrfs_filesystem::flush_cache returned {}, going readonly\n", status);
            go_readonly();
            return status;
        }
    }

    status = vol->flush();
    if (!success(status)) {
        print("volume::flush returned {}, going readonly\n", status);
        go_readonly();
        return status;
    }

    sb->root_level = root_root->ri.level;
    sb->root = root_root->ri.bytenr;

    sb->chunk_root = chunk_root->ri.bytenr;
    sb->chunk_root_generation = chunk_root->ri.generation;
    sb->chunk_root_level = chunk_root->ri.level;

    update_superblock_backup(*this);

    write_superblocks(*this);

    // FIXME - send SATA command again to ensure cache flushed

    sb->generation++;

    while (!is_list_empty(space_pending_chunks)) {
        auto& c = *(chunk*)((uint8_t*)space_pending_chunks.next - offsetof(chunk, space_pending_le));

        handle_pending_space(c);

        remove_entry_list(c.space_pending_le);
    }

    if (make_readonly)
        go_readonly();

    return nt_status::SUCCESS;
}

void btrfs_filesystem::flush_thread() {
    auto tim_ref = create_timer(true);
    if (!tim_ref) {
        print("create_timer failed, exiting flush thread\n");
        return;
    }

    auto& tim = static_cast<timer&>(tim_ref.get());

    int64_t due_time_ticks = 30 * SYSTEM_TIME_RESOLUTION; // 30 seconds
    auto due_time_ms = (int32_t)((due_time_ticks * 1000) / SYSTEM_TIME_RESOLUTION);

    auto status = tim.set(-due_time_ticks, nullptr, nullptr, false,
                          due_time_ms, nullptr);
    if (!success(status)) {
        print("timer::set returned {}, exiting flush thread\n", status);
        return;
    }

    while (true) {
        array<object*, 2> objs;

        objs[0] = &tim;
        objs[1] = &readonly_event.get();

        status = wait_for_objects(objs, true, true);

        if ((unsigned int)status == (unsigned int)nt_status::SUCCESS + 1) // readonly event
            break;

        auto status2 = flush(false);
        if (!success(status2))
            break;

        if (status != nt_status::SUCCESS) // thread has been killed
            break;
    }
}

void btrfs_filesystem::go_readonly() {
    if (readonly)
        return;

    readonly = true;

    set_event(static_cast<event&>(readonly_event.get()));
}

chunk::~chunk() {
    while (!is_list_empty(space_list)) {
        auto& s = containing_record<chunk_space>(space_list.next);

        remove_entry_list(s.le);
        free(&s);
    }
}

nt_status btrfs_inode::do_fchown(span<const uint8_t> buffer) {
    nt_status status;

    if (buffer.size() < sizeof(MN_FCHOWN))
        return nt_status::BUFFER_TOO_SMALL;

    auto& params = *(MN_FCHOWN*)buffer.data();

    auto now = (uint64_t)kernel_time_page.system_time + kernel_time_page.system_time_offset;

    {
        btrfs_path p(fs.sb->nodesize);
        lock_guard lock(fs.trans_lock);

        {
            lock_guard lock2(inode_lock);

            if (params.uid != 0xffffffff)
                ii.uid = params.uid;

            if (params.gid != 0xffffffff)
                ii.gid = params.gid;

            ctime = now;
            ii.transid = fs.sb->generation;
            ii.sequence++;

            if (!S_ISDIR(ii.mode)) {
                ii.mode &= ~S_ISUID;

                if (ii.mode & S_IXGRP)
                    ii.mode &= ~S_ISGID;
            }
        }

        status = flush_inode_item(p);
        if (!success(status)) {
            print("btrfs_inode::flush_inode_item returned {}\n", status);
            return status;
        }
    }

    return nt_status::SUCCESS;
}

nt_status btrfs_inode::do_fchmod(span<const uint8_t> buffer) {
    nt_status status;

    static const uint32_t MASK = 0xfff;

    if (buffer.size() < sizeof(uint32_t))
        return nt_status::BUFFER_TOO_SMALL;

    auto v = *(uint32_t*)buffer.data();

    v &= MASK;

    auto now = (uint64_t)kernel_time_page.system_time + kernel_time_page.system_time_offset;

    {
        btrfs_path p(fs.sb->nodesize);
        lock_guard lock(fs.trans_lock);

        {
            lock_guard lock2(inode_lock);

            ii.mode &= ~MASK;
            ii.mode |= v;

            ctime = now;
            ii.transid = fs.sb->generation;
            ii.sequence++;
        }

        status = flush_inode_item(p);
        if (!success(status)) {
            print("btrfs_inode::flush_inode_item returned {}\n", status);
            return status;
        }
    }

    return nt_status::SUCCESS;
}

NTSTATUS btrfs_inode::set_information(span<const uint8_t> buffer, dentry& dentry,
                                      FILE_INFORMATION_CLASS file_information_class) {
    if (fs.readonly)
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
            print("btrfs_inode::set_information: unhandled file information class {}\n",
                  file_information_class);
            return nt_status::NOT_IMPLEMENTED;
    }
}

nt_status btrfs_inode::set_basic_information(span<const uint8_t> buffer) {
    if (buffer.size() < sizeof(FILE_BASIC_INFORMATION))
        return nt_status::INVALID_PARAMETER;

    const auto& fbi = *(FILE_BASIC_INFORMATION*)buffer.data();

    if (fbi.CreationTime.QuadPart != 0 || fbi.LastAccessTime.QuadPart != 0 ||
        fbi.LastWriteTime.QuadPart != 0 || fbi.ChangeTime.QuadPart != 0) {

        btrfs_path p(fs.sb->nodesize);
        nt_status status;

        lock_guard lock(fs.trans_lock);

        status = fs.find_item(*r, {tree.key, key_type::INODE_ITEM, 0}, p, true);
        if (!success(status))
            return status;

        auto& th = *(btrfs_header*)p.data[0];
        auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

        auto& it = items[p.slots[0]];
        const auto& found_key = it.key;

        if (found_key.objectid != tree.key || found_key.type != key_type::INODE_ITEM)
            return nt_status::DISK_CORRUPT_ERROR;

        auto data = span((const uint8_t*)&th + sizeof(btrfs_header) + it.offset, it.size);

        if (it.size < sizeof(btrfs_inode_item))
            return nt_status::DISK_CORRUPT_ERROR;

        auto& ii = *(btrfs_inode_item*)data.data();

        ii.sequence++;

        // FIXME - setting times to -1 or -2
        // FIXME - subsequent writes etc. shouldn't clobber set time

        if (fbi.CreationTime.QuadPart != 0)
            ii.otime = win_to_unix_time(fbi.CreationTime.QuadPart);

        if (fbi.LastAccessTime.QuadPart != 0)
            ii.atime = win_to_unix_time(fbi.LastAccessTime.QuadPart);

        if (fbi.LastWriteTime.QuadPart != 0) {
            mtime = fbi.LastWriteTime.QuadPart;
            ii.mtime = system_time_to_btrfs_time(mtime);
        }

        if (fbi.ChangeTime.QuadPart != 0) {
            ctime = fbi.ChangeTime.QuadPart;
            ii.ctime = system_time_to_btrfs_time(ctime);
        }

        // copy tree back into page cache and mark dirty

        if (!fs.cache.write(span(p.data[0], fs.sb->nodesize), th.bytenr)) // FIXME - return status
            return nt_status::INTERNAL_ERROR;
    }

    // FIXME - FileAttributes

    return nt_status::SUCCESS;
}

static nt_status remove_dir_item(btrfs_filesystem& fs, root& r, uint64_t inode,
                                 string_view name, uint64_t parent) {
    nt_status status;
    optional<uint64_t> index;

    // lookup INODE_REF to get index

    {
        btrfs_path p(fs.sb->nodesize);
        auto k = btrfs_key{inode, key_type::INODE_REF, parent};

        status = fs.find_item(r, k, p, true);
        if (!success(status))
            return status;

        auto& th = *(btrfs_header*)p.data[0];
        auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

        if (p.slots[0] == th.nritems) {
            print("remove_dir_item: could not find {}\n", k);
            return nt_status::DISK_CORRUPT_ERROR;
        }

        const auto& it = items[p.slots[0]];
        const auto& found_key = it.key;

        if (found_key != k) {
            print("remove_dir_item: could not find {}\n", k);
            return nt_status::DISK_CORRUPT_ERROR;
        }

        auto data = span((const uint8_t*)&th + sizeof(btrfs_header) + it.offset, it.size);
        auto orig_data = data;

        while (data.size() >= offsetof(btrfs_inode_ref, name)) {
            const auto& ir = *(btrfs_inode_ref*)data.data();
            size_t len = offsetof(btrfs_inode_ref, name) + ir.name_len;

            if (data.size() < len)
                break;

            if (string_view(ir.name, ir.name_len) == name) {
                index = ir.index;

                if (orig_data.size() == len) {
                    status = fs.remove_item2(p, r);
                    if (!success(status))
                        return status;
                } else {
                    if (data.data() + len == orig_data.data() + orig_data.size())
                        fs.shorten_item(p, (uint32_t)len);
                    else {
                        auto buf = (uint8_t*)malloc(orig_data.size() - len);
                        if (!buf) {
                            print("malloc failed\n");
                            return nt_status::INSUFFICIENT_RESOURCES;
                        }

                        auto bef = (size_t)(data.data() - orig_data.data());

                        memcpy(buf, orig_data.data(), bef);
                        memcpy(buf + bef, data.data() + len,
                               orig_data.size() - bef - len);

                        fs.shorten_item(p, (uint32_t)len);

                        {
                            auto& th2 = *(btrfs_header*)p.data[0];
                            auto items2 = span((btrfs_item*)((uint8_t*)&th2 + sizeof(btrfs_header)),
                                               th2.nritems);
                            const auto& it2 = items2[p.slots[0]];
                            auto data2 = span((uint8_t*)&th2 + sizeof(btrfs_header) + it2.offset,
                                              it2.size);

                            memcpy(data2.data(), buf, data2.size());

                            if (!fs.cache.write(span(p.data[0], fs.sb->nodesize), th2.bytenr)) { // FIXME - return status
                                free(buf);
                                return nt_status::INTERNAL_ERROR;
                            }
                        }

                        free(buf);
                    }
                }

                break;
            }

            data = data.subspan(len, data.size() - len);
        }
    }

    // FIXME - INODE_EXTREF (getting index, removing) (if incompat flag set)

    if (!index.has_value()) {
        print("remove_dir_item: could not get index for {} in {:x}\n",
              name, parent);
        return nt_status::DISK_CORRUPT_ERROR;
    }

    status = fs.remove_item(r, {parent, key_type::DIR_INDEX, *index});
    if (!success(status))
        return status;

    // remove DIR_ITEM

    {
        btrfs_path p(fs.sb->nodesize);
        auto k = btrfs_key{parent, key_type::DIR_ITEM, calc_crc32c(0xfffffffe, name)};

        status = fs.find_item(r, k, p, true);
        if (!success(status))
            return status;

        auto& th = *(btrfs_header*)p.data[0];
        auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

        if (p.slots[0] == th.nritems) {
            print("remove_dir_item: could not find {}\n", k);
            return nt_status::DISK_CORRUPT_ERROR;
        }

        const auto& it = items[p.slots[0]];
        const auto& found_key = it.key;

        if (found_key != k) {
            print("remove_dir_item: could not find {}\n", k);
            return nt_status::DISK_CORRUPT_ERROR;
        }

        bool done = false;
        auto data = span((const uint8_t*)&th + sizeof(btrfs_header) + it.offset, it.size);
        auto orig_data = data;

        while (data.size() >= offsetof(btrfs_dir_item, name)) {
            const auto& di = *(btrfs_dir_item*)data.data();
            size_t len = offsetof(btrfs_dir_item, name) + di.data_len + di.name_len;

            if (data.size() < len)
                break;

            if (string_view(di.name, di.name_len) == name) {
                if (orig_data.size() == len) {
                    status = fs.remove_item2(p, r);
                    if (!success(status))
                        return status;
                } else {
                    if (data.data() + len == orig_data.data() + orig_data.size())
                        fs.shorten_item(p, (uint32_t)len);
                    else {
                        auto buf = (uint8_t*)malloc(orig_data.size() - len);
                        if (!buf) {
                            print("malloc failed\n");
                            return nt_status::INSUFFICIENT_RESOURCES;
                        }

                        auto bef = (size_t)(data.data() - orig_data.data());

                        memcpy(buf, orig_data.data(), bef);
                        memcpy(buf + bef, data.data() + len,
                               orig_data.size() - bef - len);

                        fs.shorten_item(p, (uint32_t)len);

                        {
                            auto& th2 = *(btrfs_header*)p.data[0];
                            auto items2 = span((btrfs_item*)((uint8_t*)&th2 + sizeof(btrfs_header)),
                                               th2.nritems);
                            const auto& it2 = items2[p.slots[0]];
                            auto data2 = span((uint8_t*)&th2 + sizeof(btrfs_header) + it2.offset,
                                              it2.size);

                            memcpy(data2.data(), buf, data2.size());

                            if (!fs.cache.write(span(p.data[0], fs.sb->nodesize), th2.bytenr)) { // FIXME - return status
                                free(buf);
                                return nt_status::INTERNAL_ERROR;
                            }
                        }

                        free(buf);
                    }
                }

                done = true;

                break;
            }

            data = data.subspan(len, data.size() - len);
        }

        if (!done) {
            print("remove_dir_item: failed to find \"{}\" in {}\n",
                  name, k);
            return nt_status::DISK_CORRUPT_ERROR;
        }
    }

    return nt_status::SUCCESS;
}

nt_status btrfs_inode::set_disposition_information(span<const uint8_t> buffer,
                                                   dentry& dentry) {
    nt_status status;

    if (fs.readonly)
        return nt_status::MEDIA_WRITE_PROTECTED;

    if (buffer.size() < sizeof(FILE_DISPOSITION_INFORMATION))
        return nt_status::INVALID_PARAMETER;

    if (dentry.inode == fs.root_inode)
        return nt_status::INVALID_PARAMETER;

    auto& bi = static_cast<btrfs_inode&>(*dentry.inode);

    if (bi.tree.key == SUBVOL_ROOT_INODE) {
        // FIXME - removing subvols
        print("btrfs_inode::set_disposition_information: FIXME - removing subvols\n");
        return nt_status::NOT_SUPPORTED;
    }

    const auto& fdi = *(FILE_DISPOSITION_INFORMATION*)buffer.data();

    // FIXME - this should actually be a non-POSIX deletion

    if (!fdi.DeleteFile) // FIXME
        return nt_status::NOT_SUPPORTED;

    // FIXME - make sure we can't move or rename file at the same time

    char* name_buf;
    size_t name_len;
    uint64_t parent_inode;

    while (true) {
        {
            lock_guard lock(dentry.lock);
            name_len = dentry.name_len;
        }

        assert(name_len != 0);

        name_buf = (char*)malloc(name_len);
        if (!name_buf) {
            print("malloc failed\n");
            return nt_status::INSUFFICIENT_RESOURCES;
        }

        bool done = false;

        {
            lock_guard lock(dentry.lock);

            if (dentry.name_len == name_len) {
                memcpy(name_buf, dentry.name, name_len);
                done = true;
            }

            assert(dentry.parent);

            auto& p = static_cast<btrfs_inode&>(*dentry.parent->inode);

            parent_inode = p.tree.key;
        }

        if (done)
            break;

        free(name_buf);
    }

    auto name = string_view(name_buf, name_len);

    auto now = (uint64_t)kernel_time_page.system_time + kernel_time_page.system_time_offset;

    {
        btrfs_path p(fs.sb->nodesize);
        lock_guard lock(fs.trans_lock);

        if (inode_type == inode_type::directory) {
            lock_guard lg(inode_lock);

            if (ii.size != 0)
                return nt_status::DIRECTORY_NOT_EMPTY;
        }

        {
            lock_guard lg(dentry.lock);

            if (dentry.status != dentry_status::valid) { // already done
                free(name_buf);
                return nt_status::SUCCESS;
            }

            dentry.status = dentry_status::going_away;
        }

        status = remove_dir_item(fs, *r, tree.key, name, parent_inode);
        if (!success(status)) {
            print("btrfs_inode::set_disposition_information: remove_dir_item returned {}\n", status);
            free(name_buf);
            return status;
        }

        // change size in parent

        auto& parent = static_cast<btrfs_inode&>(*dentry.parent->inode);

        {
            lock_guard lg(parent.inode_lock);

            parent.ii.size -= name.size() * 2;
            parent.ii.sequence++;
            parent.ctime = now;
            parent.mtime = now;
        }

        status = parent.flush_inode_item(p);
        if (!success(status)) {
            print("btrfs_inode::set_disposition_information: flush_inode_item 1 returned {}\n", status);
            free(name_buf);
            return status;
        }

        // change nlink in inode item

        bool orphaned;

        {
            lock_guard lg(inode_lock);

            ii.nlink--;
            ii.transid = fs.sb->generation;
            ii.sequence++;
            ctime = now;

            orphaned = ii.nlink == 0;
        }

        status = flush_inode_item(p);
        if (!success(status)) {
            print("btrfs_inode::set_disposition_information: flush_inode_item 2 returned {}\n", status);
            free(name_buf);
            return status;
        }

        // add orphan item if nlink now 0
        if (orphaned) {
            status = insert_item(fs, *r,
                                 {BTRFS_ORPHAN_OBJECTID, key_type::ORPHAN_INODE, tree.key},
                                 span<const uint8_t>());
            if (!success(status)) {
                print("btrfs_inode::set_disposition_information: insert_item returned {}\n", status);
                free(name_buf);
                return status;
            }
        }

        // adjust root_item

        r->ri.ctime = system_time_to_btrfs_time(now);
        r->ri.ctransid = fs.sb->generation;
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

    dentry.release();

    return nt_status::SUCCESS;
}

nt_status btrfs_inode::query_basic_information(span<uint8_t> buffer, uint64_t& retlen) {
    if (buffer.size() < sizeof(FILE_BASIC_INFORMATION))
        return nt_status::BUFFER_TOO_SMALL;

    auto& fbi = *(FILE_BASIC_INFORMATION*)buffer.data();

    {
        shared_lock lg(inode_lock);

        fbi.CreationTime.QuadPart = unix_to_win_time(ii.otime);
        fbi.LastAccessTime.QuadPart = unix_to_win_time(ii.atime);
        fbi.LastWriteTime.QuadPart = mtime;
        fbi.ChangeTime.QuadPart = ctime;

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

nt_status btrfs_inode::query_standard_information(span<uint8_t> buffer, uint64_t& retlen) {
    if (buffer.size() < sizeof(FILE_STANDARD_INFORMATION))
        return nt_status::BUFFER_TOO_SMALL;

    auto& fsi = *(FILE_STANDARD_INFORMATION*)buffer.data();

    {
        shared_lock lg(inode_lock);

        fsi.AllocationSize.QuadPart = (int64_t)ii.nbytes;
        fsi.EndOfFile.QuadPart = (int64_t)file_size;
        fsi.NumberOfLinks = ii.nlink;
        fsi.DeletePending = 0; // FIXME
        fsi.Directory = inode_type == inode_type::directory ? 1 : 0;
    }

    retlen = sizeof(fsi);

    return nt_status::SUCCESS;
}

nt_status btrfs_inode::query_internal_information(span<uint8_t> buffer, uint64_t& retlen) {
    if (buffer.size() < sizeof(FILE_INTERNAL_INFORMATION))
        return nt_status::BUFFER_TOO_SMALL;

    auto& fii = *(FILE_INTERNAL_INFORMATION*)buffer.data();

    fii.IndexNumber.QuadPart = (int64_t)tree.key;

    retlen = sizeof(fii);

    return nt_status::SUCCESS;
}

NTSTATUS btrfs_inode::query_information(span<uint8_t> buffer,
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
            print("btrfs_inode::query_information: unhandled file information class {}\n",
                  file_information_class);
            return nt_status::NOT_IMPLEMENTED;
    }
}

void btrfs_inode::last_handle_closed() {
    {
        shared_lock lg(inode_lock);

        if (ii.nlink != 0)
            return;
    }

    // FIXME - we can put this in a worker rather than doing it straightaway

    {
        lock_guard lg(fs.trans_lock);

        remove_orphan(false); // FIXME - what if it fails?
    }
}

nt_status btrfs_inode::remove_orphan(bool no_orphan_item) {
    nt_status status;
    uint64_t len;
    uint32_t sectorsize = fs.sb->sectorsize;

    // needs trans_lock held

    {
        shared_lock lg(inode_lock);
        len = ii.size;
    }

    if (len % sectorsize)
        len += sectorsize - (len % sectorsize);

    // FIXME - do in multiple transactions if particularly large?

    status = punch_hole(*this, 0, len);
    if (!success(status)) {
        print("btrfs_inode::remove_orphan: punch_hole returned {}\n", status);
        return status;
    }

    while (true) {
        btrfs_path p(fs.sb->nodesize);

        status = fs.find_item(*r, {tree.key, key_type::INODE_ITEM, 0}, p, true);
        if (status == nt_status::NOT_FOUND)
            break;
        else if (!success(status)) {
            print("btrfs_inode::remove_orphan: find_item returned {}\n", status);
            return status;
        }

        {
            auto& th = *(btrfs_header*)p.data[0];

            if (p.slots[0] >= th.nritems) {
                status = next_leaf(fs, p, *r, true);
                if (status == nt_status::NOT_FOUND)
                    break;
                else if (!success(status)) {
                    print("btrfs_inode::remove_orphan: next_leaf returned {}\n", status);
                    return status;
                }
            }
        }

        auto& th = *(btrfs_header*)p.data[0];
        auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)),
                          th.nritems);
        const auto& it = items[p.slots[0]];
        const auto& k = it.key;

        if (k.objectid > tree.key)
            break;

        assert(k.type != key_type::DIR_INDEX);
        assert(k.type != key_type::DIR_ITEM);
        assert(k.type != key_type::INODE_REF);
        assert(k.type != key_type::INODE_EXTREF);

        status = fs.remove_item2(p, *r);
        if (!success(status)) {
            print("btrfs_inode::remove_orphan: remove_item2 returned {}\n",
                  status);
            return status;
        }
    }

    if (!no_orphan_item) {
        status = fs.remove_item(*r, {BTRFS_ORPHAN_OBJECTID, key_type::ORPHAN_INODE, tree.key});
        if (!success(status)) {
            print("btrfs_inode::remove_orphan: remove_item returned {}\n", status);
            return status;
        }
    }

    {
        lock_guard lg2(inode_lock);
        dead = true;
    }

    return nt_status::SUCCESS;
}

static btrfs_inode_type inode_type_to_btrfs_inode_type(inode_type t) {
    switch (t) {
        using enum inode_type;

        case file:
            return btrfs_inode_type::file;
        case directory:
            return btrfs_inode_type::directory;
        case chardev:
            return btrfs_inode_type::chardev;
        case blockdev:
            return btrfs_inode_type::blockdev;
        case fifo:
            return btrfs_inode_type::fifo;
        case socket:
            return btrfs_inode_type::socket;
        case symlink:
            return btrfs_inode_type::symlink;
        case ea:
            return btrfs_inode_type::ea;
        default:
            return btrfs_inode_type::unknown;
    }
}

nt_status btrfs_inode::rename(::dentry& dentry, ::dentry& dest_dir,
                              string_view name, bool replace_if_exists,
                              object_ref& victim) {
    nt_status status;

    assert(dentry.parent);
    assert(dentry.parent->inode);

    if (!validate_name(name))
        return nt_status::OBJECT_NAME_INVALID;

    if (fs.readonly)
        return nt_status::MEDIA_WRITE_PROTECTED;

    auto& src_parent_ino = static_cast<btrfs_inode&>(*dentry.parent->inode);
    auto& dst_parent_ino = static_cast<btrfs_inode&>(*dest_dir.inode);
    auto cur_name = string_view(dentry.name, dentry.name_len);
    btrfs_path p(fs.sb->nodesize);

    // early return if trying to replace file with itself
    // FIXME - what does NT do here? Does it depend on replace_if_exists?
    if (&src_parent_ino == &dst_parent_ino && name == cur_name)
        return nt_status::SUCCESS;

    auto hash = calc_crc32c(0xfffffffe, name);
    auto type = inode_type_to_btrfs_inode_type(inode_type);

    // FIXME - lock inode to prevent simultaneous renames?

    uint64_t index;
    btrfs_key dir_item_key;
    bool done_overwrite = false;
    auto now = (uint64_t)kernel_time_page.system_time + kernel_time_page.system_time_offset;

    // FIXME - moving file between subvols
    if (src_parent_ino.r != dst_parent_ino.r)
        return nt_status::NOT_SAME_DEVICE;

    {
        lock_guard lg(fs.trans_lock);

        status = search_dir_item(fs, *r, dst_parent_ino.tree.key, hash,
                                 name, dir_item_key);
        if (!success(status) && status != nt_status::OBJECT_NAME_NOT_FOUND) {
            print("search_dir_item returned {}\n", status);
            return status;
        }

        if (success(status)) {
            if (!replace_if_exists)
                return nt_status::OBJECT_NAME_COLLISION;

            // don't allow if victim is root of subvol
            if (dir_item_key.type == key_type::ROOT_ITEM)
                return nt_status::ACCESS_DENIED;

            status = fs.open_inode(r->id, dir_item_key.objectid, victim);
            if (!success(status)) {
                print("open_inode returned {}\n", status);
                return status;
            }

            assert(victim.get().type == object_type::inode);

            auto& victim_ino = static_cast<btrfs_inode&>(victim.get());
            bool orphaned;

            {
                lock_guard lg2(victim_ino.inode_lock);

                if (victim_ino.inode_type == inode_type::directory) {
                    if (inode_type != inode_type::directory)
                        return nt_status::ACCESS_DENIED;

                    if (victim_ino.ii.size != 0)
                        return nt_status::DIRECTORY_NOT_EMPTY;
                } else {
                    if (inode_type == inode_type::directory)
                        return nt_status::ACCESS_DENIED;
                }

                victim_ino.ii.transid = fs.sb->generation;
                victim_ino.ii.nlink--;
                victim_ino.ii.sequence++;
                victim_ino.ctime = now;

                orphaned = victim_ino.ii.nlink == 0;
            }

            status = victim_ino.flush_inode_item(p);
            if (!success(status)) {
                print("btrfs_inode::flush_inode_item returned {}\n", status);
                return status;
            }

            status = remove_dir_item(fs, *r, dir_item_key.objectid,
                                     name, dst_parent_ino.tree.key);
            if (!success(status)) {
                print("remove_dir_item returned {}\n", status);
                return status;
            }

            if (orphaned) {
                if (victim_ino.handle_count == 0)
                    victim_ino.remove_orphan(true);
                else {
                    status = insert_item(fs, *r,
                                        {BTRFS_ORPHAN_OBJECTID, key_type::ORPHAN_INODE, victim_ino.tree.key},
                                        span<const uint8_t>());
                    if (!success(status)) {
                        print("insert_item returned {}\n", status);
                        return status;
                    }
                }
            }

            done_overwrite = true;
        }

        status = add_dir_item(fs, *r, dst_parent_ino, name, tree.key, type, index);
        if (!success(status)) {
            print("add_dir_item returned {}\n", status);
            return status;
        }

        // remove old DIR_ITEM, DIR_INDEX, and INODE_REF

        status = remove_dir_item(fs, *r, tree.key, cur_name,
                                 src_parent_ino.tree.key);
        if (!success(status)) {
            print("remove_dir_item returned {}\n", status);
            return status;
        }

        // create INODE_REF

        status = add_inode_ref(fs, *r, name, index, tree.key,
                               dst_parent_ino.tree.key);
        if (!success(status)) {
            print("add_inode_ref returned {}\n", status);
            return status;
        }

        // update inode item

        {
            lock_guard lg(inode_lock);

            ii.transid = fs.sb->generation;
            ii.sequence++;
            ctime = now;
        }

        status = flush_inode_item(p);
        if (!success(status)) {
            print("btrfs_inode::flush_inode_item returned {}\n", status);
            return status;
        }

        // adjust size etc. of parent directories

        {
            lock_guard lg(src_parent_ino.inode_lock);

            src_parent_ino.ii.transid = fs.sb->generation;

            // FIXME - size munging needs to be done in same lock as checking victim_ino size?

            if (&src_parent_ino == &dst_parent_ino && !done_overwrite)
                src_parent_ino.ii.size += name.size() * 2;

            src_parent_ino.ii.size -= cur_name.size() * 2;
            src_parent_ino.ii.sequence++;
            src_parent_ino.ctime = now;
            src_parent_ino.mtime = now;
        }

        if (&src_parent_ino != &dst_parent_ino) {
            lock_guard lg(dst_parent_ino.inode_lock);

            dst_parent_ino.ii.transid = fs.sb->generation;

            if (!done_overwrite)
                dst_parent_ino.ii.size += name.size() * 2;

            dst_parent_ino.ii.sequence++;
            dst_parent_ino.ctime = now;
            dst_parent_ino.mtime = now;
        }

        status = src_parent_ino.flush_inode_item(p);
        if (!success(status)) {
            print("btrfs_inode::flush_inode_item returned {}\n", status);
            return status;
        }

        if (&src_parent_ino != &dst_parent_ino) {
            status = dst_parent_ino.flush_inode_item(p);
            if (!success(status)) {
                print("btrfs_inode::flush_inode_item returned {}\n", status);
                return status;
            }
        }

        // adjust root_item

        r->ri.ctime = system_time_to_btrfs_time(now);
        r->ri.ctransid = fs.sb->generation;
    }

    return nt_status::SUCCESS;
}

nt_status btrfs_inode::hardlink(::dentry& dentry, ::dentry& dest_dir,
                                string_view name, bool replace_if_exists,
                                object_ref& victim) {
    nt_status status;

    assert(dentry.parent);
    assert(dentry.parent->inode);

    if (!validate_name(name))
        return nt_status::OBJECT_NAME_INVALID;

    if (fs.readonly)
        return nt_status::MEDIA_WRITE_PROTECTED;

    auto& src_parent_ino = static_cast<btrfs_inode&>(*dentry.parent->inode);
    auto& dst_parent_ino = static_cast<btrfs_inode&>(*dest_dir.inode);
    auto cur_name = string_view(dentry.name, dentry.name_len);
    btrfs_path p(fs.sb->nodesize);

    // early return if trying to replace file with itself
    // FIXME - what does NT do here? Does it depend on replace_if_exists?
    if (&src_parent_ino == &dst_parent_ino && name == cur_name)
        return nt_status::SUCCESS;

    auto hash = calc_crc32c(0xfffffffe, name);

    // FIXME - lock inode to prevent simultaneous renames?

    uint64_t index;
    btrfs_key dir_item_key;
    bool done_overwrite = false;
    auto now = (uint64_t)kernel_time_page.system_time + kernel_time_page.system_time_offset;
    auto type = inode_type_to_btrfs_inode_type(inode_type);

    if (src_parent_ino.r != dst_parent_ino.r)
        return nt_status::NOT_SAME_DEVICE;

    {
        lock_guard lg(fs.trans_lock);

        status = search_dir_item(fs, *r, dst_parent_ino.tree.key, hash,
                                 name, dir_item_key);
        if (!success(status) && status != nt_status::OBJECT_NAME_NOT_FOUND) {
            print("search_dir_item returned {}\n", status);
            return status;
        }

        if (success(status)) {
            if (!replace_if_exists)
                return nt_status::OBJECT_NAME_COLLISION;


            status = fs.open_inode(r->id, dir_item_key.objectid, victim);
            if (!success(status)) {
                print("open_inode returned {}\n", status);
                return status;
            }

            assert(victim.get().type == object_type::inode);

            auto& victim_ino = static_cast<btrfs_inode&>(victim.get());

            // don't allow if victim is directory
            if (victim_ino.inode_type == inode_type::directory)
                return nt_status::ACCESS_DENIED;

            bool orphaned;

            {
                lock_guard lg2(victim_ino.inode_lock);

                if (victim_ino.inode_type == inode_type::directory)
                    return nt_status::ACCESS_DENIED;

                victim_ino.ii.transid = fs.sb->generation;
                victim_ino.ii.nlink--;
                victim_ino.ii.sequence++;
                victim_ino.ctime = now;

                orphaned = victim_ino.ii.nlink == 0;
            }

            status = victim_ino.flush_inode_item(p);
            if (!success(status)) {
                print("btrfs_inode::flush_inode_item returned {}\n", status);
                return status;
            }

            status = remove_dir_item(fs, *r, dir_item_key.objectid,
                                     name, dst_parent_ino.tree.key);
            if (!success(status)) {
                print("remove_dir_item returned {}\n", status);
                return status;
            }

            if (orphaned) {
                if (victim_ino.handle_count == 0)
                    victim_ino.remove_orphan(true);
                else {
                    status = insert_item(fs, *r,
                                        {BTRFS_ORPHAN_OBJECTID, key_type::ORPHAN_INODE, victim_ino.tree.key},
                                        span<const uint8_t>());
                    if (!success(status)) {
                        print("insert_item returned {}\n", status);
                        return status;
                    }
                }
            }

            done_overwrite = true;
        }

        status = add_dir_item(fs, *r, dst_parent_ino, name, tree.key,
                              type, index);
        if (!success(status)) {
            print("add_dir_item returned {}\n", status);
            return status;
        }

        // create INODE_REF

        status = add_inode_ref(fs, *r, name, index, tree.key,
                               dst_parent_ino.tree.key);
        if (!success(status)) {
            print("add_inode_ref returned {}\n", status);
            return status;
        }

        // update inode item

        bool orphaned = false;

        {
            lock_guard lg(inode_lock);

            ii.transid = fs.sb->generation;
            ii.sequence++;
            ctime = now;

            if (ii.nlink == 0)
                orphaned = true;

            ii.nlink++;
        }

        if (orphaned) { // unorphan
            status = fs.remove_item(*r, {BTRFS_ORPHAN_OBJECTID, key_type::ORPHAN_INODE, tree.key});
            if (!success(status)) {
                print("btrfs_inode::hardlink: remove_item returned {}\n", status);
                return status;
            }
        }

        status = flush_inode_item(p);
        if (!success(status)) {
            print("btrfs_inode::flush_inode_item returned {}\n", status);
            return status;
        }

        // adjust size etc. of parent directories

        {
            lock_guard lg(dst_parent_ino.inode_lock);

            dst_parent_ino.ii.transid = fs.sb->generation;

            if (!done_overwrite)
                dst_parent_ino.ii.size += name.size() * 2;

            dst_parent_ino.ii.sequence++;
            dst_parent_ino.ctime = now;
            dst_parent_ino.mtime = now;
        }

        status = dst_parent_ino.flush_inode_item(p);
        if (!success(status)) {
            print("btrfs_inode::flush_inode_item returned {}\n", status);
            return status;
        }

        // adjust root_item

        r->ri.ctime = system_time_to_btrfs_time(now);
        r->ri.ctransid = fs.sb->generation;
    }

    return nt_status::SUCCESS;
}

nt_status btrfs_inode::fs_control(uint32_t code, span<const uint8_t> inbuf,
                                  span<uint8_t> outbuf, uint32_t& retlen,
                                  dentry* dentry) {
    switch (code) {
        case FSCTL_GET_REPARSE_POINT:
            return get_reparse_point(outbuf, retlen);

        case FSCTL_SET_REPARSE_POINT:
            if (!dentry)
                return nt_status::INVALID_PARAMETER;

            return set_reparse_point(inbuf, *dentry);

        default:
            return nt_status::NOT_SUPPORTED;
    }
}

nt_status btrfs_inode::get_reparse_point(span<uint8_t> outbuf, uint32_t& retlen) {
    if (outbuf.size() < offsetof(REPARSE_DATA_BUFFER, SymbolicLinkReparseBuffer.PathBuffer))
        return nt_status::BUFFER_TOO_SMALL;

    auto& rdb = *(REPARSE_DATA_BUFFER*)outbuf.data();
    size_t u16len, rdblen;
    unique_ptr<char[]> data;

    {
        lock_guard lg(inode_lock);

        if (inode_type != inode_type::symlink)
            return nt_status::NOT_A_REPARSE_POINT;

        if (file_size == 0 || file_size > PAGE_SIZE)
            return nt_status::INVALID_PARAMETER;

        data.reset(new char[file_size]);
        if (!data) {
            print("new failed\n");
            return nt_status::INSUFFICIENT_RESOURCES;
        }

        auto sp = span((uint8_t*)data.get(), file_size);
        auto status = cache.read(span(&sp, 1), 0);
        if (!success(status))
            return status;

        auto sv = string_view(data.get(), file_size);

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

static nt_status change_dir_item_type(btrfs_filesystem& fs, btrfs_path& p, root& r,
                                      btrfs_inode& parent, string_view name,
                                      enum btrfs_inode_type type) {
    nt_status status;

    auto hash = calc_crc32c(0xfffffffe, name);

    btrfs_key key{parent.tree.key, key_type::DIR_ITEM, hash};

    // look for DIR_ITEM

    status = fs.find_item(r, key, p, true);
    if (!success(status))
        return status;

    auto& th = *(btrfs_header*)p.data[0];
    auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

    if (p.slots[0] == th.nritems)
        return nt_status::DISK_CORRUPT_ERROR;

    const auto& it = items[p.slots[0]];

    if (it.key != key)
        return nt_status::DISK_CORRUPT_ERROR;

    auto data = span((const uint8_t*)&th + sizeof(btrfs_header) + it.offset, it.size);

    while (data.size() > offsetof(btrfs_dir_item, name)) {
        auto& di = *(btrfs_dir_item*)data.data();

        if (data.size() < offsetof(btrfs_dir_item, name) + di.name_len)
            return nt_status::DISK_CORRUPT_ERROR;

        if (di.data_len != 0)
            return nt_status::DISK_CORRUPT_ERROR;

        if (di.name_len == name.size() && !memcmp(di.name, name.data(), name.size())) {
            di.type = type;

            if (!fs.cache.write(span(p.data[0], fs.sb->nodesize), th.bytenr)) // FIXME - return status
                return nt_status::INTERNAL_ERROR;

            return nt_status::SUCCESS;
        }

        data = span(data.data() + offsetof(btrfs_dir_item, name) + di.name_len,
                    data.size() - offsetof(btrfs_dir_item, name) - di.name_len);
    }

    return nt_status::DISK_CORRUPT_ERROR;
}

static nt_status change_dir_index_type(btrfs_filesystem& fs, btrfs_path& p, root& r,
                                       btrfs_inode& parent, string_view name,
                                       uint64_t inode, enum btrfs_inode_type type) {
    nt_status status;
    optional<uint64_t> index;

    // lookup INODE_REF to get the index

    {
        auto k = btrfs_key{inode, key_type::INODE_REF, parent.tree.key};

        status = fs.find_item(r, k, p, false);
        if (!success(status))
            return status;

        auto& th = *(btrfs_header*)p.data[0];
        auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

        if (p.slots[0] == th.nritems) {
            print("change_dir_index_type: could not find {}\n", k);
            return nt_status::DISK_CORRUPT_ERROR;
        }

        const auto& it = items[p.slots[0]];
        const auto& found_key = it.key;

        if (found_key != k) {
            print("change_dir_index_type: could not find {}\n", k);
            return nt_status::DISK_CORRUPT_ERROR;
        }

        auto data = span((const uint8_t*)&th + sizeof(btrfs_header) + it.offset, it.size);

        while (data.size() >= offsetof(btrfs_inode_ref, name)) {
            const auto& ir = *(btrfs_inode_ref*)data.data();
            size_t len = offsetof(btrfs_inode_ref, name) + ir.name_len;

            if (data.size() < len)
                return nt_status::DISK_CORRUPT_ERROR;

            if (string_view(ir.name, ir.name_len) != name) {
                data = span(data.data() + len, data.size() - len);
                continue;
            }

            index = ir.index;
            break;
        }
    }

    // FIXME - INODE_EXTREF (getting index, removing) (if incompat flag set)

    if (!index.has_value()) {
        print("change_dir_index_type: could not get index for {} in {:x}\n",
              name, parent.tree.key);
        return nt_status::DISK_CORRUPT_ERROR;
    }

    btrfs_key key{parent.tree.key, key_type::DIR_INDEX, *index};

    // look for DIR_INDEX

    status = fs.find_item(r, key, p, true);
    if (!success(status))
        return status;

    auto& th = *(btrfs_header*)p.data[0];
    auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

    if (p.slots[0] == th.nritems)
        return nt_status::DISK_CORRUPT_ERROR;

    const auto& it = items[p.slots[0]];

    if (it.key != key)
        return nt_status::DISK_CORRUPT_ERROR;

    auto data = span((const uint8_t*)&th + sizeof(btrfs_header) + it.offset, it.size);

    if (data.size() < offsetof(btrfs_dir_item, name))
        return nt_status::DISK_CORRUPT_ERROR;

    auto& di = *(btrfs_dir_item*)data.data();

    if (data.size() < offsetof(btrfs_dir_item, name) + di.name_len)
        return nt_status::DISK_CORRUPT_ERROR;

    if (di.data_len != 0)
        return nt_status::DISK_CORRUPT_ERROR;

    if (di.name_len != name.size() || memcmp(di.name, name.data(), name.size()))
        return nt_status::DISK_CORRUPT_ERROR;

    di.type = type;

    if (!fs.cache.write(span(p.data[0], fs.sb->nodesize), th.bytenr)) // FIXME - return status
        return nt_status::INTERNAL_ERROR;

    return nt_status::SUCCESS;
}

nt_status btrfs_inode::set_reparse_point_symlink(span<const uint8_t> inbuf,
                                                 dentry& dentry) {
    auto& sb = *fs.sb;
    unique_ptr<char[]> u8;
    nt_status status;

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

    if (u8len > sb.sectorsize)
        return nt_status::INVALID_PARAMETER;

    u8.reset(new char[u8len]);
    if (!u8) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    utf16_to_utf8(span(u8.get(), u8len), u16sv);

    auto u8sv = string_view(u8.get(), u8len);

    {
        lock_guard lock(fs.trans_lock);
        btrfs_path p(sb.nodesize);

        // FIXME - rollback if this fails halfway through

        {
            lock_guard lock2(inode_lock);

            if (inode_type != inode_type::file)
                return nt_status::INVALID_PARAMETER;

            if (file_size != 0)
                return nt_status::INVALID_PARAMETER;

            if (ii.nlink != 1)
                return nt_status::INVALID_PARAMETER;

            inode_type = inode_type::symlink;

            // FIXME - change perms to 777?

            ii.mode &= ~S_IFREG;
            ii.mode |= S_IFLNK;

            file_size = ii.size = ii.nbytes = u8sv.size();
        }

        status = flush_inode_item(p);
        if (!success(status)) {
            print("btrfs_inode::flush_inode_item returned {}\n", status);
            return status;
        }

        auto& parent = static_cast<btrfs_inode&>(*dentry.parent->inode);

        status = change_dir_item_type(fs, p, *r, parent, string_view(dentry.name, dentry.name_len),
                                      btrfs_inode_type::symlink);
        if (!success(status)) {
            print("change_dir_item_type returned {}\n", status);
            return status;
        }

        status = change_dir_index_type(fs, p, *r, parent, string_view(dentry.name, dentry.name_len),
                                       tree.key, btrfs_inode_type::symlink);
        if (!success(status)) {
            print("change_dir_index returned {}\n", status);
            return status;
        }

        cache.write(span((uint8_t*)u8sv.data(), u8sv.size()), 0);

        status = flush_cache_holding_trans_lock(0, bytes_to_pages(sb.sectorsize));
        if (!success(status)) {
            print("flush_cache_holding_trans_lock returned {}\n", status);
            return status;
        }
    }

    return nt_status::SUCCESS;
}

nt_status btrfs_inode::set_reparse_point_fifo(dentry& dentry) {
    auto& sb = *fs.sb;
    nt_status status;

    {
        lock_guard lock(fs.trans_lock);
        btrfs_path p(sb.nodesize);

        // FIXME - rollback if this fails halfway through

        {
            lock_guard lock2(inode_lock);

            if (inode_type != inode_type::file)
                return nt_status::INVALID_PARAMETER;

            if (file_size != 0)
                return nt_status::INVALID_PARAMETER;

            if (ii.nlink != 1)
                return nt_status::INVALID_PARAMETER;

            inode_type = inode_type::fifo;

            ii.mode &= ~S_IFREG;
            ii.mode |= S_IFIFO;
        }

        status = flush_inode_item(p);
        if (!success(status)) {
            print("btrfs_inode::flush_inode_item returned {}\n", status);
            return status;
        }

        auto& parent = static_cast<btrfs_inode&>(*dentry.parent->inode);

        status = change_dir_item_type(fs, p, *r, parent, string_view(dentry.name, dentry.name_len),
                                      btrfs_inode_type::fifo);
        if (!success(status)) {
            print("change_dir_item_type returned {}\n", status);
            return status;
        }

        status = change_dir_index_type(fs, p, *r, parent, string_view(dentry.name, dentry.name_len),
                                       tree.key, btrfs_inode_type::fifo);
        if (!success(status)) {
            print("change_dir_index returned {}\n", status);
            return status;
        }
    }

    return nt_status::SUCCESS;
}

nt_status btrfs_inode::set_reparse_point(span<const uint8_t> inbuf,
                                         dentry& dentry) {
    if (inbuf.size() < offsetof(REPARSE_DATA_BUFFER, GenericReparseBuffer.DataBuffer))
        return nt_status::BUFFER_TOO_SMALL;

    auto& rdb = *(REPARSE_DATA_BUFFER*)inbuf.data();

    switch (rdb.ReparseTag) {
        case IO_REPARSE_TAG_SYMLINK:
            return set_reparse_point_symlink(inbuf, dentry);

        case IO_REPARSE_TAG_LX_FIFO:
            return set_reparse_point_fifo(dentry);

        default:
            return nt_status::INVALID_PARAMETER;
    }
}

nt_status btrfs_filesystem::query_volume(FS_INFORMATION_CLASS fs_information_class,
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

nt_status btrfs_filesystem::query_volume_size_info(span<uint8_t> buf,
                                                   uint32_t& retlen) {
    if (buf.size() < sizeof(FILE_FS_SIZE_INFORMATION))
        return nt_status::INVALID_PARAMETER;

    auto& info = *(FILE_FS_SIZE_INFORMATION*)buf.data();

    // FIXME - total_bytes and bytes_used should be divided by divisor for DATA RAID profile

    info.TotalAllocationUnits.QuadPart = (int64_t)(sb->total_bytes / sb->sectorsize);
    info.AvailableAllocationUnits.QuadPart = (int64_t)((sb->total_bytes - sb->bytes_used) / sb->sectorsize);
    info.SectorsPerAllocationUnit = 1;
    info.BytesPerSector = sb->sectorsize;

    retlen = sizeof(FILE_FS_SIZE_INFORMATION);

    return nt_status::SUCCESS;
}

nt_status btrfs_filesystem::query_volume_attribute_info(span<uint8_t> buf,
                                                        uint32_t& retlen) {
    static const u16string_view name = u"btrfs";

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

    info.MaximumComponentNameLength = 255;
    info.FileSystemNameLength = (uint32_t)(name.size() * sizeof(char16_t));
    memcpy(info.FileSystemName, name.data(), name_len);

    retlen = (uint32_t)(offsetof(FILE_FS_ATTRIBUTE_INFORMATION, FileSystemName) + name_len);

    if (name_len < name.size() * sizeof(char16_t))
        return nt_status::BUFFER_OVERFLOW;

    return nt_status::SUCCESS;
}

nt_status btrfs_inode::query_lx_information(span<uint8_t> buffer, uint64_t& retlen) {
    if (buffer.size() < sizeof(FILE_STAT_LX_INFORMATION))
        return nt_status::BUFFER_TOO_SMALL;

    auto& fsli = *(FILE_STAT_LX_INFORMATION*)buffer.data();

    {
        shared_lock lg(inode_lock);

        fsli.FileId.QuadPart = (int64_t)tree.key;
        fsli.CreationTime.QuadPart = unix_to_win_time(ii.otime);
        fsli.LastAccessTime.QuadPart = unix_to_win_time(ii.atime);
        fsli.LastWriteTime.QuadPart = mtime;
        fsli.ChangeTime.QuadPart = ctime;
        fsli.AllocationSize.QuadPart = (int64_t)ii.nbytes;
        fsli.EndOfFile.QuadPart = (int64_t)file_size;
        fsli.NumberOfLinks = ii.nlink;
        fsli.EffectiveAccess = 0; // FIXME
        fsli.LxFlags = LX_FILE_METADATA_HAS_UID | LX_FILE_METADATA_HAS_GID |
                       LX_FILE_METADATA_HAS_MODE;
        fsli.LxUid = ii.uid;
        fsli.LxGid = ii.gid;
        fsli.LxMode = ii.mode;

        if (inode_type == inode_type::blockdev || inode_type == inode_type::chardev) {
            fsli.LxDeviceIdMajor = (uint32_t)((ii.rdev & 0xfffffffffff00000) >> 20);
            fsli.LxDeviceIdMinor = ii.rdev & 0xfffff;
        } else {
            fsli.LxDeviceIdMajor = 0;
            fsli.LxDeviceIdMinor = 0;
        }

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
