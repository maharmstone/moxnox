// SPDX-License-Identifier: GPL-2.0-only
module;

#include <string_view>
#include <span>
#include <atomic>
#include <optional>
#include <signal.h>
#include <sys/uio.h>
#include "context_offsets.h"

export module process;

import object_ref;
import nt;
import list;
import rbtree;
import mapping_type;
import spinlock;
import handle;
import objects;
import rangelock;
import rwsem;
import section;
import rbtree;

#ifdef __aarch64__
import aarch64;
#elifdef __x86_64__
import x86;
#endif

using namespace std;

#ifdef __x86_64__
export constexpr uint16_t user_CS = 0x3b;
export constexpr uint16_t user_DS = 0x33;
#endif

export class session {
public:
    void acquire() {
        refcount++;
    }

    void release() {
        if (--refcount == 0)
            delete this;
    }

    nt_status send_signal(int sig);

    uintptr_t pid;
    spinlock lock;
    object_ref terminal;

private:
    atomic<unsigned int> refcount = 1;
};

export class process_group {
public:
    process_group(session& sess) : sess(sess) {
        sess.acquire();
    }

    ~process_group();

    void acquire() {
        refcount++;
    }

    void release();

    rbtree_node<uintptr_t> tree;
    bool inserted = false;
    session& sess;

    spinlock procs_lock;
    list_entry procs = { &procs, &procs };

private:
    atomic<unsigned int> refcount = 1;
};

export struct sig_waiter {
    list_entry le;
    sigset_t set;
    int sig;
    void* t;
};

export struct handle_obj {
    rbtree_node<handle_t> tree;
    object* obj;
    uint32_t access_mask;
};

export class process : public sync_object {
public:
    process(process_group& pg);
    ~process();

    void release() {
        if (--refcount == 0)
            delete this;
    }

    uintptr_t map_section(section& sect, uintptr_t address, optional<int64_t> offset,
                          enum mapping_type protect, bool cow, size_t* view_size);
#ifdef __aarch64__
    bool handle_page_fault(uintptr_t address, uint64_t esr_value);
    bool allocate_ttbr0();
#elifdef __x86_64__
    bool handle_page_fault(uintptr_t address, uint64_t error_code);
    bool allocate_cr3();
#endif
    bool copy_from(void* ks_addr, const void* us_addr, size_t size);
    bool copy_to(void* us_addr, const void* ks_addr, size_t size);
    void terminate();
    void suspend();
    void resume();
    nt_status add_handle(object& obj, uint32_t access_mask, bool allow_shared,
                        handle_t& h);
    void add_handle_no_alloc(object& obj, uint32_t access_mask, handle_t& h,
                             handle_obj* ho);
    NTSTATUS allocate_vm(void** base_address, uintptr_t zero_bits, size_t* region_size,
                         uint32_t allocation_type, enum mapping_type protect);
    NTSTATUS free_vm(void** base_address, size_t* region_size, uint32_t free_type);
    object_ref deref_handle(handle_t h);
    object_ref deref_handle(handle_t h, ACCESS_MASK& access_mask);
    NTSTATUS close_handle(handle_t h);
    NTSTATUS map_view_of_section(section& sect, void** base_address,
                                 uintptr_t zero_bits, size_t commit_size,
                                 optional<int64_t> section_offset, size_t& view_size,
                                 enum SECTION_INHERIT inherit_disposition,
                                 uint32_t allocation_type, enum mapping_type protect,
                                 bool cow);
    NTSTATUS unmap_view_of_section(void* base_address);
    NTSTATUS create_thread(optional<reference_wrapper<handle_t>> thread_handle,
                           ACCESS_MASK desired_access, const OBJECT_ATTRIBUTES* object_attributes,
                           CLIENT_ID& client_id, const CONTEXT& thread_context, const INITIAL_TEB& initial_teb,
                           bool create_suspended);
    NTSTATUS query_info(enum PROCESS_INFORMATION_CLASS process_information_class, span<uint8_t> buf,
                        uint32_t& return_length);
    NTSTATUS read_vm(const void* base_address, void* buffer, uint32_t buffer_size,
                     uint32_t* number_of_bytes_read);
    NTSTATUS write_vm(void* base_address, const void* buffer, uint32_t buffer_size,
                      uint32_t* number_of_bytes_written);
    nt_status flush_vm(void** base_address, size_t* region_size);
    object_ref get_stderr();
    nt_status get_next_thread(void* threadptr, ACCESS_MASK desired_access,
                              uint32_t handle_attributes, uint32_t flags,
                              HANDLE* new_thread_handle);
    nt_status set_information(PROCESS_INFORMATION_CLASS info_class,
                              span<const uint8_t> buffer);
    nt_status set_pending_signal(span<const uint8_t> buffer);
    nt_status set_pending_signal_process_group(span<const uint8_t> buffer);
    nt_status do_exec(span<const uint8_t> buffer);
    nt_status new_session();
    nt_status set_controlling_terminal(span<const uint8_t> buffer);
    nt_status set_process_group(span<const uint8_t> buffer);
    nt_status query_vm(const void* base_address, MEMORY_INFORMATION_CLASS info_class,
                       span<uint8_t> buffer, size_t& return_length);
    nt_status query_vm_basic_info(const void* base_address, span<uint8_t> buffer,
                                  size_t& return_length);

    optional<reference_wrapper<section>> img;

    spinlock mappings_lock;
    rbtree_node<uint64_t>* mappings = nullptr;

#ifdef __aarch64__
    uint64_t ttbr0 = 0;
#elifdef __x86_64__
    uint64_t cr3 = 0;
#endif

    spinlock threads_lock;
    list_entry threads = { &threads, &threads };
    atomic<uint64_t> dead_threads_time_running = 0;
    atomic<uint64_t> dead_threads_time_running_in_userspace = 0;
    uint64_t children_time_running = 0;
    uint64_t children_time_running_in_userspace = 0;

    enum nt_status exit_status = nt_status::PENDING;
    atomic<unsigned int> suspend_count;
    spinlock handle_lock;
    handle_t last_handle = 0;
    rbtree_node<handle_t>* handles = nullptr;
    PEB* peb = nullptr;
    RTL_USER_PROCESS_PARAMETERS* proc_params = nullptr;
    uintptr_t ntdll_addr, img_addr;
    bool initialized = false;
    rangelock rl;
    atomic<uint64_t> running_cpus = 0;
    rbtree_node<uintptr_t> tree;
    rwsem terminating_lock;
    bool added_to_tree = false;
    bool dying = false;
    spinlock signals_lock;
    uint64_t pending_signals = 0;
    spinlock children_lock;
    list_entry children = { &children, &children };
    list_entry child_le;
    process* parent_process = nullptr;
    bool marked_as_child = false;
    object_ref unreaped_children_event;
    list_entry wait_child_waiters = { &wait_child_waiters, &wait_child_waiters };
    bool is_init = false;
    list_entry sig_waiters = { &sig_waiters, &sig_waiters };

    spinlock pg_lock;
    process_group* pg;
    list_entry pg_le;

    spinlock posix_locks_lock;
    list_entry posix_locks = { &posix_locks, &posix_locks };

private:
    nt_status query_basic_info(span<uint8_t> buf, uint32_t& return_length);
    nt_status query_image_file_name(span<uint8_t> buf, uint32_t& return_length);
    nt_status query_times(span<uint8_t> buf, uint32_t& return_length);
    nt_status query_children_times(span<uint8_t> buf, uint32_t& return_length);
    nt_status query_session_id(span<uint8_t> buf, uint32_t& return_length);
    nt_status query_find_ntdll(span<uint8_t> buf, uint32_t& return_length);
    nt_status query_is_our_child(uint32_t& return_length);
    nt_status query_process_group(span<uint8_t> buf, uint32_t& return_length);
    nt_status set_wait_pid(span<const uint8_t> buf);
    nt_status mark_as_child();
    nt_status memory_unmap(span<const uint8_t> buf);
    nt_status do_fork(span<const uint8_t> buf);
    nt_status set_alarm_timer(span<const uint8_t> buf);
};

static_assert(is_sync_object<process>);

#ifdef __aarch64__
static_assert(offsetof(process, ttbr0) == PROCESS_TTBR0);
#elifdef __x86_64__
static_assert(offsetof(process, cr3) == PROCESS_CR3);
#endif

export object_ref create_process(string_view fn, string_view curdir,
                                 u16string_view cmdline, object_ref& stdinobj,
                                 object_ref& stdoutobj, object_ref& stderrobj);
export void load_ntdll();
export void create_time_page();
export void create_zero_section();
export void send_page_tlb_ipi(process& proc, uint64_t va, uint64_t num_pages);
export object_ref create_init_process();
export process_group* get_process_group(uintptr_t pgid);
export nt_status send_signal_to_process_group(process_group& pg, int sig);
export void broadcast_sigterm();
export void wait_for_processes_to_finish(uint64_t timeout);
export atomic<bool> disallow_new_processes = false;

#ifdef __aarch64__
export ARM64_HARDWARE_PTE* find_pte(process& proc, uintptr_t va);
#elifdef __x86_64__
export HARDWARE_PTE* find_pte(process& proc, uintptr_t va);
#endif

export nt_status pin_userspace_buffer_for_writing(process& proc,
                                                  span<uint8_t> us_buf,
                                                  optional<rangelock_req>& rlr,
                                                  span<uint8_t>*& spans,
                                                  size_t& num_spans);
export nt_status pin_userspace_buffer_for_reading(process& proc,
                                                  span<const uint8_t> us_buf,
                                                  optional<rangelock_req>& rlr,
                                                  span<const uint8_t>*& spans,
                                                  size_t& num_spans);
export nt_status pin_iovecs_for_writing(process& proc,
                                        span<const iovec> vecs,
                                        optional<rangelock_req>& rlr,
                                        span<uint8_t>*& spans,
                                        size_t& num_spans, uint64_t offset = 0);
export nt_status pin_iovecs_for_reading(process& proc,
                                        span<const iovec> vecs,
                                        optional<rangelock_req>& rlr,
                                        span<const uint8_t>*& spans,
                                        size_t& num_spans);
export void send_tracepoint_ipi();
export void print_protect(uint32_t protect);
export void set_pending_signal3(process& proc, int sig, object_ref& victim);
