// SPDX-License-Identifier: GPL-2.0-only
module;

#include <span>
#include <optional>
#include <atomic>
#include <string_view>
#include <mutex>
#include <new>
#include <algorithm>
#include <variant>
#include <stddef.h>
#include <stdint.h>
#include <signal.h>
#include <sys/uio.h>
#include "assert.h"
#include "timer-asm.h"
#include "trace.h"

module process;

import print;
import format;
import bsod;
import config;
import vfs;
import interrupts;
import visit;
import unicode;
import section;
import thread;
import rangelock;
import rwsem;
import scheduler;
import smp;
import disable_interrupts;
import cpu_block;
import ntdll;
import mapping;
import crt;
import kernel_time_page;
import version;
import pe;
import page_cache;
import mem;
import cmdline;
import chardev;
import console;
import timer;
import thread;
import event;
import moxnox_ioctls;
import pty;
import vec;
import trace;

#ifdef __x86_64__
import apic;
#endif

using namespace std;

static rwsem ipi_lock;

static const uint64_t MAX_USERSPACE_ADDRESS = 0xffffffffffff;

extern "C++" {
    section* ntdll = nullptr;
    section* time_page = nullptr;
    section* zero_section = nullptr;
}

static uintptr_t mn_start_thread;

static_assert(sizeof(TEB) == 0x1818);
static_assert(sizeof(PEB) == 0x388);
static_assert(sizeof(RTL_USER_PROCESS_PARAMETERS) == 0x440);

static const size_t MAX_PATH = 260;

static atomic<uintptr_t> next_pid = 1;

static spinlock process_list_lock;
static rbtree_node<uintptr_t>* process_tree = nullptr;
static size_t process_shutdown_ratchet;
static process* process_shutdown_ignore;
static bool do_process_shutdown_ratchet;
static object_ref process_shutdown_event;

static spinlock pg_tree_lock;
static rbtree_node<uintptr_t>* pg_tree = nullptr;

static const int LAST_SIG = SIGTTOU;

#ifdef __x86_64__
union page_fault_error_code {
    uint64_t value;
    struct {
        uint64_t present : 1;
        uint64_t write : 1;
        uint64_t user : 1;
        uint64_t reserved_write : 1;
        uint64_t instruction_fetch : 1;
        uint64_t protection_key : 1;
        uint64_t shadow_stack : 1;
        uint64_t reserved1 : 8;
        uint64_t sgx : 1;
        uint64_t reserved2 : 48;
    };
};

static_assert(sizeof(page_fault_error_code) == 8);
#endif

#ifdef __aarch64__
using pte_type = ARM64_HARDWARE_PTE;
#elifdef __x86_64__
using pte_type = HARDWARE_PTE;
#endif

static nt_status pin_userspace_buffers_for_read_and_write(process& proc,
                                                          span<const uint8_t> src_us_buf,
                                                          span<uint8_t> dest_us_buf,
                                                          optional<rangelock_req>& rlr,
                                                          span<const uint8_t>*& src_spans,
                                                          size_t& num_src_spans,
                                                          span<uint8_t>*& dest_spans,
                                                          size_t& num_dest_spans);
static nt_status set_pending_signal2(process& proc, int sig);

class time_page_cache : public custom_section_cache {
public:
    NTSTATUS read_into_cache(uint64_t page_num, uint64_t num_pages,
                             uint64_t& read_pages) override;

    NTSTATUS flush_cache(uint64_t, uint64_t) override {
        // shouldn't happen, always mapped readonly
        return nt_status::INTERNAL_ERROR;
    }

    NTSTATUS query_info(enum SECTION_INFORMATION_CLASS, span<uint8_t>,
                        size_t*) override {
        return nt_status::INVALID_INFO_CLASS;
    }

    nt_status ioctl(uint32_t, span<const uint8_t>, span<uint8_t>,
                    uintptr_t&) override {
        return nt_status::NOT_SUPPORTED;
    }
};

class zero_cache : public custom_section_cache {
public:
    nt_status read_into_cache(uint64_t page_num, uint64_t num_pages,
                              uint64_t& read_pages) override;

    nt_status flush_cache(uint64_t, uint64_t) override {
        // shouldn't happen
        return nt_status::INTERNAL_ERROR;
    }

    nt_status query_info(enum SECTION_INFORMATION_CLASS, span<uint8_t>,
                         size_t*) override {
        return nt_status::INVALID_INFO_CLASS;
    }

    nt_status ioctl(uint32_t, span<const uint8_t>, span<uint8_t>,
                    uintptr_t&) override {
        return nt_status::NOT_SUPPORTED;
    }
};

// FIXME - 32-bit processes

static size_t dos_path_len(string_view s) {
    // FIXME - resolve paths to DOS paths properly

    auto len = utf8_to_utf16_len(s);

    len += 2; // for "C:"

    return len;
}

static void create_dos_path(span<char16_t> buf, string_view s) {
    // FIXME - resolve paths to DOS paths properly

    if (!buf.empty()) {
        buf[0] = 'C';
        buf = buf.subspan(1);
    }

    if (!buf.empty()) {
        buf[0] = ':';
        buf = buf.subspan(1);
    }

    utf8_to_utf16(buf, s);

    for (auto& c : buf) {
        if (c == '/')
            c = '\\';
    }
}

static bool allocate_proc_params(process& proc, HANDLE stdinh, HANDLE stdouth,
                                 HANDLE stderrh, HANDLE curdirh,
                                 string_view curdir, string_view fn,
                                 u16string_view cmdline) {
    // FIXME - also include space for environment, etc.

    auto curdir_len = dos_path_len(curdir);

    size_t size;

    size = sizeof(RTL_USER_PROCESS_PARAMETERS);
    size += MAX_PATH * sizeof(char16_t);
    size += fn.size() * sizeof(char16_t);
    size += (cmdline.size() + 1) * sizeof(char16_t);

    if (auto status = proc.allocate_vm((void**)&proc.proc_params, 0, &size, MEM_COMMIT | MEM_RESERVE, mapping_type::read_write); !success(status)) {
        print("process::allocate_vm returned {}\n", status);
        return false;
    }

    // auto num_pages = bytes_to_pages(size);s

    auto pm = find_mapping(proc, (uintptr_t)proc.proc_params);
    if (!pm) {
        print("allocate_proc_params: find_mapping failed\n");
        return false;
    }

    auto& m = *pm;

    auto ks_proc_params = (RTL_USER_PROCESS_PARAMETERS*)malloc(sizeof(RTL_USER_PROCESS_PARAMETERS));
    if (!ks_proc_params) {
        print("malloc failed\n");
        m.release();
        return false;
    }

    memset(ks_proc_params, 0, sizeof(*ks_proc_params));

    ks_proc_params->StandardInput = stdinh;
    ks_proc_params->StandardOutput = stdouth;
    ks_proc_params->StandardError = stderrh;
    ks_proc_params->CurrentDirectory.Handle = curdirh;

    ks_proc_params->CurrentDirectory.DosPath.Length = (uint16_t)(min(MAX_PATH, curdir_len) * sizeof(char16_t));
    ks_proc_params->CurrentDirectory.DosPath.MaximumLength = MAX_PATH * sizeof(char16_t);
    ks_proc_params->CurrentDirectory.DosPath.Buffer = (char16_t*)((uint8_t*)proc.proc_params + sizeof(RTL_USER_PROCESS_PARAMETERS));

    ks_proc_params->ImagePathName.Length = (uint16_t)(fn.size() * sizeof(char16_t));
    ks_proc_params->ImagePathName.MaximumLength = ks_proc_params->ImagePathName.Length + sizeof(char16_t);
    ks_proc_params->ImagePathName.Buffer = ks_proc_params->CurrentDirectory.DosPath.Buffer +
        (ks_proc_params->CurrentDirectory.DosPath.MaximumLength / sizeof(char16_t));

    ks_proc_params->CommandLine.Length = (uint16_t)(cmdline.size() * sizeof(char16_t));
    ks_proc_params->CommandLine.MaximumLength = (uint16_t)((cmdline.size() + 1) * sizeof(char16_t));
    ks_proc_params->CommandLine.Buffer = ks_proc_params->ImagePathName.Buffer +
        (ks_proc_params->ImagePathName.MaximumLength / sizeof(char16_t));

    auto dos_path = (char16_t*)malloc(ks_proc_params->CurrentDirectory.DosPath.MaximumLength);
    if (!dos_path) {
        print("malloc failed\n");
        m.release();
        free(ks_proc_params);
        return false;
    }

    create_dos_path(span(dos_path, ks_proc_params->CurrentDirectory.DosPath.MaximumLength / sizeof(char16_t)), curdir);

    if (!m.copy_to((uint32_t)((uintptr_t)ks_proc_params->CurrentDirectory.DosPath.Buffer - m.tree.key),
                   dos_path, ks_proc_params->CurrentDirectory.DosPath.Length)) {
        print("mapping::copy_to failed\n");
        m.release();
        free(dos_path);
        free(ks_proc_params);
        return false;
    }

    free(dos_path);

    auto fn_path = (char16_t*)malloc(ks_proc_params->ImagePathName.MaximumLength);
    if (!fn_path) {
        print("malloc failed\n");
        m.release();
        free(ks_proc_params);
        return false;
    }

    utf8_to_utf16(span(fn_path, ks_proc_params->ImagePathName.MaximumLength / sizeof(char16_t)), fn);

    if (!m.copy_to((uint32_t)((uintptr_t)ks_proc_params->ImagePathName.Buffer - m.tree.key),
                   fn_path, ks_proc_params->ImagePathName.Length)) {
        print("mapping::copy_to failed\n");
        m.release();
        free(fn_path);
        free(ks_proc_params);
        return false;
    }

    free(fn_path);

    if (!m.copy_to((uint32_t)((uintptr_t)ks_proc_params->CommandLine.Buffer - m.tree.key),
                   cmdline.data(), ks_proc_params->CommandLine.Length)) {
        print("mapping::copy_to failed\n");
        m.release();
        free(ks_proc_params);
        return false;
    }

    if (!m.copy_to((uint32_t)((uintptr_t)proc.proc_params - m.tree.key),
                   ks_proc_params, sizeof(RTL_USER_PROCESS_PARAMETERS))) {
        print("mapping::copy_to failed\n");
        m.release();
        free(ks_proc_params);
        return false;
    }

    free(ks_proc_params);

    m.release();

    return true;
}

static bool allocate_peb(process& proc, HANDLE stdinh, HANDLE stdouth,
                         HANDLE stderrh, HANDLE curdirh, string_view curdir,
                         string_view fn, u16string_view cmdline) {
    static_assert(sizeof(PEB) <= PAGE_SIZE);

    size_t peb_size = sizeof(PEB);

    if (!allocate_proc_params(proc, stdinh, stdouth, stderrh, curdirh, curdir,
                              fn, cmdline)) {
        print("allocate_proc_params failed\n");
        return false;
    }

    if (!success(proc.allocate_vm((void**)&proc.peb, 0, &peb_size, MEM_COMMIT | MEM_RESERVE, mapping_type::read_write))) {
        print("process::allocate_vm failed\n");
        return false;
    }

    auto pm = find_mapping(proc, (uintptr_t)proc.peb);
    if (!pm) {
        print("allocate_peb: find_mapping failed\n");
        return false;
    }

    auto& m = *pm;

    auto pa = allocate_folio(0);
    if (!pa) {
        m.release();
        return false;
    }

    memset(phys_address(pa), 0, PAGE_SIZE); // FIXME - get zeroed page

    auto& ks_peb = *(PEB*)phys_address(pa);

    ks_peb.ImageBaseAddress = (void*)proc.img_addr;
    ks_peb.ProcessParameters = proc.proc_params;

    if (!m.copy_to((uint32_t)((uintptr_t)proc.peb - m.tree.key), &ks_peb,
                   sizeof(PEB))) {
        print("mapping::copy_to failed\n");
        m.release();
        free_folio(pa);
        return false;
    }

    m.release();

    free_folio(pa);

    return true;
}

object_ref create_process(string_view fn, string_view curdir,
                          u16string_view cmdline, object_ref& stdinobj,
                          object_ref& stdoutobj, object_ref& stderrobj) {
    nt_status status;
    object_ref img_ref, curdir_ref;
    unsigned int create_result;

    // FIXME - merge with NtCreateProcess?

    {
        object_ref img_inode_ref;
        optional<reference_wrapper<dentry>> drw;

        status = open_inode(fn, *root_dentry, img_inode_ref, drw, FILE_OPEN,
                            FILE_NON_DIRECTORY_FILE, create_result, false);
        if (!success(status)) { // FIXME - return status
            print("open_inode failed for {}: {}\n", fn, status);
            return {};
        }

        if (img_inode_ref.get().type != object_type::inode) {
            print("open_inode returned something other than inode\n");

            if (drw)
                drw->get().release();

            return {};
        }

        auto& img_inode = static_cast<inode&>(img_inode_ref.get());

        auto img_file = new file(img_inode, 0, 0);
        if (!img_file) {
            print("new failed\n");

            if (drw)
                drw->get().release();

            return {};
        }

        if (drw)
            img_file->dentry = &drw->get();

        img_ref = object_ref{*img_file};
    }

    {
        object_ref inode_ref;
        optional<reference_wrapper<dentry>> drw;

        status = open_inode(curdir, *root_dentry, inode_ref, drw, FILE_OPEN,
                            FILE_NON_DIRECTORY_FILE, create_result, false);
        if (!success(status)) { // FIXME - return status
            print("open_inode failed for {}: {}\n", curdir, status);
            return {};
        }

        auto& curdir_obj = inode_ref.get();
        if (curdir_obj.type != object_type::inode) {
            print("open_inode returned something other than inode\n");

            if (drw)
                drw->get().release();

            return {};
        }

        auto& curdir_inode = static_cast<inode&>(curdir_obj);

        auto curdir_file = new file(curdir_inode, 0, 0);
        if (!curdir_file) {
            print("new failed\n");

            if (drw)
                drw->get().release();

            return {};
        }

        if (drw)
            curdir_file->dentry = &drw->get();

        curdir_ref = object_ref{*curdir_file};
    }

    auto pid = next_pid.fetch_add(1);

    auto sess = new session;
    if (!sess) {
        print("new failed\n");
        return {};
    }

    sess->pid = pid;

    auto pg = new process_group(*sess);
    if (!pg) {
        print("new failed\n");
        sess->release();
        return {};
    }

    sess->release();

    pg->tree.key = pid;

    {
        lock_guard lock(pg_tree_lock);
        insert_into_rbtree(pg_tree, pg->tree);
    }

    pg->inserted = true;

    auto pptr = new process(*pg);
    if (!pptr) {
        print("new failed\n");
        pg->release();
        return {};
    }

    pg->release();

    if (!pptr->initialized) {
        print("process::process failed\n");
        delete pptr;
        return {};
    }

    auto& p = *pptr;

    p.tree.key = pid;

    // create section for image

    object_ref imgh;

    status = create_section(static_cast<file&>(img_ref.get()), true, 0,
                            PAGE_READWRITE, imgh);
    if (!success(status)) {
        print("create_section returned {}\n", status);
        p.release();
        return {};
    }

    p.img = static_cast<section&>(imgh.get());

#ifdef __aarch64__
    static const uint16_t this_arch = IMAGE_FILE_MACHINE_ARM64;
#elifdef __x86_64__
    static const uint16_t this_arch = IMAGE_FILE_MACHINE_AMD64;
#endif

    if (p.img->get().machine != this_arch) {
        print("invalid architecture {:x}\n", p.img->get().machine);
        p.release();
        return {};
    }

#ifdef __aarch64__
    if (!p.allocate_ttbr0()) {
        print("process::allocate_ttbr0 failed\n");
        p.release();
        return {};
    }
#elifdef __x86_64__
    if (!p.allocate_cr3()) {
        print("process::allocate_cr3 failed\n");
        p.release();
        return {};
    }
#endif

    p.img->get().acquire();

    imgh.reset();

    auto& img = p.img->get();

    p.img_addr = p.map_section(img, img.preferred_base + img.size > MAX_USERSPACE_ADDRESS ? 0 : img.preferred_base,
                               nullopt, mapping_type::read_only, true, nullptr);
    if (!p.img_addr) {
        print("process::map_section failed\n");
        p.release();
        return {};
    }

    // create stack

    size_t stack_size = max(img.stack_size, (uint64_t)PAGE_SIZE);
    void* stackaddr = nullptr;

    if (auto status = p.allocate_vm(&stackaddr, 0, &stack_size, MEM_COMMIT | MEM_RESERVE, mapping_type::read_write); !success(status)) {
        print("process::allocate_vm returned {}\n", status);
        p.release();
        return {};
    }

    // FIXME - if ntdll would have collision, choose another address

    p.ntdll_addr = p.map_section(*ntdll, ntdll->preferred_base,
                                 nullopt,  mapping_type::read_only, true,
                                 nullptr);
    if (!p.ntdll_addr) {
        print("process::map_section failed for ntdll\n");
        p.release();
        return {};
    }

    if (!p.map_section(*time_page, TIME_PAGE_BASE, nullopt,
                       mapping_type::read_only, false, nullptr)) {
        print("process::map_section failed for time page\n");
        p.release();
        return {};
    }

    handle_t stdinh = 0, stdouth = 0, stderrh = 0;

    if (stdinobj) {
        status = p.add_handle(stdinobj.get(), FILE_READ_DATA, true, stdinh);
        if (!success(status)) {
            print("add_handle failed\n");
            p.release();
            return {}; // FIXME - return status
        }
    }

    if (stdoutobj) {
        status = p.add_handle(stdoutobj.get(), FILE_WRITE_DATA, true, stdouth);
        if (!success(status)) {
            print("add_handle failed\n");
            p.release();
            return {}; // FIXME - return status
        }
    }

    if (stderrobj) {
        status = p.add_handle(stderrobj.get(), FILE_WRITE_DATA, true, stderrh);
        if (!success(status)) {
            print("add_handle failed\n");
            p.release();
            return {}; // FIXME - return status
        }
    }

    handle_t curdirh;

    status = p.add_handle(curdir_ref.get(), 0, true, curdirh); // FIXME - access mask
    if (!success(status)) {
        print("add_handle failed\n");
        p.release();
        return {}; // FIXME - return status
    }

    if (static_cast<file&>(curdir_ref.get()).dentry)
        static_cast<file&>(curdir_ref.get()).dentry->open_file_count++;

    if (!allocate_peb(p, stdinh, stdouth, stderrh, curdirh, curdir, fn, cmdline)) {
        print("allocate_peb failed\n");
        p.release();
        return {};
    }

    // FIXME - token for process

    // FIXME - give process a PID
    // FIXME - add process to list

    CONTEXT entry_point_context;
    CLIENT_ID client_id;
    INITIAL_TEB initial_teb;

    initial_teb.OldStackBase = nullptr;
    initial_teb.OldStackLimit = nullptr;
    initial_teb.StackBase = (uint8_t*)stackaddr + stack_size;
    initial_teb.StackLimit = (uint8_t*)stackaddr + stack_size;
    initial_teb.StackAllocationBase = stackaddr;

    uintptr_t entry_point_ctx_addr = (uintptr_t)stackaddr + stack_size - sizeof(CONTEXT);

#ifdef __aarch64__
    entry_point_ctx_addr -= 0x10;
#elifdef __x86_64__
    entry_point_ctx_addr -= 0x28; // 0x20 for MS ABI, 0x8 for return pointer
#endif

    memset(&entry_point_context, 0, sizeof(entry_point_context));

#ifdef __aarch64__
    entry_point_context.Pc = p.img_addr + img.entry_point;
    entry_point_context.Sp = entry_point_ctx_addr + sizeof(CONTEXT);
#elifdef __x86_64__
    entry_point_context.Rip = p.img_addr + img.entry_point;
    entry_point_context.Rsp = entry_point_ctx_addr + sizeof(CONTEXT);
#endif

    if (!p.copy_to((void*)entry_point_ctx_addr, &entry_point_context, sizeof(entry_point_context))) {
        print("process::copy_to failed\n");
        p.release();
        return {};
    }

    status = p.create_thread(nullopt, 0, nullptr, client_id, entry_point_context, initial_teb, true);
    if (!success(status)) {
        print("process::create_thread returned {}\n", status);
        p.release();
        return {};
    }

    p.suspend_count = 1;

    {
        lock_guard lock(process_list_lock);
        insert_into_rbtree(process_tree, p.tree);
    }

    p.added_to_tree = true;

    return p;
}

process::process(process_group& pg) : sync_object(object_type::process, false) {
    this->pg = &pg;
    pg.acquire();

    {
        lock_guard lock(pg.procs_lock);
        insert_tail_list(pg.procs, this->pg_le);
    }

    unreaped_children_event = create_event(false);
    if (!unreaped_children_event) {
        print("create_event failed\n");
        return;
    }

    initialized = true;
}

#ifdef __aarch64__
static void free_page_tables(process& proc) {
    if (proc.ttbr0 == 0)
        return;

    // FIXME - handle huge and gigantic pages

    auto ps1 = span((ARM64_HARDWARE_PTE*)phys_address(proc.ttbr0), 512);

    for (auto& p1 : ps1) {
        if (!p1.valid)
            continue;

        auto ps2 = span((ARM64_HARDWARE_PTE*)phys_address(p1.pfn << PAGE_SHIFT), 512);

        for (auto& p2 : ps2) {
            if (!p2.valid)
                continue;

            auto ps3 = span((ARM64_HARDWARE_PTE*)phys_address(p2.pfn << PAGE_SHIFT), 512);

            for (auto& p3 : ps3) {
                if (!p3.valid)
                    continue;

                free_folio(p3.pfn << PAGE_SHIFT);
            }

            free_folio(p2.pfn << PAGE_SHIFT);
        }

        free_folio(p1.pfn << PAGE_SHIFT);
    }

    free_folio(proc.ttbr0);
}
#elifdef __x86_64__
static void free_page_tables(process& proc) {
    if (proc.cr3 == 0)
        return;

    // FIXME - handle huge and gigantic pages

    auto ps1 = span((HARDWARE_PTE*)phys_address(proc.cr3), 256); // not higher half

    for (auto& p1 : ps1) {
        if (!p1.p)
            continue;

        auto ps2 = span((HARDWARE_PTE*)phys_address(p1.pfn << PAGE_SHIFT), 512);

        for (auto& p2 : ps2) {
            if (!p2.p)
                continue;

            auto ps3 = span((HARDWARE_PTE*)phys_address(p2.pfn << PAGE_SHIFT), 512);

            for (auto& p3 : ps3) {
                if (!p3.p)
                    continue;

                free_folio(p3.pfn << PAGE_SHIFT);
            }

            free_folio(p2.pfn << PAGE_SHIFT);
        }

        free_folio(p1.pfn << PAGE_SHIFT);
    }

    free_folio(proc.cr3);
}
#endif

process::~process() {
    if (added_to_tree) {
        lock_guard lock(process_list_lock);
        remove_from_rbtree(process_tree, tree);
    }

    list_entry children2 = { &children2, &children2 };

    {
        lock_guard lock(children_lock);

        while (!is_list_empty(children)) {
            auto le = children.next;

            remove_entry_list(*le);
            insert_tail_list(children2, *le);
        }
    }

    while (!is_list_empty(children2)) {
        auto& c = *(process*)((uint8_t*)children2.next - offsetof(process, child_le));

        remove_entry_list(c.child_le);

        {
            lock_guard lock(process_list_lock);
            c.parent_process = nullptr;
        }

        c.release();
    }

    {
        lock_guard lock(pg->procs_lock);
        remove_entry_list(this->pg_le);
    }

    pg->release();
}

#ifdef __aarch64__
static bool set_page_protection(process& proc, uint64_t va, uint32_t num_pages,
                                enum mapping_type protect, bool cow,
                                bool reserve) {
    span<ARM64_HARDWARE_PTE> pt[4];
    uint16_t index[4];

    index[0] = (va & 0x1ff000) >> 12;
    index[1] = (va & 0x3fe00000) >> 21;
    index[2] = (va & 0x7fc0000000) >> 30;
    index[3] = (va & 0xff8000000000) >> 39;

    pt[3] = span((ARM64_HARDWARE_PTE*)phys_address(proc.ttbr0), 512);

    auto redo_level = [&](uint8_t level) -> bool {
        if (!pt[level][index[level]].valid) {
            uint64_t pa = allocate_folio(0, page_state::pte);
            if (pa == 0)
                return false;

            pt[level][index[level]].valid = 1;
            pt[level][index[level]].table_entry = 1;
            pt[level][index[level]].AP = ARM64_PTE_AP_EL0RW;
            pt[level][index[level]].SH = 3; // inner shareable
            pt[level][index[level]].AF = 1;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
            pt[level][index[level]].pfn = pa >> PAGE_SHIFT;
#pragma GCC diagnostic pop

            memset(phys_address(pa), 0, PAGE_SIZE);
        }

        pt[level - 1] = span((ARM64_HARDWARE_PTE*)phys_address((uint64_t)((pt[level][index[level]].pfn) << PAGE_SHIFT)), 512);

        return true;
    };

    for (uint8_t level = 3; level > 0; level--) {
        if (!redo_level(level))
            return false;
    }

    for (unsigned int i = 0; i < num_pages; i++) {
        if (i != 0) {
            index[0]++;

            if (index[0] == 512) {
                index[0] = 0;
                index[1]++;

                if (index[1] == 512) {
                    index[1] = 0;
                    index[2]++;

                    if (index[2] == 512) {
                        index[2] = 0;
                        index[3]++;

                        redo_level(3);
                    }

                    redo_level(2);
                }

                redo_level(1);
            }
        }

        auto& pte = pt[0][index[0]];

        if (pte.valid && pte.owned && pte.cow) {
            // don't clear COW flag if already COWed

            // FIXME - should be able to make COWed page readonly?

            switch (protect) {
                case mapping_type::read_execute:
                case mapping_type::read_write_execute:
                    pte.UXN = 0;
                    break;

                default:
                    pte.UXN = 1;
                    break;
            }
        } else {
            pte.reserved = reserve ? 1 : 0;
            pte.AttrIndex = attr_index_normal;
            pte.SH = 3; // inner shareable

            switch (protect) {
                case mapping_type::read_write:
                    pte.UXN = 1;
                    pte.allow_write = 1;
                    pte.cow = cow ? 1 : 0;
                    break;

                case mapping_type::read_execute:
                    pte.UXN = 0;
                    pte.allow_write = 0;
                    pte.cow = 0;
                    break;

                case mapping_type::read_write_execute:
                    pte.UXN = 0;
                    pte.allow_write = 1;
                    pte.cow = cow ? 1 : 0;
                    break;

                default:
                    pte.UXN = 1;
                    pte.allow_write = 0;
                    pte.cow = 0;
                    break;
            }
        }
    }

    __asm __volatile (
        "tlbi vae1, %x[res]\n"
        "dsb ish\n"
        "isb\n"
        :
        : [res] "r" (va >> PAGE_SHIFT)
    );

    return true;
}
#elifdef __x86_64__
static bool set_page_protection(process& proc, uint64_t va, uint32_t num_pages,
                                enum mapping_type protect, bool cow,
                                bool reserve) {
    span<HARDWARE_PTE> pt[4];
    uint16_t index[4];

    index[0] = (va & 0x1ff000) >> 12;
    index[1] = (va & 0x3fe00000) >> 21;
    index[2] = (va & 0x7fc0000000) >> 30;
    index[3] = (va & 0xff8000000000) >> 39;

    pt[3] = span((HARDWARE_PTE*)phys_address(proc.cr3), 512);

    auto redo_level = [&](uint8_t level) -> bool {
        if (!pt[level][index[level]].p) {
            uint64_t pa = allocate_folio(0, page_state::pte);
            if (pa == 0)
                return false;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
            pt[level][index[level]].pfn = pa >> PAGE_SHIFT;
#pragma GCC diagnostic pop
            pt[level][index[level]].p = 1;
            pt[level][index[level]].w = 1;
            pt[level][index[level]].u = 1;

            memset(phys_address(pa), 0, PAGE_SIZE);
        }

        pt[level - 1] = span((HARDWARE_PTE*)phys_address((uint64_t)((pt[level][index[level]].pfn) << PAGE_SHIFT)), 512);

        return true;
    };

    for (uint8_t level = 3; level > 0; level--) {
        if (!redo_level(level))
            return false;
    }

    for (unsigned int i = 0; i < num_pages; i++) {
        if (i != 0) {
            index[0]++;

            if (index[0] == 512) {
                index[0] = 0;
                index[1]++;

                if (index[1] == 512) {
                    index[1] = 0;
                    index[2]++;

                    if (index[2] == 512) {
                        index[2] = 0;
                        index[3]++;

                        redo_level(3);
                    }

                    redo_level(2);
                }

                redo_level(1);
            }
        }

        auto& pte = pt[0][index[0]];

        if (pte.p && pte.owned && pte.cow) {
            // don't clear COW flag if already COWed

            // FIXME - should be able to make COWed page readonly?

            switch (protect) {
                case mapping_type::read_execute:
                case mapping_type::read_write_execute:
                    pte.xd = 0;
                    break;

                default:
                    pte.xd = 1;
                    break;
            }
        } else {
            pte.u = 1;
            pte.w = 0;
            pte.reserved = reserve ? 1 : 0;

            switch (protect) {
                case mapping_type::read_write:
                    pte.xd = 1;
                    pte.allow_write = 1;
                    pte.cow = cow ? 1 : 0;
                    break;

                case mapping_type::read_execute:
                    pte.xd = 0;
                    pte.allow_write = 0;
                    pte.cow = 0;
                    break;

                case mapping_type::read_write_execute:
                    pte.xd = 0;
                    pte.allow_write = 1;
                    pte.cow = cow ? 1 : 0;
                    break;

                default:
                    pte.xd = 1;
                    pte.allow_write = 0;
                    pte.cow = 0;
                    break;
            }
        }
    }

    return true;
}
#endif

uintptr_t process::map_section(section& sect, uintptr_t address,
                               optional<int64_t> section_offset,
                               enum mapping_type protect, bool cow,
                               size_t* view_size) {
    // FIXME - offset and mapping size

    if (address & 0x8000000000000000 || ((address + sect.size) & 0x8000000000000000)) { // don't allow in kernel space
        print("invalid address {:x}\n", address);
        return 0;
    }

    if (address & (PAGE_SIZE - 1)) { // must be on page boundary
        print("invalid address {:x}\n", address);
        return 0;
    }

    if (address & 0xffff000000000000 || (address + sect.size) & 0xffff000000000000) { // VAs limited to 48 bits
        print("invalid address {:x}\n", address);
        return 0;
    }

    if (section_offset.has_value()) {
        if ((uint64_t)*section_offset % PAGE_SIZE) {
            print("process::map_section: invalid offset {:x}\n", *section_offset);
            return 0;
        }

        if (*section_offset >= sect.size) {
            print("process::map_section: offset {:x} >= section size {:x}\n",
                  *section_offset, sect.size);
            return 0;
        }
    }

    auto m = new mapping(*this, sect);
    if (!m) {
        print("new failed\n");
        return 0;
    }

    m->tree.key = address;
    m->offset = section_offset.has_value() ? (uint64_t)*section_offset : 0;
    m->size = bytes_to_pages(sect.size - m->offset) << PAGE_SHIFT;

    {
        lock_guard lock(mappings_lock);

        if (address == 0) {
            uintptr_t last_addr = 0x100000; // don't place images below 1 MB unless explicitly requested

            auto n1 = get_tree_minimum(mappings);

            while (n1) {
                auto& m1 = rbtree_containing_record<mapping>(n1);

                if (m1.tree.key >= last_addr + m->size) {
                    address = last_addr;
                    break;
                }

                n1 = next_rbtree_node(*n1);
                last_addr = max(last_addr, m1.tree.key + m1.size);
            }

            if (address == 0) {
                auto n1 = get_tree_maximum(mappings);

                if (!n1)
                    address = 0x100000;
                else {
                    auto& m1 = rbtree_containing_record<mapping>(n1);

                    if (m1.tree.key + m1.size + m->size >= MAX_USERSPACE_ADDRESS) {
                        delete m;
                        return 0;
                    }

                    address = m1.tree.key + m1.size;
                }
            }
        } else {
            auto n1 = find_rbtree_node_or_less(mappings, address);

            if (!n1)
                n1 = get_tree_minimum(mappings);

            if (n1) {
                auto& m1 = rbtree_containing_record<mapping>(n1);

                if (address < m1.tree.key + m1.size && address + m->size > m1.tree.key) { // collision
                    delete m;
                    return 0;
                }

                auto n2 = next_rbtree_node(m1.tree);

                if (n2) {
                    auto& m2 = rbtree_containing_record<mapping>(n2);

                    if (address < m2.tree.key + m2.size && address + m->size > m2.tree.key) { // collision
                        delete m;
                        return 0;
                    }
                }
            }
        }

        m->tree.key = address;
        m->in_address_space = true;
        insert_into_rbtree(mappings, m->tree);

        if (view_size)
            *view_size = m->size;
    }

    if (sect.pe_sections) {
        uint64_t addr = 0;

        for (unsigned int i = 0; i < sect.num_pe_sections; i++) {
            auto& s = sect.pe_sections[i];

            if (s.VirtualAddress > addr) {
                if (!set_page_protection(*this, address + addr, (uint32_t)((s.VirtualAddress - addr) >> PAGE_SHIFT),
                                         mapping_type::read_only, false, false)) {
                    return 0; // FIXME - return nt_status::INSUFFICIENT_RESOURCES
                }
            }

            uint64_t num_pages = bytes_to_pages(s.VirtualSize);

            if (!set_page_protection(*this, address + s.VirtualAddress, (uint32_t)num_pages,
                                     characteristics_to_mapping_type(s.Characteristics),
                                     true, false)) {
                return 0; // FIXME - return nt_status::INSUFFICIENT_RESOURCES
            }

            addr = s.VirtualAddress + (num_pages << PAGE_SHIFT);
        }

        if (addr < sect.size) {
            if (!set_page_protection(*this, address + addr, (uint32_t)((sect.size - addr) >> PAGE_SHIFT),
                                     mapping_type::read_only, false, false)) {
                return 0; // FIXME - return nt_status::INSUFFICIENT_RESOURCES
            }
        }
    } else {
        if (!set_page_protection(*this, address, sect.num_pages, protect, cow, false))
            return 0; // FIXME - return nt_status::INSUFFICIENT_RESOURCES
    }

    return address;
}

static bool populate_teb(process& proc, ::thread& t) {
    // only care about first page
    static_assert(offsetof(TEB, NtTib.Self) >> PAGE_SHIFT == 0);
    static_assert(offsetof(TEB, ProcessEnvironmentBlock) >> PAGE_SHIFT == 0);

    auto pm = find_mapping(proc, (uintptr_t)t.teb);
    if (!pm) {
        print("populate_teb: find_mapping failed\n");
        return false;
    }

    auto& m = *pm;

    auto pa = allocate_folio(0);
    if (!pa) {
        m.release();
        return false;
    }

    memset(phys_address(pa), 0, PAGE_SIZE); // FIXME - get zeroed page

    auto& teb = *(TEB*)phys_address(pa);

    teb.NtTib.Self = &t.teb->NtTib;
    // FIXME - StackBase
    // FIXME - StackLimit
    teb.ClientId.UniqueProcess = proc.tree.key;
    teb.ClientId.UniqueThread = t.tid;
    teb.ProcessEnvironmentBlock = proc.peb;

    if (!m.copy_to((uint32_t)((uintptr_t)t.teb - m.tree.key), &teb,
                   PAGE_SIZE)) {
        print("mapping::copy_to failed\n");
        m.release();
        free_folio(pa);
        return false;
    }

    m.release();

    free_folio(pa);

    return true;
}

#ifdef __aarch64__
ARM64_HARDWARE_PTE* find_pte(process& proc, uintptr_t va) {
    span<ARM64_HARDWARE_PTE> pt[4];

    pt[3] = span((ARM64_HARDWARE_PTE*)phys_address(proc.ttbr0), 512);

    if (!pt[3][(va & 0xff8000000000) >> 39].valid)
        return nullptr;

    pt[2] = span((ARM64_HARDWARE_PTE*)phys_address((uint64_t)((pt[3][(va & 0xff8000000000) >> 39].pfn) << PAGE_SHIFT)), 512);

    if (!pt[2][(va & 0x7fc0000000) >> 30].valid)
        return nullptr;

    pt[1] = span((ARM64_HARDWARE_PTE*)phys_address((uint64_t)((pt[2][(va & 0x7fc0000000) >> 30].pfn) << PAGE_SHIFT)), 512);

    if (!pt[1][(va & 0x3fe00000) >> 21].valid)
        return nullptr;

    pt[0] = span((ARM64_HARDWARE_PTE*)phys_address((uint64_t)((pt[1][(va & 0x3fe00000) >> 21].pfn) << PAGE_SHIFT)), 512);

    return &pt[0][(va & 0x1ff000) >> 12];
}
#elifdef __x86_64__
HARDWARE_PTE* find_pte(process& proc, uintptr_t va) {
    span<HARDWARE_PTE> pt[4];

    pt[3] = span((HARDWARE_PTE*)phys_address(proc.cr3), 512);

    if (!pt[3][(va & 0xff8000000000) >> 39].p)
        return nullptr;

    pt[2] = span((HARDWARE_PTE*)phys_address((uint64_t)((pt[3][(va & 0xff8000000000) >> 39].pfn) << PAGE_SHIFT)), 512);

    if (!pt[2][(va & 0x7fc0000000) >> 30].p)
        return nullptr;

    pt[1] = span((HARDWARE_PTE*)phys_address((uint64_t)((pt[2][(va & 0x7fc0000000) >> 30].pfn) << PAGE_SHIFT)), 512);

    if (!pt[1][(va & 0x3fe00000) >> 21].p)
        return nullptr;

    pt[0] = span((HARDWARE_PTE*)phys_address((uint64_t)((pt[1][(va & 0x3fe00000) >> 21].pfn) << PAGE_SHIFT)), 512);

    return &pt[0][(va & 0x1ff000) >> 12];
}
#endif

#ifdef __x86_64__
static void send_tlb_ipi(uint8_t cpu_no) {
    tlb_cpus_left++;

    auto icr2 = (volatile uint32_t*)&local_apic[0x310];

    *icr2 = (*icr2 & 0x00ffffff) | ((uint32_t)cpu_no << 24);

    auto icr1 = (volatile uint32_t*)&local_apic[0x300];

    *icr1 = invalidate_tlb_interrupt;

    while (*icr1 & (1 << 12)) { } // wait to clear
}

static void send_all_tlb_ipi(uint8_t cpu_no) {
    all_tlb_cpus_left++;

    auto icr2 = (volatile uint32_t*)&local_apic[0x310];

    *icr2 = (*icr2 & 0x00ffffff) | ((uint32_t)cpu_no << 24);

    auto icr1 = (volatile uint32_t*)&local_apic[0x300];

    *icr1 = invalidate_all_tlb_interrupt;

    while (*icr1 & (1 << 12)) { } // wait to clear
}
#elifdef __aarch64__
static void send_tlb_ipi(uint8_t cpu_no) {
    auto& cb = *get_cpu_block();

    // FIXME - sending to multiple CPUs at once?

    tlb_cpus_left++;

    if (cb.gicc) { // GICv2
        auto& GICD_SGIR = *(volatile uint32_t*)(gicd + 0xf00);
        ARM64_GICD_SGIR sgir;

        sgir.value = 0;
        sgir.TargetListFilter = 0; // send IPI to specified CPU
        sgir.CPUTargetList = 1 << cpu_ids[cpu_no];
        sgir.SGIINTID = TLB_SGI;

        GICD_SGIR = sgir.value;
    } else {
        ARM64_ICC_SGI1R sgir;
        ARM64_MPIDR mpidr;

        mpidr.value = cpu_ids[cpu_no];

        sgir.value = 0;
        sgir.IRM = 0; // only specified CPU
        sgir.INTID = TLB_SGI;
        sgir.Aff3 = mpidr.Aff3;
        sgir.Aff2 = mpidr.Aff2;
        sgir.Aff1 = mpidr.Aff1;
        sgir.TargetList = 1 << mpidr.Aff0;

        __asm __volatile (
            "msr icc_sgi1r_el1, %x[res]"
            :
            : [res] "r" (sgir.value)
        );
    }
}

static void send_all_tlb_ipi(uint8_t cpu_no) {
    auto& cb = *get_cpu_block();

    // FIXME - sending to multiple CPUs at once?

    all_tlb_cpus_left++;

    if (cb.gicc) { // GICv2
        auto& GICD_SGIR = *(volatile uint32_t*)(gicd + 0xf00);
        ARM64_GICD_SGIR sgir;

        sgir.value = 0;
        sgir.TargetListFilter = 0; // send IPI to specified CPU
        sgir.CPUTargetList = 1 << cpu_ids[cpu_no];
        sgir.SGIINTID = ALL_TLB_SGI;

        GICD_SGIR = sgir.value;
    } else {
        ARM64_ICC_SGI1R sgir;
        ARM64_MPIDR mpidr;

        mpidr.value = cpu_ids[cpu_no];

        sgir.value = 0;
        sgir.IRM = 0; // only specified CPU
        sgir.INTID = ALL_TLB_SGI;
        sgir.Aff3 = mpidr.Aff3;
        sgir.Aff2 = mpidr.Aff2;
        sgir.Aff1 = mpidr.Aff1;
        sgir.TargetList = 1 << mpidr.Aff0;

        __asm __volatile (
            "msr icc_sgi1r_el1, %x[res]"
            :
            : [res] "r" (sgir.value)
        );
    }
}
#endif

void send_page_tlb_ipi(process& proc, uint64_t va, uint64_t num_pages) {
    disable_preempt();

    auto cpus = proc.running_cpus.load();

    if ((cpus & ~((uint64_t)1 << get_cpu_block()->cpu_no)) == 0) {
        enable_preempt();
        return;
    }

    enable_preempt();

    {
        lock_guard lock(ipi_lock);

        disable_preempt();

        auto& cb = *get_cpu_block();

        tlb_va = va;
        tlb_num_pages = num_pages;

        tlb_cpus_left = 1;

        for (uint8_t i = 0; i < num_cpus; i++) {
            if (i == cb.cpu_no)
                continue;

            if (!(cpus & 1 << i))
                continue;

            send_tlb_ipi(i);
        }

        tlb_cpus_left--;

        while (tlb_cpus_left.load(memory_order_acquire) > 0) {
#ifdef __aarch64__
            __asm("yield");
#endif
        }

        enable_preempt();
    }
}

static void send_proc_all_tlb_ipi(process& proc) {
    disable_preempt();

    auto cpus = proc.running_cpus.load();

    if ((cpus & ~((uint64_t)1 << get_cpu_block()->cpu_no)) == 0) {
        enable_preempt();
        return;
    }

    enable_preempt();

    {
        lock_guard lock(ipi_lock);

        disable_preempt();

        auto& cb = *get_cpu_block();

        all_tlb_cpus_left = 1;

        for (uint8_t i = 0; i < num_cpus; i++) {
            if (i == cb.cpu_no)
                continue;

            if (!(cpus & 1 << i))
                continue;

            send_all_tlb_ipi(i);
        }

        all_tlb_cpus_left--;

        while (all_tlb_cpus_left.load(memory_order_acquire) > 0) {
#ifdef __aarch64__
            __asm("yield");
#endif
        }

        enable_preempt();
    }
}

static bool handle_page_fault2(process& p, uintptr_t address) {
    auto pte = find_pte(p, address);

    if (!pte)
        return false;

#ifdef __aarch64__
    // don't handle if kernel-only page
    if (pte->AP == ARM64_PTE_AP_EL1RW || pte->AP == ARM64_PTE_AP_EL1RO)
        return false;
#endif

    if (!pte->allow_write)
        return false;

#ifdef __aarch64__
    if (pte->AP != ARM64_PTE_AP_EL0RO) {
        __asm __volatile (
            "tlbi vae1, %x[res]\n"
            "dsb ish\n"
            "isb\n"
            :
            : [res] "r" (address >> PAGE_SHIFT)
        );

        return true;
    }
#elifdef __x86_64__
    if (pte->w) {
        __asm __volatile (
            "invlpg [%0]\n"
            :
            : "r" (address)
        );

        return true;
    }
#endif

    // mark cache as dirty

    auto m = find_mapping(p, address);

    if (!m)
        return false;

    bool send_ipi = false;

    if (!make_pte_writable(address, *m, *pte, send_ipi)) {
        m->release();
        return false;
    }

    m->release();

#ifdef __aarch64__
    __asm __volatile (
        "tlbi vae1, %x[res]\n"
        "dsb ish\n"
        "isb\n"
        :
        : [res] "r" (address >> PAGE_SHIFT)
    );
#elifdef __x86_64__
    __asm __volatile (
        "invlpg [%0]\n"
        :
        : "r" (address)
    );
#endif

    if (send_ipi)
        send_page_tlb_ipi(p, address, 1);

    return true;
}

#ifdef __aarch64__
bool process::handle_page_fault(uintptr_t address, uint64_t esr_value) {
    ARM64_ESR esr;

    esr.value = esr_value;

    address &= ~(PAGE_SIZE - 1);

    if (address >= 0x800000000000) // not userspace
        return false;

    bool ret;

    {
        rangelock_req rlr(rl, address >> PAGE_SHIFT, 1, rangelock_mode::exclusive);

        if ((esr.ISS & 0x3c) == 0x0c) { // permission fault - page is present
            // FIXME - NX violations

            return handle_page_fault2(*this, address);
        }

        auto pte = find_pte(*this, address);

        if (pte && pte->reserved)
            return false;

        auto m = find_mapping(*this, address);
        if (!m)
            return false;

        ret = m->map((uint32_t)(address - m->tree.key), address);

        m->release();
    }

    return ret;
}
#elifdef __x86_64__
bool process::handle_page_fault(uintptr_t address, uint64_t error_code) {
    page_fault_error_code ec;

    ec.value = error_code;

    address &= ~(PAGE_SIZE - 1);

    if (address >= 0x800000000000) // not userspace
        return false;

    bool ret;

    {
        rangelock_req rlr(rl, address >> PAGE_SHIFT, 1, rangelock_mode::exclusive);

        if (ec.present) {
            if (ec.instruction_fetch) // NX violation
                return false;

            return handle_page_fault2(*this, address);
        }

        auto pte = find_pte(*this, address);

        if (pte && pte->reserved)
            return false;

        auto m = find_mapping(*this, address);
        if (!m)
            return false;

        ret = m->map((uint32_t)(address - m->tree.key), address);

        m->release();
    }

    return ret;
}
#endif

#ifdef __aarch64__
bool process::allocate_ttbr0() {
    ttbr0 = allocate_folio(0, page_state::pte);
    if (!ttbr0)
        return false;

    memset(phys_address(ttbr0), 0, PAGE_SIZE);

    return true;
}
#elifdef __x86_64__
bool process::allocate_cr3() {
    auto kernel_cr3 = get_cpu_block()->kernel_cr3;

    cr3 = allocate_folio(0, page_state::pte);
    if (!cr3)
        return false;

    auto va = phys_address(cr3);

    // copy higher half mappings

    memset((void*)va, 0, PAGE_SIZE >> 1);
    memcpy((uint8_t*)va + (PAGE_SIZE >> 1),
           (uint8_t*)phys_address(kernel_cr3) + (PAGE_SIZE >> 1),
           PAGE_SIZE >> 1);

    return true;
}
#endif

bool process::copy_from(void* ks_addr, const void* us_addr, size_t size) {
    while (true) {
        auto m = find_mapping(*this, (uintptr_t)us_addr);
        if (!m)
            return false;

        auto to_copy = min(size, m->tree.key + m->size - (uintptr_t)us_addr);

        if (!m->copy_from((uint32_t)((uintptr_t)us_addr - m->tree.key), ks_addr, (uint32_t)to_copy)) {
            m->release();
            return false;
        }

        m->release();

        if (size == to_copy)
            return true;

        us_addr = (uint8_t*)us_addr + to_copy;
        ks_addr = (uint8_t*)ks_addr + to_copy;
        size -= to_copy;
    }

    return false;
}

bool process::copy_to(void* us_addr, const void* ks_addr, size_t size) {
    while (true) {
        auto m = find_mapping(*this, (uintptr_t)us_addr);
        if (!m)
            return false;

        auto to_copy = min(size, m->tree.key + m->size - (uintptr_t)us_addr);

        if (!m->copy_to((uint32_t)((uintptr_t)us_addr - m->tree.key), ks_addr, (uint32_t)to_copy)) {
            m->release();
            return false;
        }

        m->release();

        if (size == to_copy)
            return true;

        us_addr = (uint8_t*)us_addr + to_copy;
        ks_addr = (uint8_t*)ks_addr + to_copy;
        size -= to_copy;
    }

    return false;
}

void load_ntdll() {
    nt_status status;
    object_ref inode_ref;
    optional<reference_wrapper<dentry>> drw;
    unsigned int create_result;

    status = open_inode("/lib/ntdll.dll", *root_dentry, inode_ref, drw, FILE_OPEN,
                        FILE_NON_DIRECTORY_FILE, create_result, false);
    if (!success(status))
        bsod_format("open_inode returned {} for ntdll.dll", status);

    if (inode_ref.get().type != object_type::inode)
        bsod("open_inode returned something other than an inode");

    auto ntdll_file = new file(inode_ref.get(), 0, 0);

    if (!ntdll_file)
        bsod("load_ntdll: new failed");

    object_ref f(*ntdll_file);
    object_ref ntdllh;

    if (drw)
        ntdll_file->dentry = &drw->get();

    status = create_section(static_cast<file&>(f.get()), true, 0,
                            PAGE_READWRITE, ntdllh);
    if (!success(status))
        bsod_format("create_section returned {} for ntdll.dll", status);

    ntdll = &static_cast<section&>(ntdllh.get());
    ntdll->acquire();

    mn_start_thread = ntdll->find_export("mn_start_thread");
    if (!mn_start_thread)
        bsod("Could not find export mn_start_thread in ntdll.dll");

    mn_signal = ntdll->find_export("mn_signal");
    if (!mn_signal)
        bsod("Could not find export mn_signal in ntdll.dll");

    mn_handle_apc = ntdll->find_export("mn_handle_apc");
    if (!mn_handle_apc)
        bsod("Could not find export mn_handle_apc in ntdll.dll");

    mn_posix_signal = ntdll->find_export("mn_posix_signal");
    if (!mn_posix_signal)
        bsod("Could not find export mn_posix_signal in ntdll.dll");
}

nt_status time_page_cache::read_into_cache(uint64_t page_num, uint64_t num_pages,
                                           uint64_t& read_pages) {
    if (page_num > 0 || num_pages == 0) {
        read_pages = 0;
        return nt_status::SUCCESS;
    }

    num_pages = 1;

    auto status = cache.pin(page_num, num_pages, nullptr);
    if (!success(status)) {
        print("page_cache::pin returned {}\n", status);
        return status;
    }

    page_cache_entry* p;

    {
        lock_guard lg(cache.base.cache_lock);
        p = cache.find_page_cache_entry(page_num);
    }

    if (!p->pfn) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
        p->pfn = va_to_pa_lookup((uintptr_t)&kernel_time_page) >> PAGE_SHIFT;
#pragma GCC diagnostic pop
    }

    cache.unpin(page_num, num_pages);

    read_pages = num_pages;
    return nt_status::SUCCESS;
}

nt_status zero_cache::read_into_cache(uint64_t page_num, uint64_t num_pages,
                                      uint64_t& read_pages) {
    // FIXME - we really shouldn't be maintaining page_cache_entries for this

    auto status = cache.pin(page_num, num_pages, nullptr);
    if (!success(status)) {
        print("page_cache::pin returned {}\n", status);
        return status;
    }

    page_cache_entry* p;

    for (uint64_t i = 0; i < num_pages; i++) {
        {
            lock_guard lg(cache.base.cache_lock);

            if (i == 0)
                p = cache.find_page_cache_entry(page_num);
            else
                p = cache.next_page_cache_entry(*p);
        }

        if (p->pfn)
            continue;

        increase_page_refcount(zero_pfn);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
        p->pfn = zero_pfn;
#pragma GCC diagnostic pop
    }

    cache.unpin(page_num, num_pages);

    read_pages = num_pages;
    return nt_status::SUCCESS;
}

void create_time_page() {
    assert(version.size() < sizeof(kernel_time_page.os_version));

    memcpy(kernel_time_page.os_version, version.data(), version.size());
    kernel_time_page.os_version[version.size()] = 0;

    memcpy(kernel_time_page.os_hash, os_hash.data(), os_hash.size());
    kernel_time_page.os_hash[os_hash.size()] = 0;

#ifdef __x86_64__
    kernel_time_page.tsc_freq = tsc_freq;
#elifdef __aarch64__
    kernel_time_page.tsc_freq = cntfrq_el0_load();
#endif

    kernel_time_page.tick_rate = SYSTEM_TIME_RESOLUTION / TICK_RATE;

    time_page = new section;
    if (!time_page)
        bsod("create_time_page: new failed");

    time_page->type = section_type::anonymous;
    time_page->size = PAGE_SIZE;
    time_page->num_pages = 1;
    time_page->page_protection = PAGE_READONLY;

    time_page->custom_cache = new time_page_cache;
    if (!time_page->custom_cache)
        bsod("create_time_page: new failed");
}

void create_zero_section() {
    zero_section = new section;
    if (!zero_section)
        bsod("create_zero_section: new failed");

    zero_section->type = section_type::anonymous;
    zero_section->size = PAGE_SIZE; // FIXME
    zero_section->num_pages = 0xffffffff; // FIXME - should be 1 << (47 - PAGE_SHIFT)
    zero_section->page_protection = PAGE_READONLY;

    zero_section->custom_cache = new zero_cache;
    if (!zero_section->custom_cache)
        bsod("create_zero_section: new failed");
}

static void free_handle(rbtree_node<handle_t>* tree) {
    auto& ho = rbtree_containing_record<handle_obj>(tree);

    // FIXME - is recursion going to be a problem here?

    if (tree->left)
        free_handle(tree->left);

    if (tree->right)
        free_handle(tree->right);

    free(&ho);
}

void process::terminate() {
    bool removed_from_list = false;

    // wake up threads waiting for process to terminate

    {
        sync_lock sl;
        lock_guard lock(sl);

        signalled = true;

        wake_up_waiters(waiters, WAIT_TYPE_READING);

        {
            lock_guard lock2(process_list_lock);

            if (parent_process) {
                lock_guard lock3(parent_process->children_lock);

                if (marked_as_child) {
                    auto& e = static_cast<event&>(parent_process->unreaped_children_event.get());

                    e.set_holding_sync_lock();

                    wake_up_waiters(parent_process->wait_child_waiters, WAIT_TYPE_READING);
                } else {
                    remove_entry_list(child_le);
                    removed_from_list = true;
                }
            }

            if (do_process_shutdown_ratchet && !is_init && this != process_shutdown_ignore) {
                process_shutdown_ratchet--;

                if (process_shutdown_ratchet == 0) {
                    auto& e = static_cast<event&>(process_shutdown_event.get());

                    e.set_holding_sync_lock();
                }
            }
        }
    }

    // send SIGCHLD to parent

    process* sigchld_proc = nullptr;

    {
        lock_guard lock(process_list_lock);

        if (parent_process) {
            lock_guard lock2(parent_process->children_lock);

            if (marked_as_child) {
                sigchld_proc = parent_process;
                sigchld_proc->acquire();
            }
        }
    }

    if (sigchld_proc) {
        set_pending_signal2(*sigchld_proc, SIGCHLD);
        sigchld_proc->release();
    }

    // close open handles

    rbtree_node<handle_t>* handles2 = nullptr;

    {
        lock_guard lock(handle_lock);
        swap(handles, handles2);
    }

    if (handles2) {
        auto n = get_tree_minimum(handles2);

        while (n) {
            auto& ho = rbtree_containing_record<handle_obj>(n);

            handle_closed(*ho.obj, this);
            object_release(*ho.obj);

            n = next_rbtree_node(*n);
        }

        free_handle(handles2);
    }

    while (true) {
        mapping* m;

        {
            lock_guard lock(mappings_lock);

            auto n = get_tree_minimum(mappings);

            if (!n)
                break;

            m = &rbtree_containing_record<mapping>(n);

            remove_from_rbtree(mappings, m->tree);
        }

        m->release();
    }

    if (img)
        img->get().release();

    free_page_tables(*this);

    if (removed_from_list)
        release();
}

static nt_status terminate_other_threads(nt_status exit_status) {
    auto& this_thread = *get_current_thread();

    if (!this_thread.proc)
        return nt_status::INVALID_PARAMETER;

    auto& p = *this_thread.proc;

    {
        lock_guard lg(p.terminating_lock);

        if (this_thread.status == thread_status::dying || this_thread.status == thread_status::dead)
            return nt_status::SUCCESS;

        while (true) {
            ::thread* t;

            {
                lock_guard lock(p.threads_lock);

                if (is_list_empty(p.threads))
                    break;

                t = (::thread*)((uint8_t*)p.threads.next - offsetof(::thread, process_le));

                if (t == &this_thread) {
                    if (t->process_le.next == &p.threads)
                        break;

                    t = (::thread*)((uint8_t*)p.threads.next->next - offsetof(::thread, process_le));
                }

                t->exit_status = exit_status;
                terminate_thread(*t, false);
                remove_entry_list(t->process_le);
            }
        }
    }

    return nt_status::SUCCESS;
}

static void do_terminate(process& p, nt_status exit_status) {
    {
        lock_guard lock(p.threads_lock);

        if (p.dying)
            return;

        p.dying = true;
    }

    while (true) {
        ::thread* t;

        {
            lock_guard lock(p.threads_lock);

            if (is_list_empty(p.threads))
                break;

            t = (::thread*)((uint8_t*)p.threads.next - offsetof(::thread, process_le));

            t->exit_status = exit_status;
            terminate_thread(*t, false);
            remove_entry_list(t->process_le);
        }
    }

    p.exit_status = exit_status;
    p.terminate();
}

extern "C"
void trace_NtTerminateProcess(uintptr_t* args) {
    auto process_handle = (HANDLE)args[0];
    auto exit_status = (NTSTATUS)args[1];

    start_deferred_print();

    print("NtTerminateProcess: process_handle = ");
    print_handle(process_handle);
    print(", exit_status = {}\n", exit_status);

    end_deferred_print();
}

extern "C"
NTSTATUS NtTerminateProcess(HANDLE process_handle, NTSTATUS exit_status) {
    object_ref obj;

    TRACE2("NtTerminateProcess", process_handle, exit_status);

    // if process_handle == 0, this instead means kill all other threads
    if (process_handle == 0)
        return terminate_other_threads(exit_status);

    if (process_handle == NtCurrentProcess()) {
        auto t = get_current_thread();

        if (!t->proc)
            return nt_status::INVALID_PARAMETER;

        auto p = t->proc;
        p->acquire();

        obj = object_ref{*p};
    } else {
        obj = deref_handle(process_handle);
        if (!obj)
            return nt_status::INVALID_HANDLE;

        if (obj.get().type != object_type::process)
            return nt_status::OBJECT_TYPE_MISMATCH;

        // FIXME - check access rights
    }

    auto& p = static_cast<process&>(obj.get());

    do_terminate(p, exit_status);

    return nt_status::SUCCESS;
}

void process::resume() {
    if (--suspend_count != 0)
        return;

    sync_lock sl;

    lock_guard lock1(sl);
    lock_guard lock2(threads_lock);

    auto le = threads.next;

    while (le != &threads) {
        auto& t = *(::thread*)((uint8_t*)le - offsetof(::thread, process_le));

        t.suspend_count--;

        le = le->next;
    }
}

void process::suspend() {
    if (suspend_count++ != 0)
        return;

    sync_lock sl;

    lock_guard lock1(sl);
    lock_guard lock2(threads_lock);

    auto le = threads.next;

    while (le != &threads) {
        auto& t = *(::thread*)((uint8_t*)le - offsetof(::thread, process_le));

        t.suspend_count++;

        le = le->next;
    }
}

extern "C"
void trace_NtResumeProcess(uintptr_t* args) {
    auto process_handle = (HANDLE)args[0];

    start_deferred_print();

    print("NtResumeProcess: process_handle = ");
    print_handle(process_handle);
    print("\n");

    end_deferred_print();
}

extern "C"
NTSTATUS NtResumeProcess(HANDLE proc_handle) {
    object_ref objrw;
    process* p;

    TRACE1("NtResumeProcess", proc_handle);

    if (proc_handle == NtCurrentProcess()) {
        auto t = get_current_thread();

        if (!t->proc)
            return nt_status::INVALID_PARAMETER;

        p = t->proc;
        p->acquire();

        objrw = object_ref{*p};
    } else {
        objrw = deref_handle(proc_handle);
        if (!objrw)
            return nt_status::INVALID_HANDLE;

        auto& obj = objrw.get();

        if (obj.type != object_type::process)
            return nt_status::OBJECT_TYPE_MISMATCH;

        // FIXME - check access rights

        p = static_cast<process*>(&obj);
    }

    p->resume();

    return nt_status::SUCCESS;
}

extern "C"
void trace_NtSuspendProcess(uintptr_t* args) {
    auto process_handle = (HANDLE)args[0];

    start_deferred_print();

    print("NtSuspendProcess: process_handle = ");
    print_handle(process_handle);
    print("\n");

    end_deferred_print();
}

extern "C"
NTSTATUS NtSuspendProcess(HANDLE proc_handle) {
    object_ref objrw;
    process* p;

    TRACE1("NtSuspendProcess", proc_handle);

    if (proc_handle == NtCurrentProcess()) {
        auto t = get_current_thread();

        if (!t->proc)
            return nt_status::INVALID_PARAMETER;

        p = t->proc;
        p->acquire();

        objrw = object_ref{*p};
    } else {
        objrw = deref_handle(proc_handle);
        if (!objrw)
            return nt_status::INVALID_HANDLE;

        auto& obj = objrw.get();

        if (obj.type != object_type::process)
            return nt_status::OBJECT_TYPE_MISMATCH;

        // FIXME - check access rights

        p = static_cast<process*>(&obj);
    }

    p->suspend();

    return nt_status::SUCCESS;
}

void process::add_handle_no_alloc(object& obj, uint32_t access_mask,
                                  handle_t& h, handle_obj* ho) {
    obj.acquire();

    {
        lock_guard lock(handle_lock);

        h = ++last_handle;

        ho->tree.key = h;
        ho->obj = &obj;
        ho->access_mask = access_mask;

        insert_into_rbtree(handles, ho->tree);
    }
}

nt_status process::add_handle(object& obj, uint32_t access_mask,
                              bool allow_shared, handle_t& h) {
    // FIXME - reusing handles?

    if (obj.handle_count.fetch_add(1) > 0 && !allow_shared) {
        obj.handle_count--;
        return nt_status::SHARING_VIOLATION;
    }

    auto ho = (handle_obj*)malloc(sizeof(handle_obj));
    if (!ho)
        return nt_status::INSUFFICIENT_RESOURCES;

    add_handle_no_alloc(obj, access_mask, h, ho);

    return nt_status::SUCCESS;
}

NTSTATUS process::allocate_vm(void** base_address, uintptr_t, size_t* region_size,
                              uint32_t allocation_type, enum mapping_type protect) {
    // FIXME - what are we supposed to do with zero_bits?

    if (!(allocation_type & (MEM_RESERVE | MEM_COMMIT | MEM_RESET)))
        return nt_status::INVALID_PAGE_PROTECTION;

    if (allocation_type & MEM_RESET) {
        print("FIXME: process::allocate_vm MEM_RESET\n"); // FIXME
        return nt_status::INVALID_PARAMETER;
    }

    auto addr = (uintptr_t)*base_address;
    auto size = *region_size;

    auto end = addr + size;

    addr &= ~(PAGE_SIZE - 1); // round down

    auto num_pages = bytes_to_pages(end - addr);
    size = num_pages << PAGE_SHIFT;

    if (size == 0)
        return nt_status::INVALID_PARAMETER;

    if (size > numeric_limits<uint32_t>::max())
        return nt_status::INVALID_PARAMETER;

    if (allocation_type & MEM_COMMIT && !(allocation_type & MEM_RESERVE)) {
        if (addr == 0)
            return nt_status::INVALID_PARAMETER;

        *base_address = (void*)addr;
        *region_size = size;

        while (true) {
            auto m = find_mapping(*this, addr);

            if (!m)
                return nt_status::NO_MEMORY;

            auto to_commit = min(size, m->tree.key + m->size - addr);

            if (!set_page_protection(*this, addr, (uint32_t)(to_commit >> PAGE_SHIFT), protect,
                                     true, false)) {
                m->release();
                return nt_status::INSUFFICIENT_RESOURCES;
            }

            if (to_commit == size) {
                m->release();
                return nt_status::SUCCESS;
            }

            addr += to_commit;
            size -= to_commit;

            m->release();
        }

        return nt_status::NO_MEMORY;
    }

    auto m = new mapping(*this, *zero_section);
    if (!m) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    m->size = (uint32_t)size;

    {
        lock_guard lock(mappings_lock);

        if (addr == 0) {
            uintptr_t last_addr = 0x100000; // don't allocate in first megabyte

            auto n1 = get_tree_minimum(mappings);

            while (n1) {
                auto& m1 = rbtree_containing_record<mapping>(n1);

                if (m1.tree.key >= last_addr + m->size) {
                    addr = last_addr;
                    break;
                }

                n1 = next_rbtree_node(*n1);
                last_addr = max(last_addr, m1.tree.key + m1.size);
            }

            if (addr == 0) {
                auto n1 = get_tree_maximum(mappings);

                if (!n1)
                    addr = 0x100000;
                else {
                    auto& m1 = rbtree_containing_record<mapping>(n1);

                    if (m1.tree.key + m1.size + m->size >= MAX_USERSPACE_ADDRESS) {
                        m->release();
                        return nt_status::NO_MEMORY;
                    }

                    addr = m1.tree.key + m1.size;
                }
            }
        } else {
            auto n1 = find_rbtree_node_or_less(mappings, addr);

            if (!n1)
                n1 = get_tree_minimum(mappings);

            if (n1) {
                auto& m1 = rbtree_containing_record<mapping>(n1);

                if (addr < m1.tree.key + m1.size && addr + m->size > m1.tree.key) { // collision
                    m->release();
                    return nt_status::NO_MEMORY;
                }

                auto n2 = next_rbtree_node(m1.tree);

                if (n2) {
                    auto& m2 = rbtree_containing_record<mapping>(n2);

                    if (addr < m2.tree.key + m2.size && addr + m->size > m2.tree.key) { // collision
                        m->release();
                        return nt_status::NO_MEMORY;
                    }
                }
            }
        }

        m->offset = addr;
        m->tree.key = addr;
        m->in_address_space = true;
        insert_into_rbtree(mappings, m->tree);
    }

    if (!set_page_protection(*this, addr, num_pages, protect, true,
                             !(allocation_type & MEM_COMMIT))) {
        print("set_page_protection failed\n");
        {
            lock_guard lock(mappings_lock);
            remove_from_rbtree(mappings, m->tree);
            m->release();
        }
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    *base_address = (void*)addr;
    *region_size = size;

    return nt_status::SUCCESS;
}

static void print_allocation_type(uint32_t allocation_type) {
    static constexpr pair<uint32_t, string_view> values[] = {
        { MEM_COMMIT, "MEM_COMMIT" },
        { MEM_RESERVE, "MEM_RESERVE" },
        { MEM_DECOMMIT, "MEM_DECOMMIT" },
        { MEM_RELEASE, "MEM_RELEASE" },
        { MEM_FREE, "MEM_FREE" },
        { MEM_PRIVATE, "MEM_PRIVATE" },
        { MEM_MAPPED, "MEM_MAPPED" },
        { MEM_RESET, "MEM_RESET" },
        { MEM_TOP_DOWN, "MEM_TOP_DOWN" },
        { MEM_WRITE_WATCH, "MEM_WRITE_WATCH" },
        { MEM_PHYSICAL, "MEM_PHYSICAL" },
        { MEM_RESET_UNDO, "MEM_RESET_UNDO" },
        { MEM_IMAGE, "MEM_IMAGE" },
        { MEM_LARGE_PAGES, "MEM_LARGE_PAGES" },
        { MEM_4MB_PAGES, "MEM_4MB_PAGES" },
    };

    bool first = true;

    for (auto& v : values) {
        if ((allocation_type & v.first) == v.first) {
            allocation_type &= ~v.first;

            if (!first)
                print(" | ");

            print("{}", v.second);
            first = false;
        }
    }

    if (allocation_type != 0) {
        if (!first)
            print(" | ");

        print("{:x}", allocation_type);
    }

    if (first && allocation_type == 0)
        print("0");
}

void print_protect(uint32_t protect) {
    static constexpr pair<uint32_t, string_view> values[] = {
        { PAGE_NOACCESS, "PAGE_NOACCESS" },
        { PAGE_READONLY, "PAGE_READONLY" },
        { PAGE_READWRITE, "PAGE_READWRITE" },
        { PAGE_WRITECOPY, "PAGE_WRITECOPY" },
        { PAGE_EXECUTE, "PAGE_EXECUTE" },
        { PAGE_EXECUTE_READ, "PAGE_EXECUTE_READ" },
        { PAGE_EXECUTE_READWRITE, "PAGE_EXECUTE_READWRITE" },
        { PAGE_EXECUTE_WRITECOPY, "PAGE_EXECUTE_WRITECOPY" },
        { PAGE_GUARD, "PAGE_GUARD" },
        { PAGE_NOCACHE, "PAGE_NOCACHE" },
        { PAGE_WRITECOMBINE, "PAGE_WRITECOMBINE" },
    };

    bool first = true;

    for (auto& v : values) {
        if ((protect & v.first) == v.first) {
            protect &= ~v.first;

            if (!first)
                print(" | ");

            print("{}", v.second);
            first = false;
        }
    }

    if (protect != 0) {
        if (!first)
            print(" | ");

        print("{:x}", protect);
    }

    if (first && protect == 0)
        print("0");
}

extern "C"
void trace_NtAllocateVirtualMemory(uintptr_t* args) {
    auto process_handle = (HANDLE)args[0];
    auto base_address = (void**)args[1];
    auto zero_bits = (uintptr_t)args[2];
    auto region_size = (size_t*)args[3];
    auto allocation_type = (uint32_t)args[4];
    auto protect = (uint32_t)args[5];

    start_deferred_print();

    print("NtAllocateVirtualMemory: process_handle = ");
    print_handle(process_handle);
    print(", base_address = {:x}, zero_bits = {}, region_size = {:x}, allocation_type = ",
          (uintptr_t)*base_address, zero_bits, *region_size);
    print_allocation_type(allocation_type);
    print(", protect = ");
    print_protect(protect);
    print("\n");

    end_deferred_print();
}

extern "C"
NTSTATUS NtAllocateVirtualMemory(HANDLE proc_handle, void** base_address, uintptr_t zero_bits,
                                 size_t* region_size, uint32_t allocation_type, uint32_t protect) {
    object_ref objrw;
    process* p;
    enum mapping_type prot;

    TRACE6("NtAllocateVirtualMemory", proc_handle, base_address, zero_bits,
           region_size, allocation_type, protect);

    switch (protect) {
        case PAGE_NOACCESS:
        case PAGE_GUARD:
            prot = mapping_type::no_access;
            break;

        case PAGE_READONLY:
            prot = mapping_type::read_only;
            break;

        case PAGE_READWRITE:
            prot = mapping_type::read_write;
            break;

        case PAGE_EXECUTE:
        case PAGE_EXECUTE_READ:
            prot = mapping_type::read_execute;
            break;

        case PAGE_EXECUTE_READWRITE:
            prot = mapping_type::read_write_execute;
            break;

        default:
            return nt_status::INVALID_PARAMETER;
    }

    if (proc_handle == NtCurrentProcess()) {
        auto t = get_current_thread();

        if (!t->proc)
            return nt_status::INVALID_PARAMETER;

        p = t->proc;
        p->acquire();

        objrw = object_ref{*p};
    } else {
        objrw = deref_handle(proc_handle);
        if (!objrw)
            return nt_status::INVALID_HANDLE;

        auto& obj = objrw.get();

        if (obj.type != object_type::process)
            return nt_status::OBJECT_TYPE_MISMATCH;

        // FIXME - check access rights

        p = static_cast<process*>(&obj);
    }

    return p->allocate_vm(base_address, zero_bits, region_size, allocation_type, prot);
}

NTSTATUS process::free_vm(void** base_address, size_t* region_size, uint32_t free_type) {
    if (!(free_type & (MEM_DECOMMIT | MEM_RELEASE)))
        return nt_status::INVALID_PARAMETER;

    if (free_type & MEM_DECOMMIT) {
        print("FIXME: process::free_vm MEM_DECOMMIT\n"); // FIXME
        return nt_status::INVALID_PARAMETER;
    }

    if (*region_size != 0)
        return nt_status::INVALID_PARAMETER;

    // FIXME - what happens if this is called for section mapping?

    mapping* pm = nullptr;

    {
        lock_guard lock(mappings_lock);

        auto n = find_rbtree_node(mappings, (uintptr_t)*base_address);

        if (!n)
            return nt_status::INVALID_PARAMETER;

        auto& m2 = rbtree_containing_record<mapping>(n);

        if (m2.tree.key != (uintptr_t)*base_address)
            return nt_status::INVALID_PARAMETER;

        remove_from_rbtree(mappings, m2.tree);

        pm = &m2;
    }

    auto& m = *pm;

    // FIXME - if current process running on other cores and mapping changed, send IPI to clear TLB

    *base_address = (void*)m.tree.key;
    *region_size = m.size;

    m.release();

    return nt_status::SUCCESS;
}

extern "C"
void trace_NtFreeVirtualMemory(uintptr_t* args) {
    auto process_handle = (HANDLE)args[0];
    auto base_address = (void**)args[1];
    auto region_size = (size_t*)args[2];
    auto free_type = (uint32_t)args[3];

    start_deferred_print();

    print("NtFreeVirtualMemory: process_handle = ");
    print_handle(process_handle);
    print(", base_address = {:x}, region_size = {:x}, free_type = ",
          (uintptr_t)*base_address, *region_size, free_type);
    print_allocation_type(free_type);
    print("\n");

    end_deferred_print();
}

extern "C"
NTSTATUS NtFreeVirtualMemory(HANDLE proc_handle, void** base_address, size_t* region_size,
                             uint32_t free_type) {
    object_ref objrw;
    process* p;

    TRACE4("NtFreeVirtualMemory", proc_handle, base_address, region_size,
           free_type);

    if (proc_handle == NtCurrentProcess()) {
        auto t = get_current_thread();

        if (!t->proc)
            return nt_status::INVALID_PARAMETER;

        p = t->proc;
        p->acquire();

        objrw = object_ref{*p};
    } else {
        objrw = deref_handle(proc_handle);
        if (!objrw)
            return nt_status::INVALID_HANDLE;

        auto& obj = objrw.get();

        if (obj.type != object_type::process)
            return nt_status::OBJECT_TYPE_MISMATCH;

        // FIXME - check access rights

        p = static_cast<process*>(&obj);
    }

    return p->free_vm(base_address, region_size, free_type);
}


object_ref process::deref_handle(handle_t h) {
    if (h == NtCurrentProcess()) {
        this->acquire();
        return *this;
    }

    if (h == 0)
        return nullopt;

    {
        lock_guard lock(handle_lock);

        auto n = find_rbtree_node(handles, h);

        if (!n || n->key != h)
            return nullopt;

        auto& ho = rbtree_containing_record<handle_obj>(n);

        ho.obj->acquire(); // needs to be within spinlock

        return *ho.obj;
    }
}

object_ref process::deref_handle(handle_t h, ACCESS_MASK& access_mask) {
    if (h == NtCurrentProcess()) {
        this->acquire();
        return *this;
    }

    if (h == 0)
        return nullopt;

    {
        lock_guard lock(handle_lock);

        auto n = find_rbtree_node(handles, h);

        if (!n || n->key != h)
            return nullopt;

        auto& ho = rbtree_containing_record<handle_obj>(n);

        ho.obj->acquire(); // needs to be within spinlock

        access_mask = ho.access_mask;

        return *ho.obj;
    }
}

#ifdef __aarch64__
static uint32_t get_page_protection(process& proc, uint64_t va) {
    span<ARM64_HARDWARE_PTE> pt[4];
    uint16_t index[4];

    index[0] = (va & 0x1ff000) >> 12;
    index[1] = (va & 0x3fe00000) >> 21;
    index[2] = (va & 0x7fc0000000) >> 30;
    index[3] = (va & 0xff8000000000) >> 39;

    pt[3] = span((ARM64_HARDWARE_PTE*)phys_address(proc.ttbr0), 512);

    for (uint8_t level = 3; level > 0; level--) {
        if (!pt[level][index[level]].valid)
            return PAGE_NOACCESS;

        pt[level - 1] = span((ARM64_HARDWARE_PTE*)phys_address((uint64_t)((pt[level][index[level]].pfn) << PAGE_SHIFT)), 512);
    }

    auto& pte = pt[0][index[0]];

    if (pte.UXN) {
        if (pte.cow)
            return PAGE_WRITECOPY;
        else if (pte.allow_write)
            return PAGE_READWRITE;
        else
            return PAGE_READONLY;
    } else {
        if (pte.cow)
            return PAGE_EXECUTE_WRITECOPY;
        else if (pte.allow_write)
            return PAGE_EXECUTE_READWRITE;
        else
            return PAGE_EXECUTE_READ;
    }
}
#elifdef __x86_64__
static uint32_t get_page_protection(process& proc, uint64_t va) {
    span<HARDWARE_PTE> pt[4];
    uint16_t index[4];

    index[0] = (va & 0x1ff000) >> 12;
    index[1] = (va & 0x3fe00000) >> 21;
    index[2] = (va & 0x7fc0000000) >> 30;
    index[3] = (va & 0xff8000000000) >> 39;

    pt[3] = span((HARDWARE_PTE*)phys_address(proc.cr3), 512);

    for (uint8_t level = 3; level > 0; level--) {
        if (!pt[level][index[level]].p)
            return PAGE_NOACCESS;

        pt[level - 1] = span((HARDWARE_PTE*)phys_address((uint64_t)((pt[level][index[level]].pfn) << PAGE_SHIFT)), 512);
    }

    auto& pte = pt[0][index[0]];

    if (pte.xd) {
        if (pte.cow)
            return PAGE_WRITECOPY;
        else if (pte.allow_write)
            return PAGE_READWRITE;
        else
            return PAGE_READONLY;
    } else {
        if (pte.cow)
            return PAGE_EXECUTE_WRITECOPY;
        else if (pte.allow_write)
            return PAGE_EXECUTE_READWRITE;
        else
            return PAGE_EXECUTE_READ;
    }
}
#endif

static nt_status protect_vm(process& proc, void** base_address, size_t* region_size,
                            enum mapping_type prot, bool cow, uint32_t& old_type) {
    auto addr = (uintptr_t)*base_address;
    auto addr_end = addr + *region_size;

    addr &= ~(PAGE_SIZE - 1);

    auto pages = bytes_to_pages(addr_end - addr);

    if (pages == 0)
        return nt_status::INVALID_PARAMETER;

    *base_address = (void*)addr;
    *region_size = pages << PAGE_SHIFT;

    bool first = true;

    {
        rangelock_req rlr(proc.rl, addr >> PAGE_SHIFT, pages, rangelock_mode::exclusive);

        while (true) {
            auto m = find_mapping(proc, addr);
            if (!m)
                break;

            auto num_pages = min(pages, (uint32_t)((m->tree.key + m->size - addr) >> PAGE_SHIFT));

            if (first)
                old_type = get_page_protection(proc, addr);

            if (m->sect.type != section_type::image) {
                if (prot == mapping_type::read_write_execute ||
                    prot == mapping_type::read_execute) {
                    switch (m->sect.page_protection) {
                        case PAGE_EXECUTE_READ:
                        case PAGE_EXECUTE_WRITECOPY:
                        case PAGE_EXECUTE_READWRITE:
                        case PAGE_EXECUTE:
                            break;

                        default:
                            return nt_status::ACCESS_DENIED;
                    }
                }

                if (!cow && (prot == mapping_type::read_write_execute ||
                             prot == mapping_type::read_write)) {
                    switch (m->sect.page_protection) {
                        case PAGE_READWRITE:
                        case PAGE_EXECUTE_READWRITE:
                            break;

                        default:
                            return nt_status::ACCESS_DENIED;
                    }
                }
            }

            bool cow2 = cow;

            // force COW on for images or zero section
            if (m->sect.type == section_type::image || &m->sect == zero_section)
                cow2 = true;

            if (!set_page_protection(proc, addr, num_pages, prot, cow2, false)) {
                m->release();
                return nt_status::INSUFFICIENT_RESOURCES;
            }

            m->release();

            first = false;

            pages -= num_pages;

            if (pages == 0)
                break;

            addr += num_pages << PAGE_SHIFT;
        }
    }

    if (pages != 0)
        return nt_status::NO_MEMORY;

    // FIXME - if mappings changed and process running on other cores, send IPI to clear TLB

    return nt_status::SUCCESS;
}

extern "C"
void trace_NtProtectVirtualMemory(uintptr_t* args) {
    auto process_handle = (HANDLE)args[0];
    auto base_address = (void**)args[1];
    auto region_size = (size_t*)args[2];
    auto new_prot = (uint32_t)args[3];

    start_deferred_print();

    print("NtProtectVirtualMemory: process_handle = ");
    print_handle(process_handle);
    print(", base_address = {:x}, region_size = {:x}, new_prot = ",
          (uintptr_t)*base_address, *region_size);
    print_protect(new_prot);
    print("\n");

    end_deferred_print();
}

extern "C"
NTSTATUS NtProtectVirtualMemory(HANDLE proc_handle, void** base_address, size_t* region_size,
                                uint32_t new_prot, uint32_t* old_prot) {
    enum mapping_type prot;
    object_ref objrw;
    bool cow;
    process* p;

    TRACE4("NtProtectVirtualMemory", proc_handle, base_address, region_size,
           new_prot);

    switch (new_prot) {
        case PAGE_NOACCESS:
        case PAGE_GUARD:
            prot = mapping_type::no_access;
            cow = false;
            break;

        case PAGE_READONLY:
            prot = mapping_type::read_only;
            cow = false;
            break;

        case PAGE_READWRITE:
            prot = mapping_type::read_write;
            cow = false;
            break;

        case PAGE_EXECUTE:
        case PAGE_EXECUTE_READ:
            prot = mapping_type::read_execute;
            cow = false;
            break;

        case PAGE_EXECUTE_READWRITE:
            prot = mapping_type::read_write_execute;
            cow = false;
            break;

        case PAGE_WRITECOPY:
            prot = mapping_type::read_write;
            cow = true;
            break;

        case PAGE_EXECUTE_WRITECOPY:
            prot = mapping_type::read_write_execute;
            cow = true;
            break;

        default:
            return nt_status::INVALID_PARAMETER;
    }

    if (proc_handle == NtCurrentProcess()) {
        auto t = get_current_thread();

        if (!t->proc)
            return nt_status::INVALID_PARAMETER;

        p = t->proc;
        p->acquire();

        objrw = object_ref{*p};
    } else {
        objrw = deref_handle(proc_handle);
        if (!objrw)
            return nt_status::INVALID_HANDLE;

        auto& obj = objrw.get();

        if (obj.type != object_type::process)
            return nt_status::OBJECT_TYPE_MISMATCH;

        // FIXME - check access rights

        p = static_cast<process*>(&obj);
    }

    return protect_vm(*p, base_address, region_size, prot, cow, *old_prot);
}

NTSTATUS process::close_handle(handle_t h) {
    if (!h)
        return nt_status::INVALID_HANDLE;

    handle_obj* ho;

    {
        lock_guard lock(handle_lock);

        auto n = find_rbtree_node(handles, h);

        if (!n || n->key != h)
            return nt_status::INVALID_HANDLE;

        ho = &rbtree_containing_record<handle_obj>(n);

        remove_from_rbtree(handles, ho->tree);
    }

    handle_closed(*ho->obj, this);
    object_release(*ho->obj);

    free(ho);

    return nt_status::SUCCESS;
}

NTSTATUS process::map_view_of_section(section& sect, void** base_address, uintptr_t, size_t,
                                      optional<int64_t> section_offset, size_t& view_size,
                                      enum SECTION_INHERIT, uint32_t,
                                      enum mapping_type protect, bool cow) {
    // FIXME - zero_bits
    // FIXME - commit_size
    // FIXME - view_size
    // FIXME - inherit_disposition
    // FIXME - allocation_type

    auto addr = (uintptr_t)*base_address;

    if (addr & (PAGE_SIZE - 1))
        addr &= ~(PAGE_SIZE - 1);

    addr = map_section(sect, addr, section_offset, protect, cow, &view_size);

    if (addr == 0)
        return nt_status::INTERNAL_ERROR;

    *base_address = (void*)addr;

    return nt_status::SUCCESS;
}

NTSTATUS process::unmap_view_of_section(void* base_address) {
    auto m = find_mapping(*this, (uintptr_t)base_address);
    if (!m)
        return nt_status::NO_MEMORY;

    {
        lock_guard lock(mappings_lock);
        remove_from_rbtree(mappings, m->tree);
    }

    m->release(); // because we're removing from the list
    m->release(); // and again because of find_mapping

    return nt_status::SUCCESS;
}

extern "C"
void trace_NtCreateProcess(uintptr_t* args) {
    auto parent_process = (HANDLE)args[0];
    auto section_handle = (HANDLE)args[1];

    start_deferred_print();

    print("NtCreateProcess: parent_process = ");
    print_handle(parent_process);
    print(", section_handle = ");
    print_handle(section_handle);
    print("\n");

    end_deferred_print();
}

extern "C"
NTSTATUS NtCreateProcess(HANDLE* process_handle, ACCESS_MASK, const OBJECT_ATTRIBUTES*,
                         HANDLE parent_process, bool, HANDLE section_handle,
                         HANDLE, HANDLE) {
    object_ref parent;

    TRACE2("NtCreateProcess", parent_process, section_handle);

#ifdef __aarch64__
    static const uint16_t this_arch = IMAGE_FILE_MACHINE_ARM64;
#elifdef __x86_64__
    static const uint16_t this_arch = IMAGE_FILE_MACHINE_AMD64;
#endif

    // FIXME - desired_access
    // FIXME - object_attributes
    // FIXME - inherit_object_table
    // FIXME - debug_port
    // FIXME - exception_port

    if (disallow_new_processes)
        return nt_status::SYSTEM_SHUTDOWN;

    auto sect = deref_handle(section_handle);
    if (!sect)
        return nt_status::INVALID_HANDLE;

    if (sect.get().type != object_type::section)
        return nt_status::OBJECT_TYPE_MISMATCH;

    auto& img = *static_cast<section*>(&sect.get());

    if (img.type != section_type::image)
        return nt_status::INVALID_PARAMETER;

    if (img.machine != this_arch)
        return nt_status::IMAGE_MACHINE_TYPE_MISMATCH_EXE;

    if (img.characteristics & IMAGE_FILE_DLL)
        return nt_status::INVALID_IMAGE_FORMAT;

    if (parent_process == NtCurrentProcess()) {
        auto& t = *get_current_thread();

        t.proc->acquire();

        parent = object_ref{*t.proc};
    } else {
        parent = deref_handle(parent_process);
        if (!parent)
            return nt_status::INVALID_HANDLE;

        if (parent.get().type != object_type::process)
            return nt_status::OBJECT_TYPE_MISMATCH;
    }

    auto* pg = static_cast<process&>(parent.get()).pg;

    auto pptr = new process(*pg);
    if (!pptr)
        return nt_status::INSUFFICIENT_RESOURCES;

    if (!pptr->initialized) {
        print("process::process failed\n");
        delete pptr;
        return nt_status::INTERNAL_ERROR;
    }

    auto& p = *pptr;

    p.tree.key = next_pid.fetch_add(1);

    p.img = img;
    img.acquire();

#ifdef __aarch64__
    if (!p.allocate_ttbr0()) {
        print("process::allocate_ttbr0 failed\n");
        p.release();
        return nt_status::INTERNAL_ERROR;
    }
#elifdef __x86_64__
    if (!p.allocate_cr3()) {
        print("process::allocate_cr3 failed\n");
        p.release();
        return nt_status::INTERNAL_ERROR;
    }
#endif

    p.img_addr = p.map_section(img, img.preferred_base + img.size > MAX_USERSPACE_ADDRESS ? 0 : img.preferred_base,
                               nullopt, mapping_type::read_only, true, nullptr);
    if (!p.img_addr) {
        print("process::map_section failed\n");
        p.release();
        return nt_status::INTERNAL_ERROR;
    }

    // FIXME - if ntdll would have collision, choose another address

    p.ntdll_addr = p.map_section(*ntdll, ntdll->preferred_base,
                                 nullopt, mapping_type::read_only, true,
                                 nullptr);
    if (!p.ntdll_addr) {
        print("process::map_section failed\n");
        p.release();
        return nt_status::INTERNAL_ERROR;
    }

    if (!p.map_section(*time_page, TIME_PAGE_BASE, nullopt,
                       mapping_type::read_only, false, nullptr)) {
        print("process::map_section failed for time page\n");
        p.release();
        return nt_status::INTERNAL_ERROR;
    }

    char* path = nullptr;
    size_t path_len;

    if (img.file) {
        auto& f = img.file->get();
        dentry* d = nullptr;

        {
            lock_guard lg(f.lock);

            if (f.dentry) {
                d = f.dentry;
                d->acquire();
            }
        }

        if (d) {
            auto status = get_dentry_path(*d, path, path_len);
            if (!success(status)) {
                print("get_dentry_path returned {}\n", status);
                d->release();
                p.release();
                return status;
            }

            d->release();
        }
    }

    string_view pathsv;

    if (path)
        pathsv = string_view(path, path_len);

    if (!allocate_peb(p, 0, 0, 0, 0, "", pathsv, u"")) {
        print("allocate_peb failed\n");
        p.release();

        if (path)
            free(path);

        return nt_status::INTERNAL_ERROR;
    }

    if (path)
        free(path);

    // FIXME - token for process

    // FIXME - give process a PID
    // FIXME - add process to list

    p.suspend_count = 0;

    {
        lock_guard lock(process_list_lock);
        insert_into_rbtree(process_tree, p.tree);
    }

    p.added_to_tree = true;

    auto& curproc = *get_current_thread()->proc;

    handle_t h;

    auto status = curproc.add_handle(p, 0, true, h); // FIXME - access mask
    if (!success(status)) {
        p.release();
        return status;
    }

    auto& pp = static_cast<process&>(parent.get());

    {
        lock_guard lock(process_list_lock);
        p.parent_process = &pp;
    }

    p.acquire(); // for children list

    {
        lock_guard lock(pp.children_lock);
        insert_tail_list(pp.children, p.child_le);
    }

    p.release(); // add_handle increases refcount

    *process_handle = h;

    return nt_status::SUCCESS;
}

static void print_duplicate_object_options(uint32_t options) {
    static constexpr pair<uint32_t, string_view> values[] = {
        { DUPLICATE_CLOSE_SOURCE, "DUPLICATE_CLOSE_SOURCE" },
        { DUPLICATE_SAME_ACCESS, "DUPLICATE_SAME_ACCESS" },
        { DUPLICATE_SAME_ATTRIBUTES, "DUPLICATE_SAME_ATTRIBUTES" },
    };

    bool first = true;

    for (auto& v : values) {
        if ((options & v.first) == v.first) {
            options &= ~v.first;

            if (!first)
                print(" | ");

            print("{}", v.second);
            first = false;
        }
    }

    if (options != 0) {
        if (!first)
            print(" | ");

        print("{:x}", options);
    }

    if (first && options == 0)
        print("0");
}

extern "C"
void trace_NtDuplicateObject(uintptr_t* args) {
    auto source_process_handle = (HANDLE)args[0];
    auto source_handle = (HANDLE)args[1];
    auto target_process_handle = (HANDLE)args[2];
    auto desired_access = (uint32_t)args[3];
    auto options = (uint32_t)args[4];

    // FIXME - we could print access mask nicely if we dereferenced HANDLE to find out its type

    start_deferred_print();

    print("NtDuplicateObject: source_process_handle = ");
    print_handle(source_process_handle);
    print(", source_handle = ");
    print_handle(source_handle);
    print(", target_process_handle = ");
    print_handle(target_process_handle);
    print(", desired_access = {:x}, options = ", desired_access);
    print_duplicate_object_options(options);
    print("\n");

    end_deferred_print();
}

extern "C"
NTSTATUS NtDuplicateObject(HANDLE source_process_handle, HANDLE source_handle, HANDLE target_process_handle,
                           HANDLE* target_handle, ACCESS_MASK desired_access, uint32_t, uint32_t options) {
    object_ref srcprocobj, destprocobj;
    process* srcproc;
    process* destproc;
    ACCESS_MASK current_access;
    object_ref objrw;

    TRACE5("NtDuplicateObject", source_process_handle, source_handle,
           target_process_handle, desired_access, options);

    // FIXME - handle_attributes
    // FIXME - options (DUPLICATE_SAME_ATTRIBUTES, DUPLICATE_CLOSE_SOURCE)

    if (source_process_handle == NtCurrentProcess()) {
        auto t = get_current_thread();

        if (!t->proc)
            return nt_status::INVALID_PARAMETER;

        srcproc = t->proc;
        srcproc->acquire();

        srcprocobj = object_ref{*srcproc};
    } else {
        srcprocobj = deref_handle(source_process_handle);
        if (!srcprocobj)
            return nt_status::INVALID_HANDLE;

        auto& obj = srcprocobj.get();

        if (obj.type != object_type::process)
            return nt_status::OBJECT_TYPE_MISMATCH;

        // FIXME - check access rights

        srcproc = static_cast<process*>(&obj);
    }

    if (target_process_handle == NtCurrentProcess()) {
        auto t = get_current_thread();

        if (!t->proc)
            return nt_status::INVALID_PARAMETER;

        destproc = t->proc;
        destproc->acquire();

        destprocobj = object_ref{*destproc};
    } else {
        destprocobj = deref_handle(target_process_handle);
        if (!destprocobj)
            return nt_status::INVALID_HANDLE;

        auto& obj = destprocobj.get();

        if (obj.type != object_type::process)
            return nt_status::OBJECT_TYPE_MISMATCH;

        // FIXME - check access rights

        destproc = static_cast<process*>(&obj);
    }

    if (source_handle == NtCurrentProcess()) {
        auto& t = *get_current_thread();

        if (!t.proc)
            return nt_status::INVALID_PARAMETER;

        t.proc->acquire();

        objrw = object_ref{*t.proc};
    } else if (source_handle == NtCurrentThread()) {
        auto& t = *get_current_thread();

        t.acquire();

        objrw = object_ref{t};
    } else {
        objrw = srcproc->deref_handle(source_handle, current_access);

        if (!objrw)
            return nt_status::INVALID_HANDLE;
    }

    if (options & DUPLICATE_SAME_ACCESS)
        desired_access = current_access;
    else if (desired_access & ~current_access)
        return nt_status::ACCESS_DENIED;

    auto& obj = objrw.get();

    handle_t h;
    auto status = destproc->add_handle(obj, desired_access, true, h);

    if (!success(status))
        return status;

    if (target_handle)
        *target_handle = h;

    return nt_status::SUCCESS;
}

NTSTATUS process::read_vm(const void* base_address, void* buffer,
                          uint32_t buffer_size, uint32_t* number_of_bytes_read) {
    uint32_t copied = 0;

    while (true) {
        auto m = find_mapping(*this, (uintptr_t)base_address);
        if (!m)
            break;

        auto to_copy = min(buffer_size, (uint32_t)(m->tree.key + m->size - (uintptr_t)base_address));

        if (!m->copy_from((uint32_t)((uintptr_t)base_address - m->tree.key), buffer, (uint32_t)to_copy)) {
            m->release();
            return nt_status::INTERNAL_ERROR;
        }

        m->release();

        copied += to_copy;

        if (buffer_size == to_copy)
            break;

        base_address = (uint8_t*)base_address + to_copy;
        buffer = (uint8_t*)buffer + to_copy;
        buffer_size -= to_copy;
    }

    if (copied == 0)
        return nt_status::NO_MEMORY;

    if (number_of_bytes_read)
        *number_of_bytes_read = copied;

    return nt_status::SUCCESS;
}

extern "C"
void trace_NtReadVirtualMemory(uintptr_t* args) {
    auto process_handle = (HANDLE)args[0];
    auto base_address = (void*)args[1];
    auto us_buffer = (void*)args[2];
    auto buffer_size = (uint32_t)args[3];

    start_deferred_print();

    print("NtReadVirtualMemory: process_handle = ");
    print_handle(process_handle);
    print(", base_address = {:x}, us_buffer = {:x}, buffer_size = {}\n",
          (uintptr_t)base_address, (uintptr_t)us_buffer, buffer_size);

    end_deferred_print();
}

extern "C"
NTSTATUS NtReadVirtualMemory(HANDLE process_handle, void* base_address,
                             void* us_buffer, uint32_t buffer_size,
                             uint32_t* number_of_bytes_read) {
    auto& t = *get_current_thread();
    object_ref objrw;
    process* src_proc;
    process* dest_proc = t.proc;
    nt_status status;

    TRACE4("NtReadVirtualMemory", process_handle, base_address, us_buffer,
           buffer_size);

    if (!dest_proc)
        return nt_status::INVALID_PARAMETER;

    if (buffer_size == 0)
        return nt_status::INVALID_PARAMETER;

    if (process_handle == NtCurrentProcess()) {
        src_proc = t.proc;
        src_proc->acquire();

        objrw = object_ref{*src_proc};
    } else {
        objrw = deref_handle(process_handle);
        if (!objrw)
            return nt_status::INVALID_HANDLE;

        auto& obj = objrw.get();

        if (obj.type != object_type::process)
            return nt_status::OBJECT_TYPE_MISMATCH;

        // FIXME - check access rights

        src_proc = static_cast<process*>(&obj);
    }

    span<const uint8_t>* src_spans = nullptr;
    span<uint8_t>* dest_spans = nullptr;
    size_t num_src_spans = 0, num_dest_spans = 0;

    auto free_src_spans = [&]() {
        auto src_spans_size = sizeof(span<const uint8_t>) * num_src_spans;
        if (src_spans_size > PAGE_SIZE)
            vfree(src_spans, src_spans_size);
        else if (src_spans_size > 0)
            free(src_spans);
    };

    auto free_dest_spans = [&]() {
        auto dest_spans_size = sizeof(span<uint8_t>) * num_dest_spans;
        if (dest_spans_size > PAGE_SIZE)
            vfree(dest_spans, dest_spans_size);
        else if (dest_spans_size > 0)
            free(dest_spans);
    };

    if (src_proc == dest_proc) {
        optional<rangelock_req> rlr;

        status = pin_userspace_buffers_for_read_and_write(*src_proc,
                                                          span((uint8_t*)base_address, buffer_size),
                                                          span((uint8_t*)us_buffer, buffer_size),
                                                          rlr, src_spans,
                                                          num_src_spans,
                                                          dest_spans,
                                                          num_dest_spans);
        if (!success(status))
            return status;

        memcpy_vec(span(dest_spans, num_dest_spans), span(src_spans, num_src_spans));

        rlr.reset();
    } else {
        optional<rangelock_req> src_rlr, dest_rlr;

        auto do_dest = [&]() {
            return pin_userspace_buffer_for_writing(*dest_proc,
                                                    span((uint8_t*)us_buffer, buffer_size),
                                                    dest_rlr, dest_spans,
                                                    num_dest_spans);
        };

        if ((uintptr_t)dest_proc < (uintptr_t)src_proc) {
            status = do_dest();
            if (!success(status))
                return status;
        }

        status = pin_userspace_buffer_for_reading(*src_proc,
                                                  span((uint8_t*)base_address, buffer_size),
                                                  src_rlr, src_spans,
                                                  num_src_spans);
        if (!success(status)) {
            if ((uintptr_t)dest_proc < (uintptr_t)src_proc)
                free_dest_spans();

            return status;
        }

        if ((uintptr_t)dest_proc > (uintptr_t)src_proc) {
            status = do_dest();
            if (!success(status)) {
                free_src_spans();
                return status;
            }
        }

        memcpy_vec(span(dest_spans, num_dest_spans), span(src_spans, num_src_spans));

        src_rlr.reset();
        dest_rlr.reset();
    }

    free_src_spans();
    free_dest_spans();

    // FIXME - should we be allowing short reads?

    if (number_of_bytes_read)
        *number_of_bytes_read = buffer_size;

    return nt_status::SUCCESS;
}

NTSTATUS process::write_vm(void* base_address, const void* buffer,
                           uint32_t buffer_size, uint32_t* number_of_bytes_written) {
    uint32_t copied = 0;

    while (true) {
        auto m = find_mapping(*this, (uintptr_t)base_address);
        if (!m)
            break;

        auto to_copy = min(buffer_size, (uint32_t)(m->tree.key + m->size - (uintptr_t)base_address));

        if (!m->copy_to((uint32_t)((uintptr_t)base_address - m->tree.key), buffer, (uint32_t)to_copy)) {
            m->release();
            return nt_status::INTERNAL_ERROR;
        }

        m->release();

        copied += to_copy;

        if (buffer_size == to_copy)
            break;

        base_address = (uint8_t*)base_address + to_copy;
        buffer = (uint8_t*)buffer + to_copy;
        buffer_size -= to_copy;
    }

    if (copied == 0)
        return nt_status::NO_MEMORY;

    if (number_of_bytes_written)
        *number_of_bytes_written = copied;

    return nt_status::SUCCESS;
}

extern "C"
void trace_NtWriteVirtualMemory(uintptr_t* args) {
    auto process_handle = (HANDLE)args[0];
    auto base_address = (void*)args[1];
    auto us_buffer = (const void*)args[2];
    auto buffer_size = (uint32_t)args[3];

    start_deferred_print();

    print("NtWriteVirtualMemory: process_handle = ");
    print_handle(process_handle);
    print(", base_address = {:x}, us_buffer = {:x}, buffer_size = {}\n",
          (uintptr_t)base_address, (uintptr_t)us_buffer, buffer_size);

    end_deferred_print();
}

extern "C"
NTSTATUS NtWriteVirtualMemory(HANDLE process_handle, void* base_address,
                              const void* us_buffer, uint32_t buffer_size,
                              uint32_t* number_of_bytes_written) {
    auto& t = *get_current_thread();
    object_ref objrw;
    process* src_proc = t.proc;
    process* dest_proc;
    nt_status status;

    TRACE4("NtWriteVirtualMemory", process_handle, base_address, us_buffer,
           buffer_size);

    if (!src_proc)
        return nt_status::INVALID_PARAMETER;

    if (buffer_size == 0)
        return nt_status::INVALID_PARAMETER;

    if (process_handle == NtCurrentProcess()) {
        dest_proc = t.proc;
        dest_proc->acquire();

        objrw = object_ref{*dest_proc};
    } else {
        objrw = deref_handle(process_handle);
        if (!objrw)
            return nt_status::INVALID_HANDLE;

        auto& obj = objrw.get();

        if (obj.type != object_type::process)
            return nt_status::OBJECT_TYPE_MISMATCH;

        // FIXME - check access rights

        dest_proc = static_cast<process*>(&obj);
    }

    span<const uint8_t>* src_spans = nullptr;
    span<uint8_t>* dest_spans = nullptr;
    size_t num_src_spans = 0, num_dest_spans = 0;

    auto free_src_spans = [&]() {
        auto src_spans_size = sizeof(span<const uint8_t>) * num_src_spans;
        if (src_spans_size > PAGE_SIZE)
            vfree(src_spans, src_spans_size);
        else if (src_spans_size > 0)
            free(src_spans);
    };

    auto free_dest_spans = [&]() {
        auto dest_spans_size = sizeof(span<uint8_t>) * num_dest_spans;
        if (dest_spans_size > PAGE_SIZE)
            vfree(dest_spans, dest_spans_size);
        else if (dest_spans_size > 0)
            free(dest_spans);
    };

    if (src_proc == dest_proc) {
        optional<rangelock_req> rlr;

        status = pin_userspace_buffers_for_read_and_write(*src_proc,
                                                          span((uint8_t*)us_buffer, buffer_size),
                                                          span((uint8_t*)base_address, buffer_size),
                                                          rlr, src_spans,
                                                          num_src_spans,
                                                          dest_spans,
                                                          num_dest_spans);
        if (!success(status))
            return status;

        memcpy_vec(span(dest_spans, num_dest_spans), span(src_spans, num_src_spans));

        rlr.reset();
    } else {
        optional<rangelock_req> src_rlr, dest_rlr;

        auto do_dest = [&]() {
            return pin_userspace_buffer_for_writing(*dest_proc,
                                                    span((uint8_t*)base_address, buffer_size),
                                                    dest_rlr, dest_spans,
                                                    num_dest_spans);
        };

        if ((uintptr_t)dest_proc < (uintptr_t)src_proc) {
            status = do_dest();
            if (!success(status))
                return status;
        }

        status = pin_userspace_buffer_for_reading(*src_proc,
                                                  span((uint8_t*)us_buffer, buffer_size),
                                                  src_rlr, src_spans,
                                                  num_src_spans);
        if (!success(status)) {
            if ((uintptr_t)dest_proc < (uintptr_t)src_proc)
                free_dest_spans();

            return status;
        }

        if ((uintptr_t)dest_proc > (uintptr_t)src_proc) {
            status = do_dest();
            if (!success(status)) {
                free_src_spans();
                return status;
            }
        }

        memcpy_vec(span(dest_spans, num_dest_spans), span(src_spans, num_src_spans));

        src_rlr.reset();
        dest_rlr.reset();
    }

    free_src_spans();
    free_dest_spans();

    // FIXME - should we be allowing short writes?

    if (number_of_bytes_written)
        *number_of_bytes_written = buffer_size;

    return nt_status::SUCCESS;
}

nt_status process::query_basic_info(span<uint8_t> buf, uint32_t& return_length) {
    if (buf.size() < sizeof(PROCESS_BASIC_INFORMATION))
        return nt_status::INFO_LENGTH_MISMATCH;

    auto& pbi = *(PROCESS_BASIC_INFORMATION*)buf.data();

    pbi.ExitStatus = exit_status;
    pbi.PebBaseAddress = peb;
    pbi.AffinityMask = 0xffffffff; // FIXME
    pbi.BasePriority = 0; // FIXME
    pbi.UniqueProcessId = tree.key;

    {
        lock_guard lock(process_list_lock);

        if (parent_process)
            pbi.InheritedFromUniqueProcessId = parent_process->tree.key;
        else
            pbi.InheritedFromUniqueProcessId = 0;
    }

    return_length = sizeof(PROCESS_BASIC_INFORMATION);

    return nt_status::SUCCESS;
}

nt_status process::query_image_file_name(span<uint8_t> buf, uint32_t& return_length) {
    RTL_USER_PROCESS_PARAMETERS* params;
    UNICODE_STRING path_name;

    auto status = read_vm(&peb->ProcessParameters, &params, sizeof(params), nullptr);
    if (!success(status))
        return status;

    status = read_vm(&params->ImagePathName, &path_name, sizeof(path_name),
                        nullptr);
    if (!success(status))
        return status;

    auto len = (uint32_t)(sizeof(UNICODE_STRING) + path_name.Length + sizeof(char16_t));

    if (buf.size() < len) {
        return_length = len;

        return nt_status::INFO_LENGTH_MISMATCH;
    }

    auto& us = *(UNICODE_STRING*)buf.data();

    memset(&us, 0, sizeof(us));
    us.Length = path_name.Length;
    us.MaximumLength = us.Length + sizeof(char16_t);
    us.Buffer = (char16_t*)(buf.data() + sizeof(UNICODE_STRING));

    status = read_vm(path_name.Buffer, us.Buffer, us.Length, nullptr);
    if (!success(status))
        return status;

    us.Buffer[us.Length / sizeof(char16_t)] = 0;

    return_length = len;

    return nt_status::SUCCESS;
}

nt_status process::query_times(span<uint8_t> buf, uint32_t& return_length) {
    uint64_t time_running = 0, time_running_in_userspace = 0;

    if (buf.size() < sizeof(KERNEL_USER_TIMES))
        return nt_status::BUFFER_TOO_SMALL;

    auto& kut = *(KERNEL_USER_TIMES*)buf.data();

    kut.CreateTime.QuadPart = 0; // FIXME
    kut.ExitTime.QuadPart = 0; // FIXME

    {
        lock_guard lock(threads_lock);

        for (auto le = threads.next; le != &threads; le = le->next) {
            auto& t = *(::thread*)((uint8_t*)le - offsetof(::thread, process_le));

            time_running += t.time_running;
            time_running_in_userspace += t.time_running_in_userspace;
        }
    }

    time_running += dead_threads_time_running;
    time_running_in_userspace += dead_threads_time_running_in_userspace;

    auto freq = kernel_time_page.tsc_freq;

    // units of 100ns
    kut.UserTime.QuadPart = (int64_t)((unsigned __int128)time_running_in_userspace * 10000000 / freq);
    kut.KernelTime.QuadPart = (int64_t)((unsigned __int128)(time_running - time_running_in_userspace) * 10000000 / freq);

    return_length = sizeof(KERNEL_USER_TIMES);

    return nt_status::SUCCESS;
}

nt_status process::query_children_times(span<uint8_t> buf, uint32_t& return_length) {
    uint64_t time_running = 0, time_running_in_userspace = 0;

    if (buf.size() < sizeof(KERNEL_USER_TIMES))
        return nt_status::BUFFER_TOO_SMALL;

    auto& kut = *(KERNEL_USER_TIMES*)buf.data();

    kut.CreateTime.QuadPart = 0;
    kut.ExitTime.QuadPart = 0;

    {
        lock_guard lock(threads_lock);

        time_running = children_time_running;
        time_running_in_userspace = children_time_running_in_userspace;
    }

    auto freq = kernel_time_page.tsc_freq;

    // units of 100ns
    kut.UserTime.QuadPart = (int64_t)((unsigned __int128)time_running_in_userspace * 10000000 / freq);
    kut.KernelTime.QuadPart = (int64_t)((unsigned __int128)(time_running - time_running_in_userspace) * 10000000 / freq);

    return_length = sizeof(KERNEL_USER_TIMES);

    return nt_status::SUCCESS;
}

nt_status process::query_find_ntdll(span<uint8_t> buf, uint32_t& return_length) {
    if (buf.size() < sizeof(uintptr_t))
        return nt_status::BUFFER_TOO_SMALL;

    *(uintptr_t*)buf.data() = ntdll_addr;

    return_length = sizeof(uintptr_t);

    return nt_status::SUCCESS;
}

nt_status process::set_wait_pid(span<const uint8_t> buf) {
    auto& t = *get_current_thread();
    nt_status status;

    if (buf.size() < sizeof(MN_WAITPID))
        return nt_status::BUFFER_TOO_SMALL;

    auto& params = *(MN_WAITPID*)buf.data();

    if (!params.h)
        return nt_status::INVALID_PARAMETER;

    // userspace should have already translated 0 into pgid
    if (params.pid == 0)
        return nt_status::INVALID_PARAMETER;

    // FIXME - params.suspended
    // FIXME - params.continued

    do {
        process* child = nullptr;
        bool not_found = false;

        {
            disable_interrupts();
            acquire_sync_lock();

            if (t.alerted) {
                t.alerted = false;
                release_sync_lock();
                enable_interrupts();
                return nt_status::ALERTED;
            }

            {
                lock_guard lock2(process_list_lock);

                {
                    lock_guard lock3(children_lock);

                    bool filter_matched = false;

                    for (auto le = children.next; le != &children; le = le->next) {
                        auto& c = *(process*)((uint8_t*)le - offsetof(process, child_le));

                        if (params.pid < -1) { // member of specific process group
                            lock_guard lock(c.pg_lock);

                            if (c.pg->tree.key != (uint64_t)-params.pid)
                                continue;
                        } else if (params.pid > 0) { // specific process
                            if (c.tree.key != (uint64_t)params.pid)
                                continue;
                        }

                        filter_matched = true;

                        if (c.signalled) {
                            remove_entry_list(c.child_le);
                            child = &c;
                            break;
                        }
                    }

                    if (!filter_matched)
                        not_found = true;
                    else if (child) {
                        bool clear_event = true;

                        for (auto le = children.next; le != &children; le = le->next) {
                            auto& c = *(process*)((uint8_t*)le - offsetof(process, child_le));

                            if (c.signalled) {
                                clear_event = false;
                                break;
                            }
                        }

                        if (clear_event) {
                            auto& e = static_cast<event&>(unreaped_children_event.get());

                            e.signalled = false;
                        }
                    }
                }

                if (child)
                    child->parent_process = nullptr;
            }

            if (child || params.no_hang || not_found) {
                release_sync_lock();
                enable_interrupts();

                if (not_found)
                    return nt_status::NOT_FOUND;
            } else {
                t.wait_obj.wait_type = WAIT_TYPE_READING;
                insert_tail_list(wait_child_waiters, t.wait_obj.wait_le);

                t.woken_by_alert = false;

                set_current_thread_waiting(true);

                if (t.status == thread_status::dying)
                    return nt_status::THREAD_IS_TERMINATING;

                if (t.suspend_count.load() != 0)
                    return nt_status::ALERTED;

                if (t.alerted && t.woken_by_alert) {
                    t.alerted = false;
                    t.woken_by_alert = false;
                    return nt_status::ALERTED;
                }

                {
                    lock_guard lock(t.apc_lock);

                    if (!is_list_empty(t.pending_apcs))
                        return nt_status::USER_APC;
                }
            }
        }

        if (child) {
            handle_t h;

            status = t.proc->add_handle(*child, 0, true, h); // FIXME - access mask?
            if (!success(status)) {
                child->release();
                return status;
            }

            if (!t.proc->copy_to(params.h, &h, sizeof(HANDLE))) {
                t.proc->close_handle(h);
                child->release();
                return nt_status::ACCESS_VIOLATION;
            }

            {
                lock_guard lock(threads_lock);

                children_time_running += child->dead_threads_time_running + child->children_time_running;
                children_time_running_in_userspace += child->dead_threads_time_running_in_userspace + child->children_time_running_in_userspace;
            }

            child->release();

            return nt_status::SUCCESS;
        }

        if (params.no_hang)
            return nt_status::TIMEOUT;
    } while (true);
}

nt_status process::query_is_our_child(uint32_t& return_length) {
    auto& t = *get_current_thread();

    {
        lock_guard lock(t.proc->children_lock);

        for (auto le = t.proc->children.next; le != &t.proc->children; le = le->next) {
            auto& c = *(process*)((uint8_t*)le - offsetof(process, child_le));

            if (&c == this) {
                return_length = 0;
                return nt_status::SUCCESS;
            }
        }
    }

    return nt_status::NOT_FOUND;
}

nt_status process::query_session_id(span<uint8_t> buf, uint32_t& return_length) {
    if (buf.size() < sizeof(uintptr_t))
        return nt_status::BUFFER_TOO_SMALL;

    {
        lock_guard lock(pg_lock);

        *(uintptr_t*)buf.data() = pg->sess.pid;
    }

    return_length = sizeof(uintptr_t);

    return nt_status::SUCCESS;
}

nt_status process::query_process_group(span<uint8_t> buf,
                                       uint32_t& return_length) {
    if (buf.size() < sizeof(uintptr_t))
        return nt_status::BUFFER_TOO_SMALL;

    {
        lock_guard lock(pg_lock);

        *(uintptr_t*)buf.data() = pg->tree.key;
    }

    return_length = sizeof(uintptr_t);

    return nt_status::SUCCESS;
}

NTSTATUS process::query_info(enum PROCESS_INFORMATION_CLASS process_information_class, span<uint8_t> buf,
                             uint32_t& return_length) {
    switch (process_information_class) {
        using enum PROCESS_INFORMATION_CLASS;

        case BasicInformation:
            return query_basic_info(buf, return_length);

        case ImageFileName:
            return query_image_file_name(buf, return_length);

        case Times:
            return query_times(buf, return_length);

        case MoxnoxFindNtdll:
            return query_find_ntdll(buf, return_length);

        case MoxnoxIsOurChild:
            return query_is_our_child(return_length);

        case MoxnoxChildrenTimes:
            return query_children_times(buf, return_length);

        case MoxnoxGetSessionId:
            return query_session_id(buf, return_length);

        case MoxnoxGetProcessGroup:
            return query_process_group(buf, return_length);

        default:
            print("process::query_info: unhandled info class {:x}\n",
                  (unsigned int)process_information_class);
            return nt_status::INVALID_INFO_CLASS;
    }
}

extern "C"
void trace_NtQueryInformationProcess(uintptr_t* args) {
    auto process_handle = (HANDLE)args[0];
    auto info_class = (PROCESS_INFORMATION_CLASS)args[1];
    auto buffer = (void*)args[2];
    auto length = (uint32_t)args[3];

    start_deferred_print();

    print("NtQueryInformationProcess: process_handle = ");
    print_handle(process_handle);
    print(", info_class = {}, buffer = {:x}, length = {}\n", info_class,
          (uintptr_t)buffer, length);

    end_deferred_print();
}

extern "C"
NTSTATUS NtQueryInformationProcess(HANDLE process_handle, enum PROCESS_INFORMATION_CLASS process_information_class,
                                   void* process_information, uint32_t process_information_length,
                                   uint32_t* return_length) {
    object_ref objrw;
    process* p;

    TRACE4("NtQueryInformationProcess", process_handle, process_information_class,
           process_information, process_information_length);

    if (process_handle == NtCurrentProcess()) {
        auto t = get_current_thread();

        if (!t->proc)
            return nt_status::INVALID_PARAMETER;

        p = t->proc;
        p->acquire();

        objrw = object_ref{*p};
    } else {
        objrw = deref_handle(process_handle);
        if (!objrw)
            return nt_status::INVALID_HANDLE;

        auto& obj = objrw.get();

        if (obj.type != object_type::process)
            return nt_status::OBJECT_TYPE_MISMATCH;

        // FIXME - check access rights

        p = static_cast<process*>(&obj);
    }

    return p->query_info(process_information_class, span((uint8_t*)process_information, process_information_length),
                         *return_length);
}

object_ref process::get_stderr() {
    PEB peb;
    RTL_USER_PROCESS_PARAMETERS params;

    if (!copy_from(&peb, this->peb, sizeof(PEB)))
        return nullopt;

    if (!copy_from(&params, peb.ProcessParameters, sizeof(RTL_USER_PROCESS_PARAMETERS)))
        return nullopt;

    return this->deref_handle(params.StandardError);
}

NTSTATUS process::create_thread(optional<reference_wrapper<handle_t>> thread_handle, ACCESS_MASK,
                                const OBJECT_ATTRIBUTES*, CLIENT_ID& client_id,
                                const CONTEXT& thread_context,
                                const INITIAL_TEB& initial_teb, bool create_suspended) {
    CONTEXT start_context;
    mn_image_info ntdll_info, img_info;

    {
        lock_guard lock(threads_lock);

        if (dying)
            return nt_status::PROCESS_IS_TERMINATING;
    }

    // FIXME - desired_access
    // FIXME - object_attributes

    auto tptr = new ::thread;
    if (!tptr)
        return nt_status::INSUFFICIENT_RESOURCES;

    auto& t = *tptr;

    t.tid = next_tid.fetch_add(1);

    t.teb = nullptr;
    size_t teb_size = sizeof(TEB);

    t.user_stack_base = initial_teb.StackAllocationBase;

    if (!success(allocate_vm((void**)&t.teb, 0, &teb_size, MEM_COMMIT | MEM_RESERVE, mapping_type::read_write))) {
        delete &t;
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    if (!populate_teb(*this, t)) {
        delete &t;
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    memcpy(&start_context, &thread_context, sizeof(CONTEXT));
#ifdef __x86_64__
    start_context.EFlags &= 0x40dd5; // whitelist: CF, PF, AF, ZF, SF, TF, DF, OF, AC
    start_context.EFlags |= 0x200; // force IF on
    start_context.SegCs = user_CS;
    start_context.SegSs = user_DS;
    start_context.SegEs = user_DS;
    start_context.SegFs = user_DS;
    start_context.SegGs = user_DS;
#endif

    if ((start_context.ContextFlags & CONTEXT_FLOATING_POINT) != CONTEXT_FLOATING_POINT) {
#ifdef __aarch64__
        memset(&start_context.V[0], 0, sizeof(start_context.V));
        start_context.Fpcr = 0;
        start_context.Fpsr = 0;
#elifdef __x86_64__
        memset(&start_context.FltSave, 0, sizeof(start_context.FltSave));

        start_context.FltSave.MxCsr = 0x1f80;
        start_context.FltSave.MxCsr_Mask = 0x2ffff;
#endif

        start_context.ContextFlags |= CONTEXT_FLOATING_POINT;
    }

#ifdef __x86_64__
    start_context.MxCsr = start_context.FltSave.MxCsr;
#endif

    auto ctx_addr = (uintptr_t)initial_teb.StackBase - sizeof(CONTEXT);

    if (!copy_to((void*)ctx_addr, &start_context, sizeof(start_context))) {
        print("process::copy_to failed\n");
        delete &t;
        return nt_status::NO_MEMORY;
    }

    ntdll_info.base = (void*)ntdll_addr;
    ntdll_info.offset_debug_info = ntdll->dwinfo.debug_info.offset;
    ntdll_info.size_debug_info = ntdll->dwinfo.debug_info.size;
    ntdll_info.offset_debug_abbrev = ntdll->dwinfo.debug_abbrev.offset;
    ntdll_info.size_debug_abbrev = ntdll->dwinfo.debug_abbrev.size;
    ntdll_info.offset_debug_rnglists = ntdll->dwinfo.debug_rnglists.offset;
    ntdll_info.size_debug_rnglists = ntdll->dwinfo.debug_rnglists.size;
    ntdll_info.offset_debug_str = ntdll->dwinfo.debug_str.offset;
    ntdll_info.size_debug_str = ntdll->dwinfo.debug_str.size;
    ntdll_info.offset_debug_addr = ntdll->dwinfo.debug_addr.offset;
    ntdll_info.size_debug_addr = ntdll->dwinfo.debug_addr.size;
    ntdll_info.offset_debug_str_offsets = ntdll->dwinfo.debug_str_offsets.offset;
    ntdll_info.size_debug_str_offsets = ntdll->dwinfo.debug_str_offsets.size;
    ntdll_info.offset_debug_line = ntdll->dwinfo.debug_line.offset;
    ntdll_info.size_debug_line = ntdll->dwinfo.debug_line.size;
    ntdll_info.offset_debug_line_str = ntdll->dwinfo.debug_line_str.offset;
    ntdll_info.size_debug_line_str = ntdll->dwinfo.debug_line_str.size;

    auto ntdll_info_addr = ctx_addr - sizeof(ntdll_info);

    if (!copy_to((void*)ntdll_info_addr, &ntdll_info, sizeof(ntdll_info))) {
        print("process::copy_to failed\n");
        delete &t;
        return nt_status::NO_MEMORY;
    }

    auto& sect = img->get();

    img_info.base = (void*)img_addr;
    img_info.offset_debug_info = sect.dwinfo.debug_info.offset;
    img_info.size_debug_info = sect.dwinfo.debug_info.size;
    img_info.offset_debug_abbrev = sect.dwinfo.debug_abbrev.offset;
    img_info.size_debug_abbrev = sect.dwinfo.debug_abbrev.size;
    img_info.offset_debug_rnglists = sect.dwinfo.debug_rnglists.offset;
    img_info.size_debug_rnglists = sect.dwinfo.debug_rnglists.size;
    img_info.offset_debug_str = sect.dwinfo.debug_str.offset;
    img_info.size_debug_str = sect.dwinfo.debug_str.size;
    img_info.offset_debug_addr = sect.dwinfo.debug_addr.offset;
    img_info.size_debug_addr = sect.dwinfo.debug_addr.size;
    img_info.offset_debug_str_offsets = sect.dwinfo.debug_str_offsets.offset;
    img_info.size_debug_str_offsets = sect.dwinfo.debug_str_offsets.size;
    img_info.offset_debug_line = sect.dwinfo.debug_line.offset;
    img_info.size_debug_line = sect.dwinfo.debug_line.size;
    img_info.offset_debug_line_str = sect.dwinfo.debug_line_str.offset;
    img_info.size_debug_line_str = sect.dwinfo.debug_line_str.size;

    auto img_info_addr = ntdll_info_addr - sizeof(img_info);

    if (!copy_to((void*)img_info_addr, &img_info, sizeof(img_info))) {
        print("process::copy_to failed\n");
        delete &t;
        return nt_status::NO_MEMORY;
    }

    memset(&t.context, 0, sizeof(CONTEXT));

#ifdef __aarch64__
    t.context.Pc = ntdll_addr + mn_start_thread;
    t.context.X0 = ntdll_info_addr;
    t.context.X1 = img_info_addr;
    t.context.X2 = ctx_addr;
    t.context.X18 = (uintptr_t)t.teb;

    t.user_stack = (uintptr_t)img_info_addr - 0x10;
#elifdef __x86_64__
    memset(&t.syscall_context, 0, sizeof(CONTEXT));
    memset(&t.xsave, 0, XSAVE_SIZE);

    t.context.MxCsr = t.context.FltSave.MxCsr = 0x1f80;
    t.context.FltSave.MxCsr_Mask = 0x2ffff;
    ((XSAVE_FORMAT*)&t.xsave)->MxCsr = t.context.MxCsr;
    ((XSAVE_FORMAT*)&t.xsave)->MxCsr_Mask = t.context.FltSave.MxCsr_Mask;

    t.context.EFlags |= 0x202; // IF, reserved flag
    t.context.SegCs = user_CS;
    t.context.SegSs = user_DS;
    t.context.SegEs = user_DS;
    t.context.SegFs = user_DS;
    t.context.SegGs = user_DS;
    t.context.Rip = ntdll_addr + mn_start_thread;
    t.context.Rcx = ntdll_info_addr;
    t.context.Rdx = img_info_addr;
    t.context.R8 = ctx_addr;
    t.context.Rsp = (uintptr_t)img_info_addr - 0x20 - sizeof(uint64_t); // home space, plus (unused) return pointer

    // align stack pointer for the sake of AVX2 / AVX512
    t.context.Rsp &= ~(uint64_t)0x3f;
    t.context.Rsp += sizeof(uint64_t);
#endif

    t.kernel_mode = false;

    t.suspend_count = create_suspended ? 1 : 0;
    t.status = thread_status::runnable;

    t.kernel_stack = allocate_stack();
    if (!t.kernel_stack) {
        delete &t;
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    t.syscall_stack = (uint8_t*)t.kernel_stack + ((KERNEL_STACK_PAGES + KERNEL_GUARD_PAGES) << PAGE_SHIFT);

#ifdef __aarch64__
    t.syscall_stack = (uint8_t*)t.syscall_stack - 0x10;
#elifdef __x86_64__
    t.syscall_stack = (uint8_t*)t.syscall_stack - 0x20; // 0x20 bytes for MS ABI
#endif

    t.proc = this;
    this->acquire();

    auto& current_thread = *get_current_thread();
    if (current_thread.proc == this) {
        lock_guard lock(signals_lock);
        t.blocked_signals = current_thread.blocked_signals;
    }

    sigset_t dispatch_signals;

    {
        lock_guard lock(signals_lock);

        auto from_proc = pending_signals & ~t.blocked_signals;
        dispatch_signals = from_proc;
        pending_signals &= ~from_proc;
    }

    if (dispatch_signals != 0) {
        for (int i = 1; i <= LAST_SIG; i++) {
            if (dispatch_signals & ((sigset_t)1 << (i - 1))) {
                t.queue_apc((PS_APC_ROUTINE)(ntdll_addr + mn_posix_signal),
                            (void*)(uintptr_t)i, nullptr, nullptr);
            }
        }
    }

    bool proc_dying = false;

    {
        lock_guard lock(threads_lock);

        if (dying)
            proc_dying = true;
        else
            insert_tail_list(this->threads, t.process_le);
    }

    if (proc_dying) {
        delete &t;
        return nt_status::PROCESS_IS_TERMINATING;
    }

    {
        sync_lock sl;
        lock_guard lock(sl);
        insert_tail_list(thread_list, t.le);
    }

    client_id.UniqueProcess = this->tree.key;
    client_id.UniqueThread = t.tid;

    // FIXME - copy initial_teb into TEB
    // FIXME - copy client_id into TEB

    if (thread_handle) {
        auto& p = *get_current_thread()->proc;

        handle_t h;

        auto status = p.add_handle(t, 0, true, h); // FIXME - access mask
        if (!success(status))
            return status;

        thread_handle->get() = h;
    }

    return nt_status::SUCCESS;
}

nt_status process::flush_vm(void** base_address, size_t* region_size) {
    auto addr = (uint64_t)*base_address;

    addr &= ~(PAGE_SIZE - 1);

    auto m = find_mapping(*this, addr);
    if (!m)
        return nt_status::NO_MEMORY;

    auto size = *region_size;

    if (size & (PAGE_SIZE - 1))
        size = (size & ~(PAGE_SIZE - 1)) + PAGE_SIZE;

    if (size == 0 || size > m->tree.key + m->size - addr)
        size = m->tree.key + m->size - addr;

    auto ret = m->flush(addr - m->tree.key, size);
    if (!success(ret)) {
        m->release();
        return ret;
    }

    *base_address = (void*)addr;
    *region_size = size;

    m->release();

    return ret;
}

extern "C"
void trace_NtFlushVirtualMemory(uintptr_t* args) {
    auto process_handle = (HANDLE)args[0];
    auto base_address = (void**)args[1];
    auto region_size = (size_t*)args[2];

    start_deferred_print();

    print("NtFlushVirtualMemory: process_handle = ");
    print_handle(process_handle);
    print(", base_address = {:x}, region_size = {:x}\n",
          (uintptr_t)*base_address, *region_size);

    end_deferred_print();
}

extern "C"
NTSTATUS NtFlushVirtualMemory(HANDLE proc_handle, void** base_address,
                              size_t* region_size, IO_STATUS_BLOCK* iosb) {
    object_ref objrw;
    process* p;

    TRACE3("NtFlushVirtualMemory", proc_handle, base_address, region_size);

    if (proc_handle == NtCurrentProcess()) {
        auto t = get_current_thread();

        if (!t->proc)
            return nt_status::INVALID_PARAMETER;

        p = t->proc;
        p->acquire();

        objrw = object_ref{*p};
    } else {
        objrw = deref_handle(proc_handle);
        if (!objrw)
            return nt_status::INVALID_HANDLE;

        auto& obj = objrw.get();

        if (obj.type != object_type::process)
            return nt_status::OBJECT_TYPE_MISMATCH;

        // FIXME - check access rights

        p = static_cast<process*>(&obj);
    }

    memset(iosb, 0, sizeof(IO_STATUS_BLOCK)); // FIXME - what's supposed to go here?

    return p->flush_vm(base_address, region_size);
}

nt_status process::get_next_thread(void* pthreadptr, ACCESS_MASK /*desired_access*/,
                                   uint32_t /*handle_attributes*/, uint32_t /*flags*/,
                                   HANDLE* new_thread_handle) {
    auto threadptr = (::thread*)pthreadptr;
    object_ref obj;
    handle_t h;

    // FIXME - desired_access, handle_attributes, flags
    // FIXME - skips threads that we don't have access to?

    {
        lock_guard lg(threads_lock);

        ::thread* next_thread;

        if (!threadptr) {
            if (is_list_empty(threads))
                return nt_status::NO_MORE_ENTRIES;

            next_thread = (::thread*)((uint8_t*)threads.next - offsetof(::thread, process_le));
        } else {
            if (threadptr->process_le.next == &threads)
                return nt_status::NO_MORE_ENTRIES;

            next_thread = (::thread*)((uint8_t*)threadptr->process_le.next - offsetof(::thread, process_le));
        }

        next_thread->acquire();
        obj = object_ref{*next_thread};
    }

    auto p = get_current_thread()->proc;

    auto status = p->add_handle(obj.get(), 0, true, h); // FIXME - access mask
    if (!success(status))
        return status;

    *new_thread_handle = h;

    return nt_status::SUCCESS;
}

extern "C"
void trace_NtGetNextThread(uintptr_t* args) {
    auto process_handle = (HANDLE)args[0];
    auto thread_handle = (HANDLE)args[1];
    auto desired_access = (ACCESS_MASK)args[2];

    start_deferred_print();

    print("NtGetNextThread: process_handle = ");
    print_handle(process_handle);
    print(", thread_handle = ");
    print_handle(thread_handle);
    print(", desired_access = ");
    print_thread_access_mask(desired_access);
    print("\n");

    end_deferred_print();
}

extern "C"
NTSTATUS NtGetNextThread(HANDLE process_handle, HANDLE thread_handle,
                         ACCESS_MASK desired_access, uint32_t handle_attributes,
                         uint32_t flags, HANDLE* new_thread_handle) {
    object_ref proc, threadobj;

    TRACE3("NtGetNextThread", process_handle, thread_handle, desired_access);

    // FIXME - check access mask in process handle

    if (process_handle == NtCurrentProcess()) {
        auto t = get_current_thread();

        if (!t->proc)
            return nt_status::INVALID_PARAMETER;

        auto p = t->proc;
        p->acquire();

        proc = object_ref{*p};
    } else {
        proc = deref_handle(process_handle);
        if (!proc)
            return nt_status::INVALID_HANDLE;

        auto& obj = proc.get();

        if (obj.type != object_type::process)
            return nt_status::OBJECT_TYPE_MISMATCH;
    }

    ::thread* threadptr;

    if (thread_handle == NtCurrentThread()) {
        threadptr = get_current_thread();

        threadptr->acquire();

        threadobj = object_ref{*threadptr};
    } else if (thread_handle != 0) {
        threadobj = deref_handle(thread_handle);
        if (!threadobj)
            return nt_status::INVALID_HANDLE;

        auto& obj = threadobj.get();

        if (obj.type != object_type::thread)
            return nt_status::OBJECT_TYPE_MISMATCH;

        threadptr = static_cast<::thread*>(&obj);
    } else
        threadptr = nullptr;

    auto& p = static_cast<process&>(proc.get());

    if (threadobj) {
        auto& t = static_cast<::thread&>(threadobj.get());

        if (t.proc != &p)
            return nt_status::INVALID_PARAMETER;

        threadptr = &t;
    }

    return p.get_next_thread(threadptr, desired_access, handle_attributes,
                             flags, new_thread_handle);
}

extern "C"
void trace_NtGetNextProcess(uintptr_t* args) {
    auto process_handle = (HANDLE)args[0];

    start_deferred_print();

    print("NtGetNextProcess: process_handle = ");
    print_handle(process_handle);
    print("\n");

    end_deferred_print();
}

extern "C"
NTSTATUS NtGetNextProcess(HANDLE process_handle, ACCESS_MASK /*desired_access*/,
                          ULONG /*handle_attributes*/, ULONG /*flags*/,
                          HANDLE* new_process_handle) {
    auto& t = *get_current_thread();
    object_ref old_proc;
    process* new_proc;

    TRACE1("NtGetNextProcess", process_handle);

    // FIXME - desired_access, handle_attributes, flags

    if (process_handle == NtCurrentProcess()) {
        auto t = get_current_thread();

        if (!t->proc)
            return nt_status::INVALID_PARAMETER;

        auto p = t->proc;
        p->acquire();

        old_proc = object_ref{*p};
    } else if (process_handle != 0) {
        old_proc = deref_handle(process_handle);
        if (!old_proc)
            return nt_status::INVALID_HANDLE;

        auto& obj = old_proc.get();

        if (obj.type != object_type::process)
            return nt_status::OBJECT_TYPE_MISMATCH;
    }

    while (true) {
        {
            lock_guard lock(process_list_lock);

            if (!old_proc) {
                auto n = get_tree_minimum(process_tree);
                if (!n)
                    return nt_status::NO_MORE_ENTRIES;

                new_proc = &rbtree_containing_record<process>(n);
            } else {
                auto& p = static_cast<process&>(old_proc.get());

                auto n = next_rbtree_node(p.tree);
                if (!n)
                    return nt_status::NO_MORE_ENTRIES;

                new_proc = &rbtree_containing_record<process>(n);
            }

            new_proc->acquire();
        }

        if (new_proc->signalled) { // process is dead, go to next one
            old_proc = object_ref{*new_proc};
            continue;
        }

        break;
    }

    handle_t h;

    auto status = t.proc->add_handle(*new_proc, 0, true, h);
    if (!success(status)) {
        print("add_handle failed\n");
        new_proc->release();
        return status;
    }

    new_proc->release();

    *new_process_handle = h;

    return nt_status::SUCCESS;
}

void print_process_access_mask(ACCESS_MASK access_mask) {
    static constexpr pair<ACCESS_MASK, string_view> values[] = {
        { PROCESS_TERMINATE, "PROCESS_TERMINATE" },
        { PROCESS_CREATE_THREAD, "PROCESS_CREATE_THREAD" },
        { PROCESS_SET_SESSIONID, "PROCESS_SET_SESSIONID" },
        { PROCESS_VM_OPERATION, "PROCESS_VM_OPERATION" },
        { PROCESS_VM_READ, "PROCESS_VM_READ" },
        { PROCESS_VM_WRITE, "PROCESS_VM_WRITE" },
        { PROCESS_DUP_HANDLE, "PROCESS_DUP_HANDLE" },
        { PROCESS_CREATE_PROCESS, "PROCESS_CREATE_PROCESS" },
        { PROCESS_SET_QUOTA, "PROCESS_SET_QUOTA" },
        { PROCESS_SET_INFORMATION, "PROCESS_SET_INFORMATION" },
        { PROCESS_QUERY_INFORMATION, "PROCESS_QUERY_INFORMATION" },
        { PROCESS_SUSPEND_RESUME, "PROCESS_SUSPEND_RESUME" },
        { PROCESS_QUERY_LIMITED_INFORMATION, "PROCESS_QUERY_LIMITED_INFORMATION" },
        { PROCESS_SET_LIMITED_INFORMATION, "PROCESS_SET_LIMITED_INFORMATION" },
        { PROCESS_RESERVED1, "PROCESS_RESERVED1" },
        { PROCESS_RESERVED2, "PROCESS_RESERVED2" },
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
        if ((access_mask & v.first) == v.first) {
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

extern "C"
void trace_NtOpenProcess(uintptr_t* args) {
    auto desired_access = (ACCESS_MASK)args[0];
    auto client_id = (const CLIENT_ID*)args[1];

    start_deferred_print();

    print("NtOpenProcess: desired_access = ");
    print_process_access_mask(desired_access);
    print(", ");

    if (client_id)
        print("client_id->UniqueProcess = {}", client_id->UniqueProcess);
    else
        print("client_id = NULL");

    print("\n");

    end_deferred_print();
}

extern "C"
NTSTATUS NtOpenProcess(HANDLE* process_handle, ACCESS_MASK desired_access,
                       const OBJECT_ATTRIBUTES* /*object_attributes*/,
                       const CLIENT_ID* client_id) {
    auto& t = *get_current_thread();
    object_ref proc;

    TRACE2("NtOpenProcess", desired_access, client_id);

    if (!client_id)
        return nt_status::INVALID_PARAMETER;

    if (client_id->UniqueThread != 0 || client_id->UniqueProcess == 0)
        return nt_status::INVALID_PARAMETER;

    {
        lock_guard lock(process_list_lock);

        auto n = find_rbtree_node(process_tree, client_id->UniqueProcess);

        if (!n || n->key != client_id->UniqueProcess)
            return nt_status::INVALID_CID;

        auto& p = rbtree_containing_record<process>(n);

        if (!p.try_acquire())
            return nt_status::INVALID_CID;

        proc = p;
    }

    handle_t h;

    auto status = t.proc->add_handle(proc.get(), desired_access, true, h);
    if (!success(status)) {
        print("add_handle failed\n");
        static_cast<process&>(proc.get()).release();
        return status;
    }

    *process_handle = h;

    return nt_status::SUCCESS;
}

extern "C"
void trace_NtClose(uintptr_t* args) {
    auto handle = (HANDLE)args[0];

    start_deferred_print();

    print("NtClose: handle = ");
    print_handle(handle);
    print("\n");

    end_deferred_print();
}

extern "C"
NTSTATUS NtClose(HANDLE handle) {
    auto t = get_current_thread();
    auto p = t->proc;

    TRACE1("NtClose", handle);

    if (!p)
        return nt_status::INVALID_PARAMETER;

    return p->close_handle(handle);
}

object_ref create_init_process() {
    auto& p = static_cast<chardev&>(console_chardev.get());

    p.acquire();

    object_ref initin;
    object_ref initout{p};

    if (console_type == console_type::other) {
        p.acquire();
        initin = object_ref{p};
    } else {
        auto status = open_object("null", initin);
        if (!success(status))
            bsod_format("Error {} opening /dev/null", status);
    }

    static const u16string_view init_cmdline_pref = u"init.exe ";

    auto init_cmdline_len = init_cmdline_pref.size() + utf8_to_utf16_len(cmdline);

    auto init_cmdline = (char16_t*)malloc(init_cmdline_len * sizeof(char16_t));
    if (!init_cmdline)
        bsod("malloc failed while creating init process");

    memcpy(init_cmdline, init_cmdline_pref.data(),
           init_cmdline_pref.size() * sizeof(char16_t));

    utf8_to_utf16(span(init_cmdline + init_cmdline_pref.size(),
                       init_cmdline_len - init_cmdline_pref.size()), cmdline);

    auto ref = create_process("/bin/init.exe", "/", u16string_view(init_cmdline, init_cmdline_len),
                              initin, initout, initout);

    free(init_cmdline);

    if (!ref)
        return object_ref{};

    assert(ref.get().type == object_type::process);

    auto& proc = static_cast<process&>(ref.get());

    object_ref vfsctl;

    auto status = open_object("vfsctl", vfsctl);
    if (!success(status))
        bsod_format("open_object returned {} for /dev/vfsctl", status);

    handle_t h;

    status = proc.add_handle(vfsctl.get(), 0, true, h);
    if (!success(status))
        bsod_format("process::add_handle returned {}", status);

    // put vfsctl handle in environment variable

    void* us_env = nullptr;
    size_t size = PAGE_SIZE;

    status = proc.allocate_vm(&us_env, 0, &size, MEM_RESERVE | MEM_COMMIT,
                              mapping_type::read_write);
    if (!success(status))
        bsod_format("process::allocate_vm returned {}", status);

    char buf[50];
    auto sp = span(buf, sizeof(buf));

    sp = format(sp, "VFSCTL={}", (unsigned int)h);
    auto sv = string_view(buf, (size_t)(sp.data() - buf));

    auto u16len = utf8_to_utf16_len(sv);
    uint64_t buf2len = (u16len + 1 + 1) * sizeof(char16_t);
    auto buf2 = (char16_t*)malloc(buf2len);
    if (!buf2)
        bsod_format("create_init_process: malloc failed");

    utf8_to_utf16(span(buf2, u16len), sv);
    buf2[u16len] = 0;
    buf2[u16len + 1] = 0;

    status = proc.write_vm(us_env, buf2, (uint32_t)buf2len, nullptr);
    if (!success(status))
        bsod_format("process::write_vm returned {}", status);

    free(buf2);

    void* us_proc_params;

    status = proc.read_vm((uint8_t*)proc.peb + offsetof(PEB, ProcessParameters),
                          &us_proc_params, sizeof(us_proc_params), nullptr);
    if (!success(status))
        bsod_format("process:read_vm returned {}", status);

    status = proc.write_vm((uint8_t*)us_proc_params + offsetof(RTL_USER_PROCESS_PARAMETERS, Environment),
                           &us_env, sizeof(us_env), nullptr);
    if (!success(status))
        bsod_format("process:write_vm returned {}", status);

    status = proc.write_vm((uint8_t*)us_proc_params + offsetof(RTL_USER_PROCESS_PARAMETERS, EnvironmentSize),
                           &buf2len, sizeof(buf2len), nullptr);
    if (!success(status))
        bsod_format("process:write_vm returned {}", status);

    return ref;
}

extern "C"
void trace_NtSetInformationProcess(uintptr_t* args) {
    auto process_handle = (HANDLE)args[0];
    auto info_class = (PROCESS_INFORMATION_CLASS)args[1];
    auto buffer = (const void*)args[2];
    auto length = (uint32_t)args[3];

    start_deferred_print();

    print("NtSetInformationProcess: process_handle = ");
    print_handle(process_handle);
    print(", info_class = {}, buffer = {:x}, length = {}\n", info_class,
          (uintptr_t)buffer, length);

    end_deferred_print();
}

extern "C"
NTSTATUS NtSetInformationProcess(HANDLE process_handle, PROCESS_INFORMATION_CLASS info_class,
                                 const void* buffer, uint32_t buffer_size) {
    object_ref obj;

    TRACE4("NtSetInformationProcess", process_handle, info_class, buffer,
           buffer_size);

    if (process_handle == NtCurrentProcess()) {
        auto* p = get_current_thread()->proc;

        p->acquire();

        obj = object_ref{*p};
    } else {
        obj = deref_handle(process_handle);
        if (!obj)
            return nt_status::INVALID_HANDLE;

        if (obj.get().type != object_type::process)
            return nt_status::OBJECT_TYPE_MISMATCH;

        // FIXME - check access rights
    }

    auto& p = static_cast<process&>(obj.get());

    return p.set_information(info_class, span((uint8_t*)buffer, buffer_size));
}

nt_status process::new_session() {
    auto sess2 = new session;
    if (!sess2) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    auto pg2 = new process_group(*sess2);
    if (!pg2) {
        print("new failed\n");
        sess2->release();
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    sess2->release();

    bool fail = false;
    uintptr_t pid;

    {
        lock_guard lock(process_list_lock);
        pid = tree.key;
    }

    sess2->pid = pid;
    pg2->tree.key = pid;

    {
        lock_guard lock(pg_lock);

        if (pg->tree.key == pid) // process group leader
            fail = true;
        else {
            bool already_there = false;

            {
                lock_guard lock2(pg_tree_lock);

                if (find_rbtree_node(pg_tree, pid))
                    already_there = true;
                else {
                    insert_into_rbtree(pg_tree, pg2->tree);
                    pg2->inserted = true;
                }
            }

            if (!already_there) {
                lock_guard lock2(pg->procs_lock);

                remove_entry_list(this->pg_le);

                swap(pg, pg2);

                insert_tail_list(pg->procs, this->pg_le);
            }
        }
    }

    pg2->release();

    return fail ? nt_status::INVALID_PARAMETER : nt_status::SUCCESS;
}

nt_status process::set_controlling_terminal(span<const uint8_t> buffer) {
    uintptr_t written;

    // get object

    if (buffer.size() != sizeof(HANDLE))
        return nt_status::INVALID_PARAMETER;

    auto h = *(HANDLE*)buffer.data();

    auto obj = get_current_thread()->proc->deref_handle(h);
    if (!obj)
        return nt_status::INVALID_HANDLE;

    // make sure is pty

    if (obj.get().type == object_type::file) {
        auto& f = static_cast<file&>(obj.get());

        f.ino.acquire();
        obj.reset();
        obj = f.ino;
    }

    if (obj.get().type != object_type::chardev)
        return nt_status::OBJECT_TYPE_MISMATCH;

    auto& c = static_cast<chardev&>(obj.get());

    auto status = c.ioctl(IOCTL_MOXNOX_PTY_ISATTY, span<uint8_t>(),
                          span<uint8_t>(), written);
    if (!success(status))
        return status;

    {
        lock_guard lock(process_list_lock);

        auto pid = tree.key;

        {
            lock_guard lock2(pg_lock);

            if (pg->sess.pid != pid) // not session leader
                return nt_status::INVALID_PARAMETER;

            {
                lock_guard lock3(pg->sess.lock);

                if (pg->sess.terminal)
                    return nt_status::INVALID_PARAMETER;

                c.acquire();
                pg->sess.terminal = c;
            }
        }
    }

    uintptr_t pg_id = pg->tree.key;

    status = c.ioctl(IOCTL_MOXNOX_PTY_SET_FOREGROUND_PROCESS_GROUP,
                     span((uint8_t*)&pg_id, sizeof(uintptr_t)),
                     span<uint8_t>(), written);
    if (!success(status))
        return status;

    // FIXME - check pty not another session's controlling terminal?

    return nt_status::SUCCESS;
}

nt_status process::set_process_group(span<const uint8_t> buffer) {
    if (buffer.size() != sizeof(uintptr_t))
        return nt_status::INVALID_PARAMETER;

    auto pgid = *(uintptr_t*)buffer.data();

    // FIXME - check not session leader
    // FIXME - process must be caller or child of caller

    process_group* pg2 = nullptr;

    {
        lock_guard lock(pg_tree_lock);

        auto* tree = find_rbtree_node(pg_tree, pgid);
        if (tree) {
            pg2 = &rbtree_containing_record<process_group>(tree);
            pg2->acquire();
        }
    }

    {
        lock_guard lock(pg_lock);

        if (pg2 == pg) { // no change
            pg2->release();
            return nt_status::SUCCESS;
        }
    }

    if (!pg2) { // not found
        session* sess;
        uintptr_t pid;
        bool already_there = false;

        {
            lock_guard lock(process_list_lock);
            pid = tree.key;
        }

        if (pgid != pid)
            return nt_status::ACCESS_DENIED;

        // add to new process group

        {
            lock_guard lock(pg_lock);

            sess = &pg->sess;
            sess->acquire();
        }

        pg2 = new process_group(*sess);
        if (!pg2) {
            print("new failed\n");
            sess->release();
            return nt_status::INSUFFICIENT_RESOURCES;
        }

        sess->release();

        pg2->tree.key = pid;

        {
            lock_guard lock2(pg_tree_lock);

            if (find_rbtree_node(pg_tree, pid))
                already_there = true;
            else {
                insert_into_rbtree(pg_tree, pg2->tree);
                pg2->inserted = true;
            }
        }

        if (!already_there) {
            lock_guard lock(pg_lock);
            lock_guard lock2(pg->procs_lock);

            remove_entry_list(this->pg_le);

            swap(pg, pg2);

            insert_tail_list(pg->procs, this->pg_le);
        }

        pg2->release();
    } else { // found, add to existing group
        bool fail = false;

        {
            lock_guard lock(pg_lock);

            if (&pg2->sess == &pg->sess) {
                if (pg != pg2) {
                    swap(pg, pg2);

                    auto& first_pg = pg->tree.key < pg2->tree.key ? *pg : *pg2;
                    auto& second_pg = pg->tree.key < pg2->tree.key ? *pg2 : *pg;

                    {
                        lock_guard lock2(first_pg.procs_lock);
                        lock_guard lock3(second_pg.procs_lock);

                        remove_entry_list(this->pg_le);
                        insert_tail_list(pg->procs, this->pg_le);
                    }
                }
            } else
                fail = true;
        }

        pg2->release();

        if (fail)
            return nt_status::ACCESS_DENIED;
    }

    return nt_status::SUCCESS;
}

nt_status process::mark_as_child() {
    if (!parent_process)
        return nt_status::INVALID_PARAMETER;

    {
        lock_guard lock(parent_process->children_lock);
        marked_as_child = true;
    }

    return nt_status::SUCCESS;
}

nt_status process::memory_unmap(span<const uint8_t> buf) {
    static const uintptr_t MAX_USERSPACE_ADDRESS = 0x7fffffffffff;

    if (buf.size() < sizeof(MN_MEMORY_UNMAP))
        return nt_status::BUFFER_TOO_SMALL;

    auto& params = *(MN_MEMORY_UNMAP*)buf.data();

    if (params.length == 0)
        return nt_status::INVALID_PARAMETER;

    auto addr = (uintptr_t)params.addr;
    auto length = (size_t)(bytes_to_pages(params.length) << PAGE_SHIFT);

    if (addr & (PAGE_SIZE - 1))
        return nt_status::INVALID_PARAMETER;

    if (addr > MAX_USERSPACE_ADDRESS || addr + length - 1 > MAX_USERSPACE_ADDRESS)
        return nt_status::INVALID_PARAMETER;

    {
        rangelock_req rlr(rl, addr >> PAGE_SHIFT, length >> PAGE_SHIFT, rangelock_mode::exclusive);

        while (true) {
            mapping* m2 = nullptr;

            {
                lock_guard lock(mappings_lock);

                auto n = find_rbtree_node_or_less(mappings, addr);

                if (!n)
                    n = get_tree_minimum(mappings);

                if (!n)
                    break;

                auto& m = rbtree_containing_record<mapping>(n);

                if (m.tree.key < addr + length && addr < m.tree.key + m.size) {
                    m2 = &m;
                    m.acquire();
                } else {
                    auto n2 = next_rbtree_node(*n);

                    if (n2) {
                        auto& m3 = rbtree_containing_record<mapping>(n2);

                        if (m3.tree.key < addr + length && addr < m3.tree.key + m3.size) {
                            m2 = &m3;
                            m3.acquire();
                        }
                    }
                }
            }

            if (!m2)
                break;

            auto orig_address = m2->tree.key;
            auto orig_size = m2->size;

            if (addr <= m2->tree.key && addr + length >= m2->tree.key + m2->size) { // remove whole thing
                {
                    lock_guard lock(mappings_lock);
                    remove_from_rbtree(mappings, m2->tree);
                }

                m2->release();
            } else if (addr <= m2->tree.key) { // remove start
                m2->size = (uint32_t)(m2->tree.key + m2->size - addr - length);
                m2->tree.key = addr + length;
                m2->offset += m2->tree.key - orig_address;

                unmap_proc_pages(*this, orig_address, (addr + length - orig_address) >> PAGE_SHIFT);

                m2->release();
                break;
            } else if (addr + length >= m2->tree.key + m2->size) { // remove end
                m2->size = (uint32_t)(addr - m2->tree.key);

                unmap_proc_pages(*this, addr, (orig_address + orig_size - addr) >> PAGE_SHIFT);

                length -= orig_address + orig_size - addr;
                addr = orig_address + orig_size;
            } else { // poke a hole
                auto m = new mapping(*this, m2->sect);
                if (!m) {
                    print("new failed\n");
                    return nt_status::INSUFFICIENT_RESOURCES;
                }

                m->tree.key = addr + length;
                m->size = (uint32_t)(m2->tree.key + m2->size - addr - length);
                m->offset = m->tree.key - m2->tree.key + m2->offset;

                m2->size = (uint32_t)(addr - m2->tree.key);

                {
                    lock_guard lock(mappings_lock);
                    insert_into_rbtree(mappings, m->tree);
                    m->in_address_space = true;
                }

                unmap_proc_pages(*this, addr, length >> PAGE_SHIFT);

                m2->release();
                break;
            }

            m2->release();

            if (addr + length > orig_address + orig_size) {
                length -= orig_address + orig_size - addr;
                addr = orig_address + orig_size;
            } else
                break;

            if (length == 0)
                break;
        }
    }

    return nt_status::SUCCESS;
}

nt_status process::set_alarm_timer(span<const uint8_t> buffer) {
    if (buffer.size() != sizeof(HANDLE))
        return nt_status::INVALID_PARAMETER;

    auto& h = *(HANDLE*)buffer.data();

    auto timerobj = deref_handle(h);
    if (!timerobj)
        return nt_status::INVALID_HANDLE;

    if (timerobj.get().type != object_type::timer)
        return nt_status::OBJECT_TYPE_MISMATCH;

    auto& t = static_cast<timer&>(timerobj.get());
    process* last_proc = nullptr;

    acquire();

    {
        sync_lock sl;
        lock_guard lock(sl);

        last_proc = t.timer_proc;
        t.timer_proc = this;
    }

    if (last_proc)
        last_proc->release();

    return nt_status::SUCCESS;
}

nt_status process::set_information(PROCESS_INFORMATION_CLASS info_class,
                                   span<const uint8_t> buffer) {
    switch (info_class) {
        using enum PROCESS_INFORMATION_CLASS;

        case MoxnoxSetPendingSignal:
            return set_pending_signal(buffer);

        case MoxnoxExec:
            return do_exec(buffer);

        case MoxnoxNewSession:
            return new_session();

        case MoxnoxSetControllingTerminal:
            return set_controlling_terminal(buffer);

        case MoxnoxSetProcessGroup:
            return set_process_group(buffer);

        case MoxnoxSetPendingSignalProcessGroup:
            return set_pending_signal_process_group(buffer);

        case MoxnoxWaitPid:
            return set_wait_pid(buffer);

        case MoxnoxMarkAsChild:
            return mark_as_child();

        case MoxnoxMemoryUnmap:
            return memory_unmap(buffer);

        case MoxnoxFork:
            return do_fork(buffer);

        case MoxnoxAlarmTimer:
            return set_alarm_timer(buffer);

        default:
            return nt_status::NOT_IMPLEMENTED;
    }
}

void set_pending_signal3(process& proc, int sig, object_ref& victim) {
    auto mask = (uint64_t)1 << (sig - 1);

    // holding sync_lock

    lock_guard lock(proc.signals_lock);

    for (auto le = proc.sig_waiters.next; le != &proc.sig_waiters; le = le->next) {
        auto& sw = containing_record<sig_waiter>(le);

        if (sw.set & mask) {
            auto& t = *(::thread*)sw.t;

            sw.sig = sig;
            remove_entry_list(sw.le);

            if (t.status == thread_status::waiting_no_object) {
                if (t.wait_timeout != 0)
                    remove_entry_list(t.timeout_le);

                t.status = thread_status::runnable;
            }

            return;
        }
    }

    {
        lock_guard lock3(proc.threads_lock);

        for (auto le = proc.threads.next; le != &proc.threads; le = le->next) {
            auto& t = *(::thread*)((uint8_t*)le - offsetof(::thread, process_le));

            if (!(t.blocked_signals & mask)) {
                t.acquire();
                victim = t;

                break;
            }
        }
    }

    // if no thread found, set as pending for process
    if (!victim)
        proc.pending_signals |= mask;
}

static nt_status set_pending_signal2(process& proc, int sig) {
    object_ref victim;

    if (sig == SIGKILL) {
        do_terminate(proc, (nt_status)SIGKILL);
        return nt_status::SUCCESS;
    } else if (sig == SIGSTOP) {
        proc.suspend();
        return nt_status::SUCCESS;
    }

    {
        sync_lock sl;
        lock_guard lock1(sl);

        set_pending_signal3(proc, sig, victim);
    }

    if (victim) {
        auto& t = static_cast<::thread&>(victim.get());

        return t.queue_apc((PS_APC_ROUTINE)(proc.ntdll_addr + mn_posix_signal),
                           (void*)(uintptr_t)sig, nullptr, nullptr);
    }

    return nt_status::SUCCESS;
}

nt_status process::set_pending_signal(span<const uint8_t> buffer) {
    if (buffer.size() != sizeof(int))
        return nt_status::INVALID_PARAMETER;

    auto sig = *(int*)buffer.data();

    if (sig <= 0 || sig > LAST_SIG)
        return nt_status::INVALID_PARAMETER;

    return set_pending_signal2(*this, sig);
}

nt_status send_signal_to_process_group(process_group& pg, int sig) {
    unique_ptr<process*[]> procs;
    size_t max_count = 0, count = 0;

    {
        lock_guard lock(pg.procs_lock);

        for (auto le = pg.procs.next; le != &pg.procs; le = le->next) {
            max_count++;
        }
    }

    if (max_count == 0)
        return nt_status::SUCCESS;

    procs.reset(new process*[max_count]);
    if (!procs) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    {
        lock_guard lock(pg.procs_lock);

        for (auto le = pg.procs.next; le != &pg.procs; le = le->next) {
            auto& p = *(process*)((uint8_t*)le - offsetof(process, pg_le));

            auto old = p.refcount.load();

            while (old != 0 && !p.refcount.compare_exchange_weak(old, old + 1)) {
            }

            if (old == 0)
                continue; // dying

            procs[count] = &p;
            count++;

            if (count == max_count)
                break;
        }
    }

    auto status = nt_status::SUCCESS;

    for (size_t i = 0; i < count; i++) {
        if (auto status2 = set_pending_signal2(*procs[i], sig); !success(status2))
            status = status2;

        procs[i]->release();
    }

    return status;
}

nt_status process::set_pending_signal_process_group(span<const uint8_t> buffer) {
    process_group* pg2 = nullptr;

    if (buffer.size() != sizeof(MN_PENDING_SIGNAL_PROCESS_GROUP))
        return nt_status::INVALID_PARAMETER;

    auto& param = *(MN_PENDING_SIGNAL_PROCESS_GROUP*)buffer.data();

    if (param.sig <= 0 || param.sig > LAST_SIG)
        return nt_status::INVALID_PARAMETER;

    {
        lock_guard lock(pg_tree_lock);

        auto* tree = find_rbtree_node(pg_tree, param.pgid);
        if (tree) {
            pg2 = &rbtree_containing_record<process_group>(tree);
            pg2->acquire();
        }
    }

    if (!pg2)
        return nt_status::INVALID_CID;

    auto status = send_signal_to_process_group(*pg2, param.sig);

    pg2->release();

    return status;
}

nt_status process::do_exec(span<const uint8_t> buffer) {
    auto& t = *get_current_thread();

    if (buffer.size() != sizeof(HANDLE))
        return nt_status::INVALID_PARAMETER;

    auto h = *(HANDLE*)buffer.data();

    auto threadobj = deref_handle(h);
    if (!threadobj)
        return nt_status::INVALID_HANDLE;

    if (threadobj.get().type != object_type::thread)
        return nt_status::OBJECT_TYPE_MISMATCH;

    auto& newthread = static_cast<::thread&>(threadobj.get());

    if (t.proc == newthread.proc)
        return nt_status::INVALID_PARAMETER;

    // check new thread is suspended

    if (newthread.suspend_count == 0)
        return nt_status::INVALID_PARAMETER;

    {
        lock_guard lock(process_list_lock);

        // only allow if we created new process
        if (newthread.proc->parent_process != t.proc)
            return nt_status::INVALID_PARAMETER;

        // swap PIDs

        remove_from_rbtree(process_tree, t.proc->tree);
        remove_from_rbtree(process_tree, newthread.proc->tree);

        swap(t.proc->tree.key, newthread.proc->tree.key);

        insert_into_rbtree(process_tree, t.proc->tree);
        insert_into_rbtree(process_tree, newthread.proc->tree);

        // swap parents

        {
            lock_guard lock(t.proc->children_lock);
            remove_entry_list(newthread.proc->child_le);
        }

        newthread.proc->parent_process = t.proc->parent_process;

        if (t.proc->parent_process) {
            {
                lock_guard lock(t.proc->parent_process->children_lock);
                insert_tail_list(t.proc->parent_process->children, newthread.proc->child_le);
                remove_entry_list(t.proc->child_le);
            }

            t.proc->parent_process = nullptr;
            t.proc->release();
        } else
            newthread.proc->release();

        // move children

        auto& first_proc = t.proc->tree.key < newthread.proc->tree.key ? *t.proc : *newthread.proc;
        auto& second_proc = t.proc->tree.key < newthread.proc->tree.key ? *newthread.proc : *t.proc;

        {
            lock_guard lock1(first_proc.children_lock);
            lock_guard lock2(second_proc.children_lock);

            while (!is_list_empty(t.proc->children)) {
                auto le = t.proc->children.next;
                auto& c = *(process*)((uint8_t*)le - offsetof(process, child_le));

                remove_entry_list(*le);
                insert_tail_list(newthread.proc->children, *le);

                c.parent_process = newthread.proc;
            }

            newthread.proc->marked_as_child = t.proc->marked_as_child;
            t.proc->marked_as_child = false;
        }
    }

    reassign_posix_locks(*t.proc, *newthread.proc);

    // resume new thread and terminate old one

    newthread.suspend_count.fetch_sub(1);

    do_terminate(*t.proc, nt_status::SUCCESS);

    return nt_status::SUCCESS;
}

extern "C"
void trace_NtQueryVirtualMemory(uintptr_t* args) {
    auto process_handle = (HANDLE)args[0];
    auto base_address = (const void*)args[1];
    auto info_class = (MEMORY_INFORMATION_CLASS)args[2];
    auto buffer = (void*)args[3];
    auto buffer_size = (size_t)args[4];

    start_deferred_print();

    print("NtQueryVirtualMemory: process_handle = ");
    print_handle(process_handle);
    print(", base_address = {:x}, info_class = {}, buffer = {:x}, buffer_size = {}\n",
          (uintptr_t)base_address, info_class, (uintptr_t)buffer, buffer_size);

    end_deferred_print();
}

extern "C"
nt_status NtQueryVirtualMemory(HANDLE process_handle, const void* base_address,
                               MEMORY_INFORMATION_CLASS info_class, void* buffer,
                               size_t buffer_size, size_t* return_length) {
    object_ref obj;

    TRACE5("NtQueryVirtualMemory", process_handle, base_address, info_class,
           buffer, buffer_size);

    if (process_handle == NtCurrentProcess()) {
        auto* p = get_current_thread()->proc;

        p->acquire();

        obj = object_ref{*p};
    } else {
        obj = deref_handle(process_handle);
        if (!obj)
            return nt_status::INVALID_HANDLE;

        if (obj.get().type != object_type::process)
            return nt_status::OBJECT_TYPE_MISMATCH;

        // FIXME - check access rights
    }

    auto& p = static_cast<process&>(obj.get());

    return p.query_vm(base_address, info_class, span((uint8_t*)buffer, buffer_size),
                      *return_length);
}

nt_status process::query_vm(const void* base_address, MEMORY_INFORMATION_CLASS info_class,
                            span<uint8_t> buffer, size_t& return_length) {
    switch (info_class) {
        using enum MEMORY_INFORMATION_CLASS;

        case BasicInformation:
            return query_vm_basic_info(base_address, buffer, return_length);

        default:
            return nt_status::NOT_SUPPORTED;
    }
}

static nt_status query_mapping_basic_info(uint64_t va, process& proc, mapping& m,
                                          MEMORY_BASIC_INFORMATION& mbi) {
    va &= ~(PAGE_SIZE - 1);

    mbi.AllocationBase = (void*)m.tree.key;
    mbi.AllocationProtect = 0; // FIXME - return original protection
    mbi.PartitionId = 0;
    mbi.Protect = get_page_protection(proc, va);

    uint64_t base_address = va;

    while (base_address > m.tree.key && get_page_protection(proc, base_address - PAGE_SIZE) == mbi.Protect) {
        base_address -= PAGE_SIZE;
    }

    mbi.BaseAddress = (void*)base_address;

    uint64_t end_region = va;

    while (end_region + PAGE_SIZE < m.tree.key + m.size && get_page_protection(proc, end_region + PAGE_SIZE) == mbi.Protect) {
        end_region += PAGE_SIZE;
    }

    mbi.RegionSize = end_region - base_address + PAGE_SIZE;

    mbi.State = MEM_COMMIT; // FIXME - MEM_FREE(?), MEM_RESERVE

    if (m.sect.type == section_type::image)
        mbi.Type = MEM_IMAGE;
    else if (m.sect.type == section_type::file)
        mbi.Type = MEM_MAPPED;
    else
        mbi.Type = MEM_PRIVATE;

    return nt_status::SUCCESS;
}

nt_status process::query_vm_basic_info(const void* base_address, span<uint8_t> buffer,
                                       size_t& return_length) {
    if (buffer.size() < sizeof(MEMORY_BASIC_INFORMATION))
        return nt_status::BUFFER_TOO_SMALL;

    auto& mbi = *(MEMORY_BASIC_INFORMATION*)buffer.data();

    auto* m = find_mapping(*this, (uintptr_t)base_address);

    if (!m)
        return nt_status::NO_MEMORY;

    auto ret = query_mapping_basic_info((uint64_t)base_address, *this, *m, mbi);

    if (success(ret))
        return_length = sizeof(MEMORY_BASIC_INFORMATION);

    m->release();

    return ret;
}

process_group::~process_group() {
    sess.release();
}

void process_group::release() {
    {
        lock_guard lock(pg_tree_lock);

        if (--refcount != 0)
            return;

        if (inserted)
            remove_from_rbtree(pg_tree, tree);
    }

    delete this;
}

process_group* get_process_group(uintptr_t pgid) {
    {
        lock_guard lock(pg_tree_lock);

        auto* tree = find_rbtree_node(pg_tree, pgid);
        if (tree) {
            auto pg = &rbtree_containing_record<process_group>(tree);
            pg->acquire();

            return pg;
        }
    }

    return nullptr;
}

void broadcast_sigterm() {
    auto& t = *get_current_thread();

    object_ref obj;

    {
        lock_guard lock(process_list_lock);

        auto n = get_tree_minimum(process_tree);

        while (n) {
            auto& p = rbtree_containing_record<process>(n);
            auto old = p.refcount.load();

            while (old != 0 && !p.refcount.compare_exchange_weak(old, old + 1)) { }

            if (old != 0) {
                obj = object_ref{p};
                break;
            }

            n = next_rbtree_node(p.tree);
        }

        if (!n)
            return;
    }

    while (true) {
        object_ref obj2;
        auto& proc = static_cast<process&>(obj.get());

        if (!proc.is_init && &proc != t.proc)
            set_pending_signal2(proc, SIGTERM);

        {
            lock_guard lock(process_list_lock);

            auto n = next_rbtree_node(proc.tree);

            while (n) {
                auto& p2 = rbtree_containing_record<process>(n);
                auto old = p2.refcount.load();

                while (old != 0 && !p2.refcount.compare_exchange_weak(old, old + 1)) { }

                if (old != 0) {
                    obj2 = object_ref{p2};
                    break;
                }

                n = next_rbtree_node(p2.tree);
            }

            if (!n)
                return;
        }

        swap(obj, obj2);
    }
}

void wait_for_processes_to_finish(uint64_t timeout) {
    auto& this_proc = *get_current_thread()->proc;
    size_t count = 0;

    process_shutdown_event = create_event(false);

    {
        sync_lock sl;
        lock_guard lock(sl);
        lock_guard lock2(process_list_lock);

        auto n = get_tree_minimum(process_tree);
        if (!n)
            return;

        while (n) {
            auto& p = rbtree_containing_record<process>(n);

            if (!p.is_init && &p != &this_proc && !p.signalled)
                count++;

            n = next_rbtree_node(*n);
        }

        process_shutdown_ratchet = count;
        process_shutdown_ignore = &this_proc;
        do_process_shutdown_ratchet = true;
    }

    if (count != 0)
        wait_for_object(process_shutdown_event.get(), false, timeout);
}

static nt_status make_userspace_range_writable(process& proc, uintptr_t addr,
                                               uintptr_t size) {
    while (true) {
        auto m = find_mapping(proc, addr);
        if (!m)
            return nt_status::NO_MEMORY;

        auto len = min(size, m->tree.key + m->size - addr);
        auto num_pages = (len + PAGE_SIZE - 1) >> PAGE_SHIFT;

        for (size_t i = 0; i < num_pages; i++) {
            auto va = addr + (i << PAGE_SHIFT);
            auto pte = find_pte(proc, va);

            if (!pte) { // shouldn't happen
                m->release();
                return nt_status::NO_MEMORY;
            }

            if (pte->reserved) {
                m->release();
                return nt_status::ACCESS_VIOLATION;
            }

#ifdef __aarch64__
            bool pte_valid = pte->valid;
            bool pte_writable = pte->AP == ARM64_PTE_AP_EL0RW;
#elifdef __x86_64__
            bool pte_valid = pte->p;
            bool pte_writable = pte->w == 1;
#endif

            if (pte_valid && pte_writable)
                continue;

            {
                rangelock_req rlr2(proc.rl, va >> PAGE_SHIFT, 1,
                                    rangelock_mode::exclusive);

                if (!pte_valid) {
                    if (!m->map((uint32_t)(va - m->tree.key), va)) {
                        m->release();
                        return nt_status::ACCESS_VIOLATION;
                    }

#ifdef __aarch64__
                    pte_writable = pte->AP == ARM64_PTE_AP_EL0RW;
#elifdef __x86_64__
                    pte_writable = pte->w == 1;
#endif
                }

                if (!pte_writable) {
                    bool send_ipi = false;

                    if (!make_pte_writable(va, *m, *pte, send_ipi)) {
                        m->release();
                        return nt_status::ACCESS_VIOLATION;
                    }

                    if (send_ipi)
                        send_page_tlb_ipi(proc, va, 1);
                }
            }
        }

        m->release();

        if (size == len)
            break;

        addr += len;
        size -= len;
    }

    return nt_status::SUCCESS;
}

nt_status pin_userspace_buffer_for_writing(process& proc, span<uint8_t> us_buf,
                                           optional<rangelock_req>& rlr,
                                           span<uint8_t>*& spans,
                                           size_t& num_spans) {
    nt_status status;
    auto orig_addr = (uintptr_t)us_buf.data() & ~(PAGE_SIZE - 1);
    auto orig_size = (((uintptr_t)us_buf.data() + us_buf.size() + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1)) - orig_addr;

    status = make_userspace_range_writable(proc, orig_addr, orig_size);
    if (!success(status))
        return status;

    while (true) {
        rlr.emplace(proc.rl, orig_addr >> PAGE_SHIFT, orig_size >> PAGE_SHIFT,
                    rangelock_mode::shared);

        bool again = false;
        uintptr_t last_pfn;

        num_spans = 0;

        for (size_t i = 0; i < orig_size >> PAGE_SHIFT; i++) {
            auto va = orig_addr + (i << PAGE_SHIFT);
            auto pte = find_pte(proc, va);

            if (!pte) // shouldn't happen
                return nt_status::NO_MEMORY;

            if (pte->reserved)
                return nt_status::ACCESS_VIOLATION;

#ifdef __aarch64__
            bool pte_valid = pte->valid;
            bool pte_writable = pte->AP == ARM64_PTE_AP_EL0RW;
#elifdef __x86_64__
            bool pte_valid = pte->p;
            bool pte_writable = pte->w == 1;
#endif

            // page was faulted back out while we weren't holding the lock
            if (!pte_valid || !pte_writable) {
                rlr.reset();

                auto m = find_mapping(proc, va);
                if (!m)
                    return nt_status::NO_MEMORY;

                {
                    rangelock_req rlr2(proc.rl, va >> PAGE_SHIFT, 1,
                                       rangelock_mode::exclusive);

                    if (!pte_valid) {
                        if (!m->map((uint32_t)(va - m->tree.key), va)) {
                            m->release();
                            return nt_status::ACCESS_VIOLATION;
                        }

#ifdef __aarch64__
                        pte_writable = pte->AP == ARM64_PTE_AP_EL0RW;
#elifdef __x86_64__
                        pte_writable = pte->w == 1;
#endif
                    }

                    if (!pte_writable) {
                        bool send_ipi = false;

                        if (!make_pte_writable(va, *m, *pte, send_ipi)) {
                            m->release();
                            return nt_status::ACCESS_VIOLATION;
                        }

                        if (send_ipi)
                            send_page_tlb_ipi(proc, va, 1);
                    }
                }

                m->release();

                again = true;
                break;
            }

            if (num_spans == 0 || pte->pfn != last_pfn + 1)
                num_spans++;

            last_pfn = pte->pfn;
        }

        if (again)
            continue;

        size_t size_needed = num_spans * sizeof(span<uint8_t>);

        if (size_needed > PAGE_SIZE)
            spans = (span<uint8_t>*)vmalloc(size_needed);
        else
            spans = (span<uint8_t>*)malloc(size_needed);

        if (!spans) {
            print("malloc failed\n");
            return nt_status::INSUFFICIENT_RESOURCES;
        }

        size_t idx = 0;
        for (size_t i = 0; i < orig_size >> PAGE_SHIFT; i++) {
            auto pte = find_pte(proc, orig_addr + (i << PAGE_SHIFT));

            // shouldn't have changed as we're holding the range lock
#ifdef __aarch64__
            assert(pte && pte->valid && pte->AP == ARM64_PTE_AP_EL0RW);
#elifdef __x86_64__
            assert(pte && pte->p && pte->w);
#endif

            if (i != 0 && pte->pfn == last_pfn + 1)
                spans[idx - 1] = span(spans[idx - 1].data(), spans[idx - 1].size() + PAGE_SIZE);
            else {
                spans[idx] = span((uint8_t*)phys_address(pte->pfn << PAGE_SHIFT), PAGE_SIZE);
                idx++;
            }

            last_pfn = pte->pfn;
        }

        break;
    }

    if (num_spans == 0)
        return nt_status::SUCCESS;

    // adjust start and end

    size_t shift_start = (uintptr_t)us_buf.data() & (PAGE_SIZE - 1);
    size_t shift_end = (PAGE_SIZE - (((uintptr_t)us_buf.data() + us_buf.size()) & (PAGE_SIZE - 1))) & (PAGE_SIZE - 1);

    spans[0] = span(spans[0].data() + shift_start, spans[0].size() - shift_start);
    spans[num_spans - 1] = span(spans[num_spans - 1].data(), spans[num_spans - 1].size() - shift_end);

    // still holding range lock shared

    return nt_status::SUCCESS;
}

static nt_status make_userspace_range_readable(process& proc, uintptr_t addr,
                                               uintptr_t size) {
    while (true) {
        auto m = find_mapping(proc, addr);
        if (!m)
            return nt_status::NO_MEMORY;

        auto len = min(size, m->tree.key + m->size - addr);
        auto num_pages = (len + PAGE_SIZE - 1) >> PAGE_SHIFT;

        for (size_t i = 0; i < num_pages; i++) {
            auto va = addr + (i << PAGE_SHIFT);
            auto pte = find_pte(proc, va);

            if (!pte) { // shouldn't happen
                m->release();
                return nt_status::NO_MEMORY;
            }

            if (pte->reserved) {
                m->release();
                return nt_status::ACCESS_VIOLATION;
            }

#ifdef __aarch64__
            bool pte_valid = pte->valid;
#elifdef __x86_64__
            bool pte_valid = pte->p;
#endif

            if (!pte_valid) {
                rangelock_req rlr2(proc.rl, va >> PAGE_SHIFT, 1,
                                    rangelock_mode::exclusive);

                if (!m->map((uint32_t)(va - m->tree.key), va)) {
                    m->release();
                    return nt_status::ACCESS_VIOLATION;
                }
            }
        }

        m->release();

        if (size == len)
            break;

        addr += len;
        size -= len;
    }

    return nt_status::SUCCESS;
}

nt_status pin_userspace_buffer_for_reading(process& proc, span<const uint8_t> us_buf,
                                           optional<rangelock_req>& rlr,
                                           span<const uint8_t>*& spans,
                                           size_t& num_spans) {
    nt_status status;
    auto orig_addr = (uintptr_t)us_buf.data() & ~(PAGE_SIZE - 1);
    auto orig_size = (((uintptr_t)us_buf.data() + us_buf.size() + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1)) - orig_addr;


    status = make_userspace_range_readable(proc, orig_addr, orig_size);
    if (!success(status))
        return status;

    while (true) {
        rlr.emplace(proc.rl, orig_addr >> PAGE_SHIFT, orig_size >> PAGE_SHIFT,
                    rangelock_mode::shared);

        bool again = false;
        uintptr_t last_pfn;

        num_spans = 0;

        for (size_t i = 0; i < orig_size >> PAGE_SHIFT; i++) {
            auto va = orig_addr + (i << PAGE_SHIFT);
            auto pte = find_pte(proc, va);

            if (!pte) // shouldn't happen
                return nt_status::NO_MEMORY;

            if (pte->reserved)
                return nt_status::ACCESS_VIOLATION;

#ifdef __aarch64__
            bool pte_valid = pte->valid;
#elifdef __x86_64__
            bool pte_valid = pte->p;
#endif

            if (!pte_valid) { // page was faulted back out while we weren't holding the lock
                rlr.reset();

                auto m = find_mapping(proc, va);
                if (!m)
                    return nt_status::NO_MEMORY;

                {
                    rangelock_req rlr2(proc.rl, va >> PAGE_SHIFT, 1,
                                       rangelock_mode::exclusive);

                    if (!m->map((uint32_t)(va - m->tree.key), va)) {
                        m->release();
                        return nt_status::ACCESS_VIOLATION;
                    }
                }

                m->release();

                again = true;
                break;
            }

            if (num_spans == 0 || pte->pfn != last_pfn + 1)
                num_spans++;

            last_pfn = pte->pfn;
        }

        if (again)
            continue;

        size_t size_needed = num_spans * sizeof(span<const uint8_t>);

        if (size_needed > PAGE_SIZE)
            spans = (span<const uint8_t>*)vmalloc(size_needed);
        else
            spans = (span<const uint8_t>*)malloc(size_needed);

        if (!spans) {
            print("malloc failed\n");
            return nt_status::INSUFFICIENT_RESOURCES;
        }

        size_t idx = 0;
        for (size_t i = 0; i < orig_size >> PAGE_SHIFT; i++) {
            auto pte = find_pte(proc, orig_addr + (i << PAGE_SHIFT));

            // shouldn't have changed as we're holding the range lock
#ifdef __aarch64__
            assert(pte && pte->valid);
#elifdef __x86_64__
            assert(pte && pte->p);
#endif

            if (i != 0 && pte->pfn == last_pfn + 1)
                spans[idx - 1] = span(spans[idx - 1].data(), spans[idx - 1].size() + PAGE_SIZE);
            else {
                spans[idx] = span((const uint8_t*)phys_address(pte->pfn << PAGE_SHIFT), PAGE_SIZE);
                idx++;
            }

            last_pfn = pte->pfn;
        }

        break;
    }

    if (num_spans == 0)
        return nt_status::SUCCESS;

    // adjust start and end

    size_t shift_start = (uintptr_t)us_buf.data() & (PAGE_SIZE - 1);
    size_t shift_end = (PAGE_SIZE - (((uintptr_t)us_buf.data() + us_buf.size()) & (PAGE_SIZE - 1))) & (PAGE_SIZE - 1);

    spans[0] = span(spans[0].data() + shift_start, spans[0].size() - shift_start);
    spans[num_spans - 1] = span(spans[num_spans - 1].data(), spans[num_spans - 1].size() - shift_end);

    // still holding range lock shared

    return nt_status::SUCCESS;
}

static nt_status pin_userspace_buffers_for_read_and_write(process& proc,
                                                          span<const uint8_t> src_us_buf,
                                                          span<uint8_t> dest_us_buf,
                                                          optional<rangelock_req>& rlr,
                                                          span<const uint8_t>*& src_spans,
                                                          size_t& num_src_spans,
                                                          span<uint8_t>*& dest_spans,
                                                          size_t& num_dest_spans) {
    nt_status status;
    auto orig_src_addr = (uintptr_t)src_us_buf.data() & ~(PAGE_SIZE - 1);
    auto orig_src_size = (((uintptr_t)src_us_buf.data() + src_us_buf.size() + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1)) - orig_src_addr;
    auto orig_dest_addr = (uintptr_t)dest_us_buf.data() & ~(PAGE_SIZE - 1);
    auto orig_dest_size = (((uintptr_t)dest_us_buf.data() + dest_us_buf.size() + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1)) - orig_dest_addr;

    status = make_userspace_range_readable(proc, orig_src_addr, orig_src_size);
    if (!success(status))
        return status;

    status = make_userspace_range_writable(proc, orig_dest_addr, orig_dest_size);
    if (!success(status))
        return status;

    // FIXME - this is hackish, we should be teaching the range lock to accept pairs of ranges

    uint64_t first_page = min(orig_src_addr, orig_dest_addr) >> PAGE_SHIFT;
    uint64_t num_pages = (max(orig_src_addr + orig_src_size, orig_dest_addr + orig_dest_size) >> PAGE_SHIFT) - first_page;

    while (true) {
        rlr.emplace(proc.rl, first_page, num_pages, rangelock_mode::shared);

        bool again = false;
        uintptr_t last_src_pfn, last_dest_pfn;

        num_src_spans = 0;
        num_dest_spans = 0;

        for (size_t i = 0; i < orig_src_size >> PAGE_SHIFT; i++) {
            auto va = orig_src_addr + (i << PAGE_SHIFT);
            auto pte = find_pte(proc, va);

            if (!pte) // shouldn't happen
                return nt_status::NO_MEMORY;

            if (pte->reserved)
                return nt_status::ACCESS_VIOLATION;

#ifdef __aarch64__
            bool pte_valid = pte->valid;
#elifdef __x86_64__
            bool pte_valid = pte->p;
#endif

            if (!pte_valid) { // page was faulted back out while we weren't holding the lock
                rlr.reset();

                auto m = find_mapping(proc, va);
                if (!m)
                    return nt_status::NO_MEMORY;

                {
                    rangelock_req rlr2(proc.rl, va >> PAGE_SHIFT, 1,
                                       rangelock_mode::exclusive);

                    if (!m->map((uint32_t)(va - m->tree.key), va)) {
                        m->release();
                        return nt_status::ACCESS_VIOLATION;
                    }
                }

                m->release();

                again = true;
                break;
            }

            if (num_src_spans == 0 || pte->pfn != last_src_pfn + 1)
                num_src_spans++;

            last_src_pfn = pte->pfn;
        }

        if (again)
            continue;

        for (size_t i = 0; i < orig_dest_size >> PAGE_SHIFT; i++) {
            auto va = orig_dest_addr + (i << PAGE_SHIFT);
            auto pte = find_pte(proc, va);

            if (!pte) // shouldn't happen
                return nt_status::NO_MEMORY;

            if (pte->reserved)
                return nt_status::ACCESS_VIOLATION;

#ifdef __aarch64__
            bool pte_valid = pte->valid;
            bool pte_writable = pte->AP == ARM64_PTE_AP_EL0RW;
#elifdef __x86_64__
            bool pte_valid = pte->p;
            bool pte_writable = pte->w == 1;
#endif

            // page was faulted back out while we weren't holding the lock
            if (!pte_valid || !pte_writable) {
                rlr.reset();

                auto m = find_mapping(proc, va);
                if (!m)
                    return nt_status::NO_MEMORY;

                {
                    rangelock_req rlr2(proc.rl, va >> PAGE_SHIFT, 1,
                                       rangelock_mode::exclusive);

                    if (!pte_valid) {
                        if (!m->map((uint32_t)(va - m->tree.key), va)) {
                            m->release();
                            return nt_status::ACCESS_VIOLATION;
                        }

#ifdef __aarch64__
                        pte_writable = pte->AP == ARM64_PTE_AP_EL0RW;
#elifdef __x86_64__
                        pte_writable = pte->w == 1;
#endif
                    }

                    if (!pte_writable) {
                        bool send_ipi = false;

                        if (!make_pte_writable(va, *m, *pte, send_ipi)) {
                            m->release();
                            return nt_status::ACCESS_VIOLATION;
                        }

                        if (send_ipi)
                            send_page_tlb_ipi(proc, va, 1);
                    }
                }

                m->release();

                again = true;
                break;
            }

            if (num_dest_spans == 0 || pte->pfn != last_dest_pfn + 1)
                num_dest_spans++;

            last_dest_pfn = pte->pfn;
        }

        if (again)
            continue;

        size_t src_size_needed = num_src_spans * sizeof(span<const uint8_t>);

        if (src_size_needed > PAGE_SIZE)
            src_spans = (span<const uint8_t>*)vmalloc(src_size_needed);
        else
            src_spans = (span<const uint8_t>*)malloc(src_size_needed);

        if (!src_spans) {
            print("malloc failed\n");
            return nt_status::INSUFFICIENT_RESOURCES;
        }

        size_t dest_size_needed = num_dest_spans * sizeof(span<uint8_t>);

        if (dest_size_needed > PAGE_SIZE)
            dest_spans = (span<uint8_t>*)vmalloc(dest_size_needed);
        else
            dest_spans = (span<uint8_t>*)malloc(dest_size_needed);

        if (!dest_spans) {
            print("malloc failed\n");

            if (src_size_needed > PAGE_SIZE)
                vfree(src_spans, src_size_needed);
            else
                free(src_spans);

            return nt_status::INSUFFICIENT_RESOURCES;
        }

        size_t src_idx = 0;
        for (size_t i = 0; i < orig_src_size >> PAGE_SHIFT; i++) {
            auto pte = find_pte(proc, orig_src_addr + (i << PAGE_SHIFT));

            // shouldn't have changed as we're holding the range lock
#ifdef __aarch64__
            assert(pte && pte->valid);
#elifdef __x86_64__
            assert(pte && pte->p);
#endif

            if (i != 0 && pte->pfn == last_src_pfn + 1)
                src_spans[src_idx - 1] = span(src_spans[src_idx - 1].data(), src_spans[src_idx - 1].size() + PAGE_SIZE);
            else {
                src_spans[src_idx] = span((uint8_t*)phys_address(pte->pfn << PAGE_SHIFT), PAGE_SIZE);
                src_idx++;
            }

            last_src_pfn = pte->pfn;
        }

        size_t dest_idx = 0;
        for (size_t i = 0; i < orig_dest_size >> PAGE_SHIFT; i++) {
            auto pte = find_pte(proc, orig_dest_addr + (i << PAGE_SHIFT));

            // shouldn't have changed as we're holding the range lock
#ifdef __aarch64__
            assert(pte && pte->valid && pte->AP == ARM64_PTE_AP_EL0RW);
#elifdef __x86_64__
            assert(pte && pte->p && pte->w);
#endif

            if (i != 0 && pte->pfn == last_dest_pfn + 1)
                dest_spans[dest_idx - 1] = span(dest_spans[dest_idx - 1].data(), dest_spans[dest_idx - 1].size() + PAGE_SIZE);
            else {
                dest_spans[dest_idx] = span((uint8_t*)phys_address(pte->pfn << PAGE_SHIFT), PAGE_SIZE);
                dest_idx++;
            }

            last_dest_pfn = pte->pfn;
        }

        break;
    }

    // adjust start and end

    if (num_src_spans > 0) {
        size_t shift_start = (uintptr_t)src_us_buf.data() & (PAGE_SIZE - 1);
        size_t shift_end = (PAGE_SIZE - (((uintptr_t)src_us_buf.data() + src_us_buf.size()) & (PAGE_SIZE - 1))) & (PAGE_SIZE - 1);

        src_spans[0] = span(src_spans[0].data() + shift_start, src_spans[0].size() - shift_start);
        src_spans[num_src_spans - 1] = span(src_spans[num_src_spans - 1].data(), src_spans[num_src_spans - 1].size() - shift_end);
    }

    if (num_dest_spans > 0) {
        size_t shift_start = (uintptr_t)dest_us_buf.data() & (PAGE_SIZE - 1);
        size_t shift_end = (PAGE_SIZE - (((uintptr_t)dest_us_buf.data() + dest_us_buf.size()) & (PAGE_SIZE - 1))) & (PAGE_SIZE - 1);

        dest_spans[0] = span(dest_spans[0].data() + shift_start, dest_spans[0].size() - shift_start);
        dest_spans[num_dest_spans - 1] = span(dest_spans[num_dest_spans - 1].data(), dest_spans[num_dest_spans - 1].size() - shift_end);
    }

    // still holding range lock shared

    return nt_status::SUCCESS;
}

nt_status pin_iovecs_for_writing(process& proc, span<const iovec> vecs,
                                 optional<rangelock_req>& rlr,
                                 span<uint8_t>*& spans, size_t& num_spans) {
    nt_status status;
    uintptr_t vec_start = 0xffffffffffffffff, vec_end = 0;
    bool empty = true;

    for (auto& v : vecs) {
        if (v.iov_len == 0)
            continue;

        auto addr = (uintptr_t)v.iov_base & ~(PAGE_SIZE - 1);
        auto size = (((uintptr_t)v.iov_base + v.iov_len + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1)) - addr;

        status = make_userspace_range_writable(proc, addr, size);
        if (!success(status))
            return status;

        vec_start = min(vec_start, (uintptr_t)v.iov_base);
        vec_end = max(vec_end, (uintptr_t)v.iov_base + v.iov_len);
        empty = false;
    }

    if (empty)
        return nt_status::SUCCESS;

    // FIXME - this is hackish, teach range lock to work with vectors instead?

    auto range_start = (uintptr_t)vec_start & ~(PAGE_SIZE - 1);
    auto range_size = (((uintptr_t)vec_end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1)) - range_start;

    while (true) {
        rlr.emplace(proc.rl, range_start >> PAGE_SHIFT, range_size >> PAGE_SHIFT,
                    rangelock_mode::shared);

        bool again = false;
        uint8_t* last_end;

        num_spans = 0;

        for (auto& v : vecs) {
            if (v.iov_len == 0)
                continue;

            auto addr = (uintptr_t)v.iov_base & ~(PAGE_SIZE - 1);
            auto size = (((uintptr_t)v.iov_base + v.iov_len + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1)) - addr;

            size_t shift_start = (uintptr_t)v.iov_base & (PAGE_SIZE - 1);
            size_t shift_end = (PAGE_SIZE - (((uintptr_t)v.iov_base + v.iov_len) & (PAGE_SIZE - 1))) & (PAGE_SIZE - 1);

            for (size_t i = 0; i < size >> PAGE_SHIFT; i++) {
                auto va = addr + (i << PAGE_SHIFT);
                auto pte = find_pte(proc, va);

                if (!pte) // shouldn't happen
                    return nt_status::NO_MEMORY;

                if (pte->reserved)
                    return nt_status::ACCESS_VIOLATION;

#ifdef __aarch64__
                bool pte_valid = pte->valid;
                bool pte_writable = pte->AP == ARM64_PTE_AP_EL0RW;
#elifdef __x86_64__
                bool pte_valid = pte->p;
                bool pte_writable = pte->w == 1;
#endif

                // page was faulted back out while we weren't holding the lock
                if (!pte_valid || !pte_writable) {
                    rlr.reset();

                    auto m = find_mapping(proc, va);
                    if (!m)
                        return nt_status::NO_MEMORY;

                    {
                        rangelock_req rlr2(proc.rl, va >> PAGE_SHIFT, 1,
                                           rangelock_mode::exclusive);

                        if (!pte_valid) {
                            if (!m->map((uint32_t)(va - m->tree.key), va)) {
                                m->release();
                                return nt_status::ACCESS_VIOLATION;
                            }

#ifdef __aarch64__
                            pte_writable = pte->AP == ARM64_PTE_AP_EL0RW;
#elifdef __x86_64__
                            pte_writable = pte->w == 1;
#endif
                        }

                        if (!pte_writable) {
                            bool send_ipi = false;

                            if (!make_pte_writable(va, *m, *pte, send_ipi)) {
                                m->release();
                                return nt_status::ACCESS_VIOLATION;
                            }

                            if (send_ipi)
                                send_page_tlb_ipi(proc, va, 1);
                        }
                    }

                    m->release();

                    again = true;
                    break;
                }

                auto phys_start = (uint8_t*)phys_address(pte->pfn << PAGE_SHIFT);
                size_t len = PAGE_SIZE;

                if (i == 0) {
                    phys_start += shift_start;
                    len -= shift_start;
                }

                if (i == (size >> PAGE_SHIFT) - 1)
                    len -= shift_end;

                if (num_spans == 0 || phys_start != last_end)
                    num_spans++;

                last_end = phys_start + len;
            }

            if (again)
                break;
        }

        if (again)
            continue;

        size_t size_needed = num_spans * sizeof(span<uint8_t>);

        if (size_needed > PAGE_SIZE)
            spans = (span<uint8_t>*)vmalloc(size_needed);
        else
            spans = (span<uint8_t>*)malloc(size_needed);

        if (!spans) {
            print("malloc failed\n");
            return nt_status::INSUFFICIENT_RESOURCES;
        }

        size_t idx = 0;
        for (auto& v : vecs) {
            if (v.iov_len == 0)
                continue;

            auto addr = (uintptr_t)v.iov_base & ~(PAGE_SIZE - 1);
            auto size = (((uintptr_t)v.iov_base + v.iov_len + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1)) - addr;

            size_t shift_start = (uintptr_t)v.iov_base & (PAGE_SIZE - 1);
            size_t shift_end = (PAGE_SIZE - (((uintptr_t)v.iov_base + v.iov_len) & (PAGE_SIZE - 1))) & (PAGE_SIZE - 1);

            for (size_t i = 0; i < size >> PAGE_SHIFT; i++) {
                auto pte = find_pte(proc, addr + (i << PAGE_SHIFT));

                // shouldn't have changed as we're holding the range lock
#ifdef __aarch64__
                assert(pte && pte->valid && pte->AP == ARM64_PTE_AP_EL0RW);
#elifdef __x86_64__
                assert(pte && pte->p && pte->w);
#endif

                auto phys_start = (uint8_t*)phys_address(pte->pfn << PAGE_SHIFT);
                size_t len = PAGE_SIZE;

                if (i == 0) {
                    phys_start += shift_start;
                    len -= shift_start;
                }

                if (i == (size >> PAGE_SHIFT) - 1)
                    len -= shift_end;

                if (idx != 0 && phys_start == spans[idx - 1].data() + spans[idx - 1].size())
                    spans[idx - 1] = span(spans[idx - 1].data(), spans[idx - 1].size() + len);
                else {
                    spans[idx] = span(phys_start, len);
                    idx++;
                }
            }
        }

        break;
    }

    // still holding range lock shared

    return nt_status::SUCCESS;
}

nt_status pin_iovecs_for_reading(process& proc, span<const iovec> vecs,
                                 optional<rangelock_req>& rlr,
                                 span<const uint8_t>*& spans, size_t& num_spans) {
    nt_status status;
    uintptr_t vec_start = 0xffffffffffffffff, vec_end = 0;
    bool empty = true;

    for (auto& v : vecs) {
        if (v.iov_len == 0)
            continue;

        auto addr = (uintptr_t)v.iov_base & ~(PAGE_SIZE - 1);
        auto size = (((uintptr_t)v.iov_base + v.iov_len + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1)) - addr;

        status = make_userspace_range_readable(proc, addr, size);
        if (!success(status))
            return status;

        vec_start = min(vec_start, (uintptr_t)v.iov_base);
        vec_end = max(vec_end, (uintptr_t)v.iov_base + v.iov_len);
        empty = false;
    }

    if (empty)
        return nt_status::SUCCESS;

    // FIXME - this is hackish, teach range lock to work with vectors instead?

    auto range_start = (uintptr_t)vec_start & ~(PAGE_SIZE - 1);
    auto range_size = (((uintptr_t)vec_end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1)) - range_start;

    while (true) {
        rlr.emplace(proc.rl, range_start >> PAGE_SHIFT, range_size >> PAGE_SHIFT,
                    rangelock_mode::shared);

        bool again = false;
        uint8_t* last_end;

        num_spans = 0;

        for (auto& v : vecs) {
            if (v.iov_len == 0)
                continue;

            auto addr = (uintptr_t)v.iov_base & ~(PAGE_SIZE - 1);
            auto size = (((uintptr_t)v.iov_base + v.iov_len + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1)) - addr;

            size_t shift_start = (uintptr_t)v.iov_base & (PAGE_SIZE - 1);
            size_t shift_end = (PAGE_SIZE - (((uintptr_t)v.iov_base + v.iov_len) & (PAGE_SIZE - 1))) & (PAGE_SIZE - 1);

            for (size_t i = 0; i < size >> PAGE_SHIFT; i++) {
                auto va = addr + (i << PAGE_SHIFT);
                auto pte = find_pte(proc, va);

                if (!pte) // shouldn't happen
                    return nt_status::NO_MEMORY;

                if (pte->reserved)
                    return nt_status::ACCESS_VIOLATION;

#ifdef __aarch64__
                bool pte_valid = pte->valid;
#elifdef __x86_64__
                bool pte_valid = pte->p;
#endif

                // page was faulted back out while we weren't holding the lock
                if (!pte_valid) {
                    rlr.reset();

                    auto m = find_mapping(proc, va);
                    if (!m)
                        return nt_status::NO_MEMORY;

                    {
                        rangelock_req rlr2(proc.rl, va >> PAGE_SHIFT, 1,
                                           rangelock_mode::exclusive);

                        if (!m->map((uint32_t)(va - m->tree.key), va)) {
                            m->release();
                            return nt_status::ACCESS_VIOLATION;
                        }
                    }

                    m->release();

                    again = true;
                    break;
                }

                auto phys_start = (uint8_t*)phys_address(pte->pfn << PAGE_SHIFT);
                size_t len = PAGE_SIZE;

                if (i == 0) {
                    phys_start += shift_start;
                    len -= shift_start;
                }

                if (i == (size >> PAGE_SHIFT) - 1)
                    len -= shift_end;

                if (num_spans == 0 || phys_start != last_end)
                    num_spans++;

                last_end = phys_start + len;
            }

            if (again)
                break;
        }

        if (again)
            continue;

        size_t size_needed = num_spans * sizeof(span<const uint8_t>);

        if (size_needed > PAGE_SIZE)
            spans = (span<const uint8_t>*)vmalloc(size_needed);
        else
            spans = (span<const uint8_t>*)malloc(size_needed);

        if (!spans) {
            print("malloc failed\n");
            return nt_status::INSUFFICIENT_RESOURCES;
        }

        size_t idx = 0;
        for (auto& v : vecs) {
            if (v.iov_len == 0)
                continue;

            auto addr = (uintptr_t)v.iov_base & ~(PAGE_SIZE - 1);
            auto size = (((uintptr_t)v.iov_base + v.iov_len + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1)) - addr;

            size_t shift_start = (uintptr_t)v.iov_base & (PAGE_SIZE - 1);
            size_t shift_end = (PAGE_SIZE - (((uintptr_t)v.iov_base + v.iov_len) & (PAGE_SIZE - 1))) & (PAGE_SIZE - 1);

            for (size_t i = 0; i < size >> PAGE_SHIFT; i++) {
                auto pte = find_pte(proc, addr + (i << PAGE_SHIFT));

                // shouldn't have changed as we're holding the range lock
#ifdef __aarch64__
                assert(pte && pte->valid);
#elifdef __x86_64__
                assert(pte && pte->p);
#endif

                auto phys_start = (uint8_t*)phys_address(pte->pfn << PAGE_SHIFT);
                size_t len = PAGE_SIZE;

                if (i == 0) {
                    phys_start += shift_start;
                    len -= shift_start;
                }

                if (i == (size >> PAGE_SHIFT) - 1)
                    len -= shift_end;

                if (idx != 0 && phys_start == spans[idx - 1].data() + spans[idx - 1].size())
                    spans[idx - 1] = span(spans[idx - 1].data(), spans[idx - 1].size() + len);
                else {
                    spans[idx] = span(phys_start, len);
                    idx++;
                }
            }
        }

        break;
    }

    // still holding range lock shared

    return nt_status::SUCCESS;
}

void send_tracepoint_ipi() {
    lock_guard lock(ipi_lock);

    disable_preempt();

    auto& cb = *get_cpu_block();

    tlb_va = 0;
    tlb_num_pages = 0;

    tlb_cpus_left = 1;

    for (uint8_t i = 0; i < num_cpus; i++) {
        if (i == cb.cpu_no)
            continue;

        send_tlb_ipi(i);
    }

    tlb_cpus_left--;

    while (tlb_cpus_left.load(memory_order_acquire) > 0) {
#ifdef __x86_64__
        __asm("pause");
#elifdef __aarch64__
        __asm("yield");
#endif
    }

    enable_preempt();
}

static nt_status create_forked_thread(process& proc, ::thread& orig_thread,
                                      object_ref& objrw) {
    auto tptr = new ::thread;
    if (!tptr) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    auto& t = *tptr;

    t.tid = next_tid.fetch_add(1);

    t.teb = orig_thread.teb;

    memcpy(&t.context, &orig_thread.syscall_context, sizeof(CONTEXT));
#ifdef __x86_64__
    memcpy(&t.xsave, &orig_thread.xsave, XSAVE_SIZE);
#endif

#ifdef __x86_64__
    t.context.Rax = (uint64_t)nt_status::SUCCESS;
#elifdef __aarch64__
    t.context.X0 = (uint64_t)nt_status::SUCCESS;
#endif

    t.kernel_mode = false;

    t.suspend_count = 1;
    t.status = thread_status::runnable;

    t.kernel_stack = allocate_stack();
    if (!t.kernel_stack) {
        delete &t;
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    t.syscall_stack = (uint8_t*)t.kernel_stack + ((KERNEL_STACK_PAGES + KERNEL_GUARD_PAGES) << PAGE_SHIFT);

#ifdef __aarch64__
    t.syscall_stack = (uint8_t*)t.syscall_stack - 0x10;
#elifdef __x86_64__
    t.syscall_stack = (uint8_t*)t.syscall_stack - 0x20; // 0x20 bytes for MS ABI
#endif

#ifdef __aarch64__
    t.user_stack = orig_thread.syscall_context.Sp;
#endif

    t.blocked_signals = orig_thread.blocked_signals;

    t.proc = &proc;
    proc.acquire();

    bool proc_dying = false;

    {
        lock_guard lock(proc.threads_lock);

        if (proc.dying)
            proc_dying = true;
        else
            insert_tail_list(proc.threads, t.process_le);
    }

    if (proc_dying) {
        delete &t;
        return nt_status::PROCESS_IS_TERMINATING;
    }

    objrw = object_ref{t};

    {
        sync_lock sl;
        lock_guard lock(sl);
        insert_tail_list(thread_list, t.le);
    }

    t.acquire();

    return nt_status::SUCCESS;
}

#ifdef __x86_64__
static nt_status prepare_forked_ptes2(unsigned int level,
                                      span<HARDWARE_PTE, 512> parent_sp,
                                      span<HARDWARE_PTE, 512> child_sp) {
    nt_status status;

    // FIXME - large pages

    for (size_t i = 0; i < parent_sp.size(); i++) {
        auto& parent_pte = parent_sp[i];
        auto& child_pte = child_sp[i];

        if (level > 1 && !parent_pte.p)
            continue;

        child_pte = parent_pte;

        if (level > 1) {
            auto pa = allocate_folio(0, page_state::pte);
            if (!pa)
                return nt_status::INSUFFICIENT_RESOURCES;

            memset(phys_address(pa), 0, PAGE_SIZE);

            child_pte.pfn = (pa >> PAGE_SHIFT) & PFN_MASK;
        }

        auto parent_sp2 = span<HARDWARE_PTE, 512>((HARDWARE_PTE*)phys_address(parent_pte.pfn << PAGE_SHIFT), 512);
        auto child_sp2 = span<HARDWARE_PTE, 512>((HARDWARE_PTE*)phys_address(child_pte.pfn << PAGE_SHIFT), 512);

        if (level > 1) {
            status = prepare_forked_ptes2(level - 1, parent_sp2, child_sp2);
            if (!success(status))
                return status;
        } else {
            if (parent_pte.cow && parent_pte.w) {
                parent_pte.w = 0;
                child_pte.w = 0;
            }

            if (parent_pte.p && parent_pte.owned)
                increase_page_refcount(parent_pte.pfn);
        }
    }

    return nt_status::SUCCESS;
}

static nt_status prepare_forked_ptes(process& parent, process& child) {
    nt_status status;
    auto parent_sp = span((HARDWARE_PTE*)phys_address(parent.cr3), 256);
    auto child_sp = span((HARDWARE_PTE*)phys_address(child.cr3), 256);

    for (size_t i = 0; i < parent_sp.size(); i++) {
        auto& parent_pte = parent_sp[i];
        auto& child_pte = child_sp[i];

        if (!parent_pte.p)
            continue;

        auto pa = allocate_folio(0, page_state::pte);
        if (!pa)
            return nt_status::INSUFFICIENT_RESOURCES;

        memset(phys_address(pa), 0, PAGE_SIZE);

        child_pte = parent_pte;
        child_pte.pfn = (pa >> PAGE_SHIFT) & PFN_MASK;

        auto parent_sp2 = span<HARDWARE_PTE, 512>((HARDWARE_PTE*)phys_address(parent_pte.pfn << PAGE_SHIFT), 512);
        auto child_sp2 = span<HARDWARE_PTE, 512>((HARDWARE_PTE*)phys_address(child_pte.pfn << PAGE_SHIFT), 512);

        status = prepare_forked_ptes2(3, parent_sp2, child_sp2);
        if (!success(status))
            return status;
    }

    return nt_status::SUCCESS;
}
#elifdef __aarch64__
static nt_status prepare_forked_ptes2(unsigned int level,
                                      span<ARM64_HARDWARE_PTE, 512> parent_sp,
                                      span<ARM64_HARDWARE_PTE, 512> child_sp) {
    nt_status status;

    // FIXME - large pages

    for (size_t i = 0; i < parent_sp.size(); i++) {
        auto& parent_pte = parent_sp[i];
        auto& child_pte = child_sp[i];

        if (level > 1 && !parent_pte.valid)
            continue;

        child_pte = parent_pte;

        if (level > 1) {
            auto pa = allocate_folio(0, page_state::pte);
            if (!pa)
                return nt_status::INSUFFICIENT_RESOURCES;

            memset(phys_address(pa), 0, PAGE_SIZE);

            child_pte.pfn = pa >> PAGE_SHIFT;
        }

        auto parent_sp2 = span<ARM64_HARDWARE_PTE, 512>((ARM64_HARDWARE_PTE*)phys_address(parent_pte.pfn << PAGE_SHIFT), 512);
        auto child_sp2 = span<ARM64_HARDWARE_PTE, 512>((ARM64_HARDWARE_PTE*)phys_address(child_pte.pfn << PAGE_SHIFT), 512);

        if (level > 1) {
            status = prepare_forked_ptes2(level - 1, parent_sp2, child_sp2);
            if (!success(status))
                return status;
        } else {
            if (parent_pte.cow && parent_pte.AP == ARM64_PTE_AP_EL0RW) {
                parent_pte.AP = ARM64_PTE_AP_EL0RO;
                child_pte.AP = ARM64_PTE_AP_EL0RO;
            }

            if (parent_pte.valid && parent_pte.owned)
                increase_page_refcount(parent_pte.pfn);
        }
    }

    return nt_status::SUCCESS;
}

static nt_status prepare_forked_ptes(process& parent, process& child) {
    nt_status status;
    auto parent_sp = span((ARM64_HARDWARE_PTE*)phys_address(parent.ttbr0), 512);
    auto child_sp = span((ARM64_HARDWARE_PTE*)phys_address(child.ttbr0), 512);

    for (size_t i = 0; i < parent_sp.size(); i++) {
        auto& parent_pte = parent_sp[i];
        auto& child_pte = child_sp[i];

        if (!parent_pte.valid)
            continue;

        auto pa = allocate_folio(0, page_state::pte);
        if (!pa)
            return nt_status::INSUFFICIENT_RESOURCES;

        memset(phys_address(pa), 0, PAGE_SIZE);

        child_pte = parent_pte;
        child_pte.pfn = pa >> PAGE_SHIFT;

        auto parent_sp2 = span<ARM64_HARDWARE_PTE, 512>((ARM64_HARDWARE_PTE*)phys_address(parent_pte.pfn << PAGE_SHIFT), 512);
        auto child_sp2 = span<ARM64_HARDWARE_PTE, 512>((ARM64_HARDWARE_PTE*)phys_address(child_pte.pfn << PAGE_SHIFT), 512);

        status = prepare_forked_ptes2(3, parent_sp2, child_sp2);
        if (!success(status))
            return status;
    }

    return nt_status::SUCCESS;
}
#endif

nt_status process::do_fork(span<const uint8_t> buf) {
    auto& t = *get_current_thread();
    object_ref thread_ref;
    nt_status status;

    if (buf.size() != sizeof(uintptr_t))
        return nt_status::INVALID_PARAMETER;

    if (t.proc != this)
        return nt_status::INVALID_PARAMETER;

    if (disallow_new_processes)
        return nt_status::SYSTEM_SHUTDOWN;

    // create process

    auto pptr = new process(*this->pg);
    if (!pptr) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    if (!pptr->initialized) {
        print("process::process failed\n");
        delete pptr;
        return nt_status::INTERNAL_ERROR;
    }

    auto& p = *pptr;

    object_ref objrw{p};

    p.tree.key = next_pid.fetch_add(1);

    p.ntdll_addr = this->ntdll_addr;
    p.img_addr = this->img_addr;
    p.peb = this->peb;
    p.proc_params = this->proc_params;

    p.img = this->img;
    p.img->get().acquire();

#ifdef __aarch64__
    if (!p.allocate_ttbr0()) {
        print("process::allocate_ttbr0 failed\n");
        return nt_status::INTERNAL_ERROR;
    }
#elifdef __x86_64__
    if (!p.allocate_cr3()) {
        print("process::allocate_cr3 failed\n");
        return nt_status::INTERNAL_ERROR;
    }
#endif

    p.suspend_count = 0;

    // duplicate handles into new process

    {
        lock_guard lock(handle_lock);

        auto n = get_tree_minimum(handles);

        while (n) {
            auto& ho = rbtree_containing_record<handle_obj>(n);
            auto& obj = *ho.obj;

            auto ho2 = (handle_obj*)malloc(sizeof(handle_obj));
            if (!ho2) {
                print("malloc failed\n");
                return nt_status::INSUFFICIENT_RESOURCES;
            }

            obj.acquire();

            ho2->tree.key = ho.tree.key;
            ho2->obj = &obj;
            ho2->access_mask = ho.access_mask;

            obj.handle_count++;

            insert_into_rbtree(p.handles, ho2->tree);

            n = next_rbtree_node(*n);
        }

        p.last_handle = last_handle;
    }

    // add to process list

    {
        lock_guard lock(process_list_lock);
        insert_into_rbtree(process_tree, p.tree);
    }

    p.added_to_tree = true;

    {
        lock_guard lock(process_list_lock);
        p.parent_process = this;
    }

    p.acquire(); // for children list

    {
        lock_guard lock(this->children_lock);
        insert_tail_list(this->children, p.child_le);

        p.marked_as_child = true;
    }

    // copy mappings

    {
        rangelock_req rlr(rl, 0, 0xffffffffffffffff, rangelock_mode::exclusive, true);

        {
            lock_guard lock(mappings_lock);

            auto n = get_tree_minimum(mappings);

            while (n) {
                auto& m = rbtree_containing_record<mapping>(n);

                auto m2 = new mapping(p, m.sect);
                if (!m2) {
                    print("new failed\n");
                    return nt_status::INSUFFICIENT_RESOURCES;
                }

                m2->tree.key = m.tree.key;
                m2->size = m.size;
                m2->offset = m.offset;
                m2->in_address_space = true;

                insert_into_rbtree(p.mappings, m2->tree);

                n = next_rbtree_node(*n);
            }
        }

        status = prepare_forked_ptes(*this, p);
        if (!success(status))
            return status;
    }

    send_proc_all_tlb_ipi(*this);

    // create new thread

    status = create_forked_thread(p, t, thread_ref);
    if (!success(status))
        return status;

    auto& new_thread = static_cast<::thread&>(thread_ref.get());

    // return pid

    uintptr_t pid = p.tree.key;
    auto pidptr = *(uintptr_t**)buf.data();

    if (!copy_to(pidptr, &pid, sizeof(pid))) {
        terminate_thread(new_thread, true);
        return nt_status::ACCESS_VIOLATION;
    }

    // resume new thread
    new_thread.suspend_count.fetch_sub(1);

#ifdef __aarch64__
    __asm __volatile (
        "tlbi vmalle1\n"
        "dsb ish\n"
        "isb\n"
    );
#endif

    return nt_status::SUCCESS;
}

nt_status session::send_signal(int sig) {
    process* proc = nullptr;

    {
        lock_guard lock(process_list_lock);

        auto n = find_rbtree_node(process_tree, pid);

        if (n && n->key == pid) {
            auto& p = rbtree_containing_record<process>(n);

            if (!p.try_acquire())
                return nt_status::SUCCESS;

            proc = &p;
        }
    }

    if (!proc)
        return nt_status::SUCCESS;

    auto status = set_pending_signal2(*proc, sig);

    if (!success(status)) {
        proc->release();
        return status;
    }

    proc->release();

    return nt_status::SUCCESS;
}
