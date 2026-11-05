// SPDX-License-Identifier: GPL-2.0-only
module;

#include <utility>
#include <stdint.h>
#include <optional>
#include <string_view>
#include <mutex>
#include <algorithm>
#include <bit>
#include <span>
#include <string.h>
#include <stddef.h>
#include "assert.h"

module xhci;

import nt;
import print;
import format;
import mem;
import config;
import interrupts;
import event;
import timer;
import module_init;
import thread;
import scheduler;
import acpi;
import bsod;
import dma;
import acpica;
import objects;

using namespace std;

static const uint16_t EVENT_RING_SIZE = 64;
static const uint16_t COMMAND_RING_SIZE = 64;
static const uint16_t ENDPOINT_RING_SIZE = 64;


struct xhci_request {
    xhci_trb_common trb;
    union {
        xhci_trb_command_completion comp;
        xhci_trb_transfer_event ev;
    };
    object_ref event;
};

struct port_status_change {
    list_entry le;
    uint8_t port_id;
};

struct xhci_psi {
    uint8_t psiv : 4;
    uint8_t psie : 2;
    uint8_t plt : 2;
    uint8_t pfd : 1;
    uint8_t reserved : 5;
    uint8_t lp : 2;
    uint16_t psim;
} __attribute((packed));

static_assert(sizeof(xhci_psi) == 0x4);

struct device_endpoint {
    device_endpoint(usb_device& dev) : queue(&dev) { }

    list_entry le;
    uint8_t num;
    bool dir;
    xhci_queue queue;
};

static bool init_xhci(const pci_device& dev);
static void found_xhci_acpi(const acpi_device& d);

static pci_class_recognizer mod_pci_class = {
    {}, pci_class::serial_bus, (uint8_t)pci_serial_bus::usb, true, init_xhci
};

static acpi_recognizer mod_pnp_hids[] = {
    { {}, "PNP0D10", found_xhci_acpi, nullptr },
    { {}, "PNP0D15", found_xhci_acpi, nullptr }
};

static module_init module_init_xhci([]() {
    insert_tail_list(pci_classes, mod_pci_class.le);

    for (unsigned int i = 0; i < sizeof(mod_pnp_hids) / sizeof(*mod_pnp_hids); i++) {
        insert_tail_list(pnp_hids, mod_pnp_hids[i].le);
    }
});

template<>
consteval bool format_spec_valid<enum TRB_CODE>(string_view fmt) {
    return fmt.empty();
}

span<char> do_format_arg(span<char> sp, string_view, enum TRB_CODE arg) {
    switch (arg) {
        case TRB_CODE::INVALID:
            return do_format_arg(sp, "", "INVALID");

        case TRB_CODE::SUCCESS:
            return do_format_arg(sp, "", "SUCCESS");

        case TRB_CODE::DATA_BUFFER_ERROR:
            return do_format_arg(sp, "", "DATA_BUFFER_ERROR");

        case TRB_CODE::BABBLE_DETECTED:
            return do_format_arg(sp, "", "BABBLE_DETECTED");

        case TRB_CODE::USB_TRANSACTION_ERROR:
            return do_format_arg(sp, "", "USB_TRANSACTION_ERROR");

        case TRB_CODE::TRB_ERROR:
            return do_format_arg(sp, "", "TRB_ERROR");

        case TRB_CODE::STALL_ERROR:
            return do_format_arg(sp, "", "STALL_ERROR");

        case TRB_CODE::RESOURCE_ERROR:
            return do_format_arg(sp, "", "RESOURCE_ERROR");

        case TRB_CODE::BANDWIDTH_ERROR:
            return do_format_arg(sp, "", "BANDWIDTH_ERROR");

        case TRB_CODE::NO_SLOTS_AVAILABLE_ERROR:
            return do_format_arg(sp, "", "NO_SLOTS_AVAILABLE_ERROR");

        case TRB_CODE::INVALID_STREAM_TYPE:
            return do_format_arg(sp, "", "INVALID_STREAM_TYPE");

        case TRB_CODE::SLOT_NOT_ENABLED:
            return do_format_arg(sp, "", "SLOT_NOT_ENABLED");

        case TRB_CODE::ENDPOINT_NOT_ENABLED:
            return do_format_arg(sp, "", "ENDPOINT_NOT_ENABLED");

        case TRB_CODE::SHORT_PACKET:
            return do_format_arg(sp, "", "SHORT_PACKET");

        case TRB_CODE::RING_UNDERRUN:
            return do_format_arg(sp, "", "RING_UNDERRUN");

        case TRB_CODE::RING_OVERRUN:
            return do_format_arg(sp, "", "RING_OVERRUN");

        case TRB_CODE::VF_EVENT_RING_FULL:
            return do_format_arg(sp, "", "VF_EVENT_RING_FULL");

        case TRB_CODE::PARAMETER_ERROR:
            return do_format_arg(sp, "", "PARAMETER_ERROR");

        case TRB_CODE::BANDWIDTH_OVERRUN:
            return do_format_arg(sp, "", "BANDWIDTH_OVERRUN");

        case TRB_CODE::CONTEXT_STATE_ERROR:
            return do_format_arg(sp, "", "CONTEXT_STATE_ERROR");

        case TRB_CODE::NO_PINT_RESPONSE:
            return do_format_arg(sp, "", "NO_PINT_RESPONSE");

        case TRB_CODE::EVENT_RING_FULL:
            return do_format_arg(sp, "", "EVENT_RING_FULL");

        case TRB_CODE::INCOMPATIBLE_DEVICE:
            return do_format_arg(sp, "", "INCOMPATIBLE_DEVICE");

        case TRB_CODE::MISSED_SERVICE:
            return do_format_arg(sp, "", "MISSED_SERVICE");

        case TRB_CODE::COMMAND_RING_STOPPED:
            return do_format_arg(sp, "", "COMMAND_RING_STOPPED");

        case TRB_CODE::COMMAND_ABORTED:
            return do_format_arg(sp, "", "COMMAND_ABORTED");

        case TRB_CODE::STOPPED:
            return do_format_arg(sp, "", "STOPPED");

        case TRB_CODE::STOPPED_LENGTH_INVALID:
            return do_format_arg(sp, "", "STOPPED_LENGTH_INVALID");

        case TRB_CODE::STOPPED_SHORT_PACKET:
            return do_format_arg(sp, "", "STOPPED_SHORT_PACKET");

        case TRB_CODE::MAX_EXIT_LATENCY_TOO_LARGE:
            return do_format_arg(sp, "", "MAX_EXIT_LATENCY_TOO_LARGE");

        case TRB_CODE::ISOCH_BUFFER_OVERRUN:
            return do_format_arg(sp, "", "ISOCH_BUFFER_OVERRUN");

        case TRB_CODE::EVENT_LOST:
            return do_format_arg(sp, "", "EVENT_LOST");

        case TRB_CODE::UNDEFINED_ERROR:
            return do_format_arg(sp, "", "UNDEFINED_ERROR");

        case TRB_CODE::INVALID_STREAM_ID:
            return do_format_arg(sp, "", "INVALID_STREAM_ID");

        case TRB_CODE::SECONDARY_BANDWIDTH_ERROR:
            return do_format_arg(sp, "", "SECONDARY_BANDWIDTH_ERROR");

        case TRB_CODE::SPLIT_TRANSACTION_ERROR:
            return do_format_arg(sp, "", "SPLIT_TRANSACTION_ERROR");

        default:
            return do_format_arg(sp, "", (uint32_t)arg);
    }
}

template<>
consteval bool format_spec_valid<enum xhci_portsc_pls>(string_view fmt) {
    return fmt.empty();
}

span<char> do_format_arg(span<char> sp, string_view, enum xhci_portsc_pls arg) {
    switch (arg) {
        using enum xhci_portsc_pls;

        case U0:
            return do_format_arg(sp, "", "U0");
        case U1:
            return do_format_arg(sp, "", "U1");
        case U2:
            return do_format_arg(sp, "", "U2");
        case U3:
            return do_format_arg(sp, "", "U3");
        case DISABLED:
            return do_format_arg(sp, "", "DISABLED");
        case RX_DETECT:
            return do_format_arg(sp, "", "RX_DETECT");
        case INACTIVE:
            return do_format_arg(sp, "", "INACTIVE");
        case POLLING:
            return do_format_arg(sp, "", "POLLING");
        case RECOVERY:
            return do_format_arg(sp, "", "RECOVERY");
        case HOT_RESET:
            return do_format_arg(sp, "", "HOT_RESET");
        case COMPILANCE_MODE:
            return do_format_arg(sp, "", "COMPILANCE_MODE");
        case TEST_MODE:
            return do_format_arg(sp, "", "TEST_MODE");
        case RESUME:
            return do_format_arg(sp, "", "RESUME");
        default:
            return do_format_arg(sp, "", (uint32_t)arg);
    }
}

template<>
consteval bool format_spec_valid<enum usb_class_code>(string_view fmt) {
    return fmt.empty();
}

span<char> do_format_arg(span<char> sp, string_view, enum usb_class_code arg) {
    switch (arg) {
        case usb_class_code::per_interface:
            return do_format_arg(sp, "", "per_interface");

        case usb_class_code::audio:
            return do_format_arg(sp, "", "audio");

        case usb_class_code::communications:
            return do_format_arg(sp, "", "communications");

        case usb_class_code::hid:
            return do_format_arg(sp, "", "hid");

        case usb_class_code::physical:
            return do_format_arg(sp, "", "physical");

        case usb_class_code::image:
            return do_format_arg(sp, "", "image");

        case usb_class_code::printer:
            return do_format_arg(sp, "", "printer");

        case usb_class_code::mass_storage:
            return do_format_arg(sp, "", "mass_storage");

        case usb_class_code::hub:
            return do_format_arg(sp, "", "hub");

        case usb_class_code::cdc_data:
            return do_format_arg(sp, "", "cdc_data");

        case usb_class_code::smart_card:
            return do_format_arg(sp, "", "smart_card");

        case usb_class_code::content_security:
            return do_format_arg(sp, "", "content_security");

        case usb_class_code::video:
            return do_format_arg(sp, "", "video");

        case usb_class_code::personal_healthcare:
            return do_format_arg(sp, "", "personal_healthcare");

        case usb_class_code::audio_video_devices:
            return do_format_arg(sp, "", "audio_video_devices");

        case usb_class_code::billboard_device:
            return do_format_arg(sp, "", "billboard_device");

        case usb_class_code::type_c_bridge:
            return do_format_arg(sp, "", "type_c_bridge");

        case usb_class_code::bulk_display_protocol_device:
            return do_format_arg(sp, "", "bulk_display_protocol_device");

        case usb_class_code::mctp_over_usb_endpoint_device:
            return do_format_arg(sp, "", "mctp_over_usb_endpoint_device");

        case usb_class_code::i3c_device:
            return do_format_arg(sp, "", "i3c_device");

        case usb_class_code::diagonstic_device:
            return do_format_arg(sp, "", "diagonstic_device");

        case usb_class_code::wireless_controller:
            return do_format_arg(sp, "", "wireless_controller");

        case usb_class_code::miscellaneous:
            return do_format_arg(sp, "", "miscellaneous");

        case usb_class_code::application_specific:
            return do_format_arg(sp, "", "application_specific");

        case usb_class_code::vendor_specific:
            return do_format_arg(sp, "", "vendor_specific");

        default:
            return do_format_arg(sp, "", (uint32_t)arg);
    }
}

static void write_volatile64(volatile uint64_t* addr, uint64_t val) {
    *(volatile uint32_t*)addr = val & 0xffffffff;
    *((volatile uint32_t*)addr + 1) = (uint32_t)(val >> 32);
}

static bool xhci_isr_func(void* ctx) {
    return ((xhci*)ctx)->isr();
}

xhci_queue::~xhci_queue() {
    if (ring)
        free(ring);

    if (reqs)
        free(reqs);
}

void xhci_queue::initialize(uint16_t size, volatile uint32_t* doorbell,
                            uint32_t doorbell_val) {
    auto size_bytes = (size + 1) * sizeof(xhci_trb_common);

    ring = (xhci_trb_common*)malloc(size_bytes);
    if (!ring) {
        print("malloc failed\n");
        return;
    }

    memset(ring, 0, size_bytes);

    reqs = (xhci_request**)malloc(sizeof(xhci_request*) * size);
    if (!reqs) {
        print("malloc failed\n");
        return;
    }

    memset(reqs, 0, sizeof(xhci_request*) * size);

    auto& cmd_link = *reinterpret_cast<xhci_trb_link*>(&ring[size]);
    cmd_link.ring_segment_pointer = va_to_pa_phys(ring);
    cmd_link.toggle_cycle = 1;
    cmd_link.trb_type = (uint16_t)xhci_trb_type::link;

    this->size = size;
    this->doorbell = doorbell;
    this->doorbell_val = doorbell_val;

    initialized = true;
}

static void copy_by_uint32(volatile void* dest, volatile void* src, size_t size) {
    auto src2 = (volatile uint32_t*)src;
    auto dest2 = (volatile uint32_t*)dest;

    while (size >= sizeof(uint32_t)) {
        *dest2 = *src2;
        src2++;
        dest2++;
        size -= sizeof(uint32_t);
    }
}

bool xhci::init_scratchpad() {
    volatile auto& cap_regs = *cap_regs_ptr;
    volatile auto& op_regs = *(xhci_op_regs*)((uint8_t*)&cap_regs + cap_regs.CAPLENGTH);

    xhci_hcsparams2 hcsparams2;

    copy_by_uint32(&hcsparams2, &cap_regs.HCSPARAMS2, sizeof(hcsparams2));

    uint16_t max_scratchpad_buffers = (uint16_t)(hcsparams2.max_scratchpad_buffers_high << 5) | hcsparams2.max_scratchpad_buffers_low;

    if (max_scratchpad_buffers == 0)
        return true;

    uint32_t op_pagesize_bits = op_regs.PAGESIZE;

    op_pagesize_bits &= 0xffff; // ignore reserved bits

    if (op_pagesize_bits == 0) {
        print("xhci::init_scratchpad: PAGESIZE was 0\n");
        return false;
    }

    uint8_t scratchpad_order = bytes_to_folio_order(1 << (countr_zero(op_pagesize_bits) + 12));

    scratchpad_array_size = max_scratchpad_buffers;

    scratchpad_array = (uint64_t*)malloc(sizeof(uint64_t) * scratchpad_array_size);
    if (!scratchpad_array) {
        print("malloc failed\n");
        return false;
    }

    for (unsigned int i = 0; i < max_scratchpad_buffers; i++) {
        auto buf = allocate_folio(scratchpad_order);
        if (!buf) {
            for (unsigned int j = 0; j < i; j++) {
                free_folio(scratchpad_array[j]);
            }

            free(scratchpad_array);
            scratchpad_array = nullptr;

            return false;
        }

        memset(phys_address(buf), 0, 1 << (PAGE_SHIFT + scratchpad_order));

        scratchpad_array[i] = buf;
    }

    return true;
}

bool xhci::init(span<volatile uint8_t> mmio) {
    cap_regs_ptr = (xhci_cap_regs*)mmio.data();
    cap_regs_max_size = mmio.size();

    volatile auto& cap_regs = *cap_regs_ptr;

    if (cap_regs.CAPLENGTH < 0x20) {
        print("init_xhci: CAPLENGTH was {}, expected at least {}\n",
              cap_regs.CAPLENGTH, 0x20);
        return false;
    }

    large_contexts = cap_regs.HCCPARAMS1 & XHCI_HCCPARAMS1_CSZ;

    xhci_hcsparams1 hcsparams1;

    copy_by_uint32(&hcsparams1, &cap_regs.HCSPARAMS1, sizeof(hcsparams1));

    num_slots = hcsparams1.max_slots;
    num_ports = hcsparams1.max_ports;

    if (num_ports == 0) {
        print("xhci::xhci: num_ports was 0\n");
        return false;
    }

    root_hub.num_ports = num_ports;

    root_hub.children = (usb_device**)malloc(sizeof(usb_device*) * num_ports);
    if (!root_hub.children) {
        print("malloc failed\n");
        return false;
    }

    memset(root_hub.children, 0, sizeof(usb_device*) * num_ports);

    if (mmio.size() < cap_regs.CAPLENGTH + offsetof(xhci_op_regs, ports) + (num_ports * sizeof(xhci_port_regs))) {
        print("init_xhci: MMIO region was {} bytes, expected at least {}\n",
              mmio.size(), cap_regs.CAPLENGTH + sizeof(xhci_op_regs) + (num_ports * sizeof(xhci_port_regs)));
        return false;
    }

    if (!init_scratchpad()) {
        print("xhci::init_scratchpad failed\n");
        return false;
    }

    volatile auto& op_regs = *(xhci_op_regs*)((uint8_t*)&cap_regs + cap_regs.CAPLENGTH);

    {
        auto tim_ref = create_timer(true);
        if (!tim_ref) {
            print("xhci::xhci: create_timer failed\n");
            return false;
        }

        auto& tim = static_cast<timer&>(tim_ref.get());

        auto status = tim.set(-ONE_MS_TICKS, nullptr, nullptr, false, 1, nullptr);
        if (!success(status)) {
            print("xhci::xhci: timer::set returned {}\n", status);
            return false;
        }

        op_regs.USBCMD &= ~XHCI_USBCMD_RS; // unset run bit
        for (unsigned int i = 0; i < 16; i++) { // specs say we should wait up to 16ms
            if (op_regs.USBSTS & 1) // halted
                break;

            wait_for_object(tim, true); // wait 1ms
        }
    }

    op_regs.USBCMD = XHCI_USBCMD_HCRST; // reset

    bool reset_done = false;

    {
        auto tim_ref = create_timer(true);
        if (!tim_ref) {
            print("xhci::xhci: create_timer failed\n");
            return false;
        }

        auto& tim = static_cast<timer&>(tim_ref.get());

        auto status = tim.set(-ONE_MS_TICKS, nullptr, nullptr, false, 1, nullptr);
        if (!success(status)) {
            print("xhci::xhci: timer::set returned {}\n", status);
            return false;
        }

        // wait for reset to complete
        for (unsigned int i = 0; i < 500; i++) {
            if (!(op_regs.USBCMD & XHCI_USBCMD_HCRST)) {
                reset_done = true;
                break;
            }

            wait_for_object(tim, true); // wait 1ms
        }
    }

    if (!reset_done) {
        print("init_xhci: timed out resetting controller\n");
        return false;
    }

    dcbaa = (uint64_t*)malloc((num_slots + 1) * sizeof(uint64_t));
    if (!dcbaa) {
        print("malloc failed\n");
        return false;
    }

    memset(dcbaa, 0, (num_slots + 1) * sizeof(uint64_t));
    dcbaa[0] = va_to_pa_phys(scratchpad_array);

    flush_cache(span((uint8_t*)dcbaa, (num_slots + 1) * sizeof(uint64_t)), false);

    write_volatile64(&op_regs.DCBAAP, va_to_pa_phys(dcbaa));

    uint32_t dboff = cap_regs.DBOFF;

    command_queue.initialize(COMMAND_RING_SIZE, (uint32_t*)(mmio.data() + dboff), 0);

    flush_cache(span((uint8_t*)command_queue.ring, (COMMAND_RING_SIZE + 1) * sizeof(xhci_trb_common)),
                false);

    write_volatile64(&op_regs.CRCR, va_to_pa_phys(command_queue.ring) | 1);

    op_regs.CONFIG = (op_regs.CONFIG & ~(uint32_t)0xff) | num_slots;

    if (mmio.size() < cap_regs.RTSOFF + offsetof(xhci_run_regs, int_regs) + sizeof(xhci_int_regs)) {
        print("init_xhci: MMIO region was {} bytes, expected at least {}\n",
              mmio.size(), cap_regs.RTSOFF + offsetof(xhci_run_regs, int_regs) + sizeof(xhci_int_regs));
        return false;
    }

    volatile auto& run_regs = *(xhci_run_regs*)((uint8_t*)&cap_regs + cap_regs.RTSOFF);

    size_t event_ring_bytes = EVENT_RING_SIZE * sizeof(xhci_trb_common);

    event_ring = (xhci_trb_common*)malloc_aligned(event_ring_bytes, 16);
    if (!event_ring) {
        print("malloc failed\n");
        return false;
    }

    memset(event_ring, 0, event_ring_bytes);

    seg_table = (xhci_event_ring_segment*)malloc_aligned(sizeof(xhci_event_ring_segment), 64);
    if (!seg_table) {
        print("malloc failed\n");
        free(event_ring);
        return false;
    }

    memset(seg_table, 0, sizeof(xhci_event_ring_segment));

    seg_table->base_address = va_to_pa_phys(event_ring);
    seg_table->size = EVENT_RING_SIZE;

    volatile auto& int_reg = run_regs.int_regs[0];

    flush_cache(span((uint8_t*)event_ring, event_ring_bytes), false);
    flush_cache(span((uint8_t*)seg_table, sizeof(xhci_event_ring_segment)), false);

    int_reg.ERSTSZ = (int_reg.ERSTSZ & 0xffff0000) | 1;
    write_volatile64(&int_reg.ERDP, va_to_pa_phys(event_ring));
    write_volatile64(&int_reg.ERSTBA, va_to_pa_phys(seg_table));

    port_status_change_event = create_event(true);

    // FIXME - stop thread if xhci object goes away(?)
    port_status_change_thread_obj = add_kernel_thread([](void* ctx) {
        ((xhci*)ctx)->port_status_change_thread();
    }, this);

    return true;
}

xhci::xhci(const pci_device& dev) {
    if (!dev.irq.has_value()) {
        if (!dev.msix && dev.msi.empty()) {
            print("init_xhci: no MSI or MSI-X capability, and no IRQ number\n");
            return;
        } else if (!msi_supported) {
            print("init_xhci: no IRQ number, and MSI disabled or not supported\n");
            return;
        }
    }

    if (dev.bar[0].size() < sizeof(xhci_cap_regs)) {
        print("init_xhci: BAR0 was {} bytes, expected at least {}\n",
              dev.bar[0].size(), sizeof(xhci_cap_regs));
        return;
    }

    init(dev.bar[0]);

    if (msi_supported && (dev.msix || !dev.msi.empty())) {
        // FIXME - free interrupt number in destructor
        auto int_num = setup_msi(dev, dev.msix, xhci_isr_func, this);
        if (int_num == 0)
            return;
    } else {
        if (!add_irq_handler(*dev.irq, true, xhci_isr_func, this)) {
            print("add_irq_handler failed\n");
            return;
        }
    }

    volatile auto& cap_regs = *cap_regs_ptr;
    volatile auto& run_regs = *(xhci_run_regs*)((uint8_t*)&cap_regs + cap_regs.RTSOFF);
    volatile auto& int_reg = run_regs.int_regs[0];
    volatile auto& op_regs = *(xhci_op_regs*)((uint8_t*)&cap_regs + cap_regs.CAPLENGTH);

    int_reg.IMAN |= XHCI_IMAN_IE;

    op_regs.USBCMD |= XHCI_USBCMD_INTE | XHCI_USBCMD_RS;

    initialized = true;
}

xhci::xhci(span<volatile uint8_t> mmio, uint32_t irq, bool level_sensitive) {
    if (mmio.size() < sizeof(xhci_cap_regs)) {
        print("xhci::xhci: MMIO was {} bytes, expected at least {}\n",
              mmio.size(), sizeof(xhci_cap_regs));
        return;
    }

    init(mmio);

    if (!add_irq_handler(irq, level_sensitive, xhci_isr_func, this)) {
        print("add_irq_handler failed\n");
        return;
    }

    volatile auto& cap_regs = *cap_regs_ptr;
    volatile auto& run_regs = *(xhci_run_regs*)((uint8_t*)&cap_regs + cap_regs.RTSOFF);
    volatile auto& int_reg = run_regs.int_regs[0];
    volatile auto& op_regs = *(xhci_op_regs*)((uint8_t*)&cap_regs + cap_regs.CAPLENGTH);

    int_reg.IMAN |= XHCI_IMAN_IE;

    op_regs.USBCMD |= XHCI_USBCMD_INTE | XHCI_USBCMD_RS;

    initialized = true;
}

xhci::~xhci() {
    if (dcbaa)
        free(dcbaa);

    if (event_ring)
        free(event_ring);

    if (seg_table)
        free(seg_table);

    if (scratchpad_array) {
        for (size_t i = 0; i < scratchpad_array_size; i++) {
            free_folio(scratchpad_array[i]);
        }
        free(scratchpad_array);
    }

    // FIXME - free IRQ handlers etc.
}

bool xhci::reset_port(uint8_t port_id) {
    volatile auto& cap_regs = *cap_regs_ptr;
    volatile auto& op_regs = *(xhci_op_regs*)((uint8_t*)&cap_regs + cap_regs.CAPLENGTH);
    volatile auto& p = *&op_regs.ports[port_id - 1];
    xhci_portsc portsc;

    portsc.value = p.PORTSC;

    {
        xhci_portsc portsc_write;

        portsc_write.value = 0;

        portsc_write.PLS = portsc.PLS;
        portsc_write.PP = portsc.PP;
        portsc_write.PIC = portsc.PIC;
        portsc_write.WCE = portsc.WCE;
        portsc_write.WDE = portsc.WDE;
        portsc_write.WOE = portsc.WOE;

        portsc_write.PR = 1;

        p.PORTSC = portsc_write.value;

#if 0
        xhci_portsc new_portsc;

        new_portsc.value = p.PORTSC;

        print("writing {:08x} to PORTSC ({:08x} -> {:08x})\n", portsc_write.value, portsc.value, new_portsc.value);
#endif
    }

    bool port_enabled = false;

    auto tim_ref = create_timer(true);
    if (!tim_ref) {
        print("xhci::reset_port: create_timer failed\n");
        return false;
    }

    auto& tim = static_cast<timer&>(tim_ref.get());

    auto status = tim.set(-ONE_MS_TICKS, nullptr, nullptr, false, 1, nullptr);
    if (!success(status)) {
        print("xhci::reset_port: timer::set returned {}\n", status);
        return false;
    }

    for (unsigned int j = 0; j < 500; j++) {
        portsc.value = p.PORTSC;

        if (portsc.PED) {
            port_enabled = true;
            break;
        }

        wait_for_object(tim, true); // wait 1ms
    }

    if (!port_enabled)
        print("xhci::reset_port: timed out waiting for port {} to be enabled\n", port_id);

    return port_enabled;
}

void xhci::init_ports() {
    volatile auto& cap_regs = *cap_regs_ptr;
    volatile auto& op_regs = *(xhci_op_regs*)((uint8_t*)&cap_regs + cap_regs.CAPLENGTH);

    for (uint8_t i = 0; i < num_ports; i++) {
        xhci_portsc portsc;

        portsc.value = op_regs.ports[i].PORTSC;

        if (!(portsc.CCS)) // ignore if no device connected
            continue;

        reset_port(i + 1);
    }
}

void xhci::isr_command_completion(xhci_trb_command_completion& comp) {
    auto ring_pa = va_to_pa_phys(command_queue.ring);

    if (comp.command_trb_pointer < ring_pa || comp.command_trb_pointer >= ring_pa + (command_queue.size * sizeof(xhci_trb_common)))
        return;

    size_t index = (size_t)(comp.command_trb_pointer - ring_pa) / sizeof(xhci_trb_common);
    auto& req = *command_queue.reqs[index];

    memcpy(&req.comp, &comp, sizeof(comp));

    set_event(static_cast<event&>(req.event.get()));
}

#if 0
static string_view psie_to_string(uint8_t psie) {
    switch (psie) {
        case 0:
            return "bits/s";

        case 1:
            return "Kb/s";

        case 2:
            return "Mb/s";

        case 3:
            return "Gb/s";

        default:
            unreachable();
    }
}
#endif

#if 0
static string_view speed_class_to_string(enum usb_speed_class sc) {
    switch (sc) {
        case usb_speed_class::unknown:
            return "unknown speed";

        case usb_speed_class::low_speed:
            return "Low Speed";

        case usb_speed_class::full_speed:
            return "Full Speed";

        case usb_speed_class::high_speed:
            return "High Speed";

        case usb_speed_class::super_speed:
            return "SuperSpeed";

        case usb_speed_class::super_speed_plus:
            return "SuperSpeed+";

        default:
            unreachable();
    }
}
#endif

uint8_t usb_device::find_root_port() {
    auto d = this;

    while (true) {
        if (!d->parent.device)
            return d->port;

        d = d->parent.device;
    }
}

uint32_t usb_device::find_route_string() {
    uint32_t ret = 0;
    auto d = this;

    while (true) {
        if (!d->parent.device)
            return ret;

        ret <<= 4;
        ret |= d->port; // FIXME - what if above 15?
        d = d->parent.device;
    }
}

pair<usb_device*, uint8_t> usb_device::find_tt() {
    auto d = this;

    while (d->parent.device) {
        if (d->speed_class < usb_speed_class::high_speed && d->parent.device->speed_class >= usb_speed_class::high_speed)
            return { d->parent.device, d->port };

        d = d->parent.device;
    }

    return { nullptr, 0 };
}

template<bool LARGE>
TRB_CODE try_address_device(xhci_input_context<LARGE>& context, uint8_t slot,
                            xhci& controller) {
    // some devices need an address_device with BSR set first

    {
        xhci_request req;

        auto& s = *bit_cast<xhci_trb_address_device*>(&req.trb);

        memset(&s, 0, sizeof(s));

        s.input_context_pointer = va_to_pa_phys(&context);
        s.trb_type = (uint16_t)xhci_trb_type::address_device_command;
        s.slot_id = slot;
        s.bsr = 1;

        controller.command_queue.submit(req, true);

        wait_for_object(req.event.get(), false);

        if ((TRB_CODE)req.comp.completion_code != TRB_CODE::SUCCESS)
            return (TRB_CODE)req.comp.completion_code;
    }

    TRB_CODE code;

    // follow Linux by waiting 200ms if address device fails, and trying again
    for (unsigned int i = 0; i < 2; i++) {
        if (i != 0)
            msleep(200);

        xhci_request req;

        auto& s = *bit_cast<xhci_trb_address_device*>(&req.trb);

        memset(&s, 0, sizeof(s));

        s.input_context_pointer = va_to_pa_phys(&context);
        s.trb_type = (uint16_t)xhci_trb_type::address_device_command;
        s.slot_id = slot;

        controller.command_queue.submit(req, true);

        wait_for_object(req.event.get(), false);

        code = (TRB_CODE)req.comp.completion_code;

        if (code == TRB_CODE::SUCCESS)
            break;
    }

    return code;
}

usb_device::usb_device(xhci& controller, usb_hub& parent, void* devctx, uint8_t slot,
                       uint8_t speed, uint8_t port, enum usb_speed_class speed_class) : controller(controller), parent(parent), devctx(devctx) {
    this->slot = slot;
    this->speed = speed;
    this->port = port;
    this->speed_class = speed_class;

#if 0
    print("root port {}, route_string {:x}, slot {}, speed {}\n",
          find_root_port(), find_route_string(), slot,
          speed_class_to_string(speed_class));
#endif

    parent.children[port - 1] = this;

    if (speed_class == usb_speed_class::unknown) {
        print("usb_device::usb_device: ignoring device with unknown speed class\n");
        return;
    }

    uint16_t max_packet_size;

    auto [tt, tt_port] = find_tt();

    auto func = [&]<bool LARGE>() -> bool {
        auto ptr = malloc(sizeof(xhci_input_context<LARGE>));
        if (!ptr) {
            print("malloc failed\n");
            return false;
        }

        auto& context = *new (ptr) xhci_input_context<LARGE>;

        input_context = &context;

        memset(&context, 0, sizeof(context));
        context.control.add_context = 3; // slot and EP0

        volatile auto& cap_regs = *controller.cap_regs_ptr;
        volatile auto dbs = (uint32_t*)((uint8_t*)&cap_regs + cap_regs.DBOFF);

        ep0_queue.initialize(ENDPOINT_RING_SIZE, &dbs[slot], 1);
        if (!ep0_queue.initialized) {
            print("xhci_queue::initialize failed\n");
            return false;
        }

        flush_cache(span((uint8_t*)ep0_queue.ring, (ENDPOINT_RING_SIZE + 1) * sizeof(xhci_trb_common)),
                    false);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
        context.slot.speed = speed;
        context.slot.root_hub_port_number = find_root_port();
        context.slot.route_string = find_route_string();
#pragma GCC diagnostic pop

        if (tt) {
            context.slot.parent_hub_slot_id = tt->slot;
            context.slot.parent_port_number = tt_port;
            context.slot.multi_tt = tt->parent.multi_tt ? 1 : 0;
        }

        // FIXME - ttt
        context.slot.context_entries = 1;

        switch (speed_class) {
            case usb_speed_class::low_speed:
                max_packet_size = 8;
                break;

            case usb_speed_class::full_speed:
            case usb_speed_class::high_speed:
                max_packet_size = 64;
                break;

            case usb_speed_class::super_speed:
            case usb_speed_class::super_speed_plus:
                max_packet_size = 512;
                break;

            default:
                unreachable();
                break;
        }

        context.ep[0].ep_type = (uint8_t)XHCI_EP_TYPE::CONTROL;
        context.ep[0].max_packet_size = max_packet_size;
        context.ep[0].tr_dequeue_pointer = va_to_pa_phys(ep0_queue.ring) | XHCI_EP_DCS;
        context.ep[0].cerr = 3;

        flush_cache(span((uint8_t*)&context, sizeof(xhci_input_context<LARGE>)),
                    false);

        if (auto code = try_address_device(context, slot, controller); code != TRB_CODE::SUCCESS) {
            print("usb_device::usb_device: controller returned error {} for address device\n", code);
            return false;
        }

        flush_cache(span((uint8_t*)&context, sizeof(xhci_input_context<LARGE>)),
                    true);

        return true;
    };

    if (controller.large_contexts) {
        if (!func.template operator()<true>())
            return;
    } else {
        if (!func.template operator()<false>())
            return;
    }

    {
        lock_guard lock(controller.devices_lock);
        insert_tail_list(controller.devices, le);
    }

    // if Full Speed, update EP0 with actual max packet size
    if (speed_class == usb_speed_class::full_speed) {
        auto buf = (uint8_t*)malloc(1024);
        if (!buf) {
            print("malloc failed\n");

            {
                lock_guard lock(controller.devices_lock);
                remove_entry_list(le);
            }

            return;
        }

        if (!get_descriptor(span(buf, 1024), usb_descriptor::DEVICE, 0)) {
            print("usb_device::get_descriptor failed\n");
            free(buf);

            {
                lock_guard lock(controller.devices_lock);
                remove_entry_list(le);
            }

            return;
        }

        auto& dd = *(usb_device_descriptor*)buf;
        uint16_t actual_max_packet_size = dd.bMaxPacketSize0;
        free(buf);

        if (actual_max_packet_size == 0) {
            print("usb_device::usb_device: device reported max packet size of 0\n");

            {
                lock_guard lock(controller.devices_lock);
                remove_entry_list(le);
            }

            return;
        }

        if (actual_max_packet_size != max_packet_size) {
            if (!change_ep0_max_packet_size(actual_max_packet_size)) {
                print("usb_device::change_ep0_max_packet_size failed\n");

                {
                    lock_guard lock(controller.devices_lock);
                    remove_entry_list(le);
                }

                return;
            }
        }
    }

    initialized = true;
}

usb_interface::~usb_interface() {
    while (!is_list_empty(endpoints)) {
        auto& e = containing_record<usb_endpoint>(endpoints.next);

        remove_entry_list(e.le);

        delete &e;
    }
}

usb_device_config::~usb_device_config() {
    while (!is_list_empty(interfaces)) {
        auto& i = containing_record<usb_interface>(interfaces.next);

        remove_entry_list(i.le);

        delete &i;
    }
}

usb_device::~usb_device() {
    if (slot != 0) {
        parent.children[port - 1] = nullptr;

        controller.disable_slot(slot);
        controller.dcbaa[slot] = 0;
    }

    if (controller.large_contexts)
        delete (xhci_device_context<true>*)devctx;
    else
        delete (xhci_device_context<false>*)devctx;

    if (input_context)
        free(input_context);

    if (initialized) {
        lock_guard lock(controller.devices_lock);
        remove_entry_list(le);
    }

    while (!is_list_empty(configs)) {
        auto& c = containing_record<usb_device_config>(configs.next);

        remove_entry_list(c.le);

        auto& cd = *(usb_configuration_descriptor*)c.data;
        auto conf_size = offsetof(usb_device_config, data) + cd.wTotalLength;

        c.~usb_device_config();

        if (conf_size < PAGE_SIZE)
            free(&c);
        else
            free_folio(phys_memory_pfn(&c) << PAGE_SHIFT);
    }

    parent.release();
}

bool usb_device::change_ep0_max_packet_size(uint16_t max_packet_size) {
    xhci_request req;

    // evaluate context command uses same TRB format as address device
    auto& s = *bit_cast<xhci_trb_address_device*>(&req.trb);

    memset(&s, 0, sizeof(s));

    auto func = [&]<bool LARGE>() {
        auto context = (xhci_input_context<LARGE>*)input_context;

        memset(&context->control, 0, sizeof(context->control));
        context->control.add_context = 3; // slot and EP0

        s.input_context_pointer = va_to_pa_phys(context);
        s.trb_type = (uint16_t)xhci_trb_type::evaluate_context_command;
        s.slot_id = slot;

        context->ep[0].max_packet_size = max_packet_size;

        flush_cache(span((uint8_t*)context, sizeof(xhci_input_context<LARGE>)),
                    false);
    };

    if (controller.large_contexts)
        func.template operator()<true>();
    else
        func.template operator()<false>();

    controller.command_queue.submit(req, true);

    wait_for_object(req.event.get(), false);

    if ((enum TRB_CODE)req.comp.completion_code != TRB_CODE::SUCCESS) {
        print("usb_device::change_ep0_max_packet_size: controller returned error {}\n", (enum TRB_CODE)req.comp.completion_code);
        return false;
    }

    return true;
}

static uint8_t find_interval(const usb_endpoint_descriptor& ep_desc,
                             enum usb_speed_class speed_class) {
    switch (ep_desc.transfer_type) {
        case USB_ENDPOINT_TRANSFER_TYPE_CONTROL:
        case USB_ENDPOINT_TRANSFER_TYPE_BULK:
            switch (speed_class) {
                case usb_speed_class::high_speed:
                    if (ep_desc.bInterval == 0)
                        return 0;
                    else
                        return (uint8_t)(bit_width(ep_desc.bInterval) - 1);

                default:
                    return 0;
            }
        break;

        case USB_ENDPOINT_TRANSFER_TYPE_INTERRUPT:
            switch (speed_class) {
                case usb_speed_class::low_speed:
                case usb_speed_class::full_speed:
                    if (ep_desc.bInterval == 0)
                        return 0;
                    else
                        return (uint8_t)(bit_width((unsigned int)(ep_desc.bInterval * 8)) - 1);

                default:
                    return (uint8_t)min(max(ep_desc.bInterval - 1, 0), 15);
            }
        break;

        case USB_ENDPOINT_TRANSFER_TYPE_ISOCHRONOUS:
            switch (speed_class) {
                case usb_speed_class::low_speed: // low-speed isochronous not allowed
                    return 0;

                case usb_speed_class::full_speed:
                    return (uint8_t)min(max(ep_desc.bInterval + 2, 3), 18);

                default:
                    return (uint8_t)min(max(ep_desc.bInterval - 1, 0), 15);
            }
        break;
    }

    return 0;
}

xhci_queue* usb_device::configure_endpoint(const usb_endpoint_descriptor& ep_desc,
                                           uint8_t num_ports, bool multi_tt,
                                           uint8_t tt_think_time) {
    auto ep_index = (uint8_t)(1 + ((ep_desc.endpoint_number - 1) * 2) + (ep_desc.direction ? 1 : 0));

    device_endpoint* dep = nullptr;
    xhci_request req;

    auto& s = *bit_cast<xhci_trb_configure_endpoint*>(&req.trb);

    memset(&s, 0, sizeof(s));

    auto func = [&]<bool LARGE>() {
        auto& context = *(xhci_input_context<LARGE>*)input_context;

        memset(&context.control, 0, sizeof(context.control));
        context.control.add_context = (uint32_t)1 | ((uint32_t)1 << (ep_index + 1)); // slot, endpoint

        s.input_context_pointer = va_to_pa_phys(&context);
        s.trb_type = (uint16_t)xhci_trb_type::configure_endpoint_command;
        s.slot_id = slot;

        {
            lock_guard lock(endpoint_mutex);

            if (dead)
                return false;

            bool new_dep = false;

            {
                lock_guard lock2(endpoint_spinlock);

                for (auto le = endpoints.next; le != &endpoints; le = le->next) {
                    auto& e = containing_record<device_endpoint>(le);

                    if (e.num == ep_desc.endpoint_number && !!e.dir == !!ep_desc.direction) {
                        dep = &e;
                        break;
                    }
                }
            }

            if (!dep) {
                dep = new device_endpoint(*this);
                if (!dep) {
                    print("new failed\n");
                    return false;
                }

                dep->num = ep_desc.endpoint_number;
                dep->dir = ep_desc.direction;

                volatile auto& cap_regs = *controller.cap_regs_ptr;
                volatile auto dbs = (uint32_t*)((uint8_t*)&cap_regs + cap_regs.DBOFF);

                dep->queue.initialize(ENDPOINT_RING_SIZE, &dbs[slot],
                                      ep_index + 1);
                if (!dep->queue.initialized) {
                    print("xhci_queue::initialize failed\n");
                    delete dep;
                    return false;
                }

                dep->queue.dir = ep_desc.direction;

                new_dep = true;
            }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
            context.slot.context_entries = (uint8_t)(ep_index + 1); // FIXME
            context.slot.ttt = tt_think_time;

            context.slot.hub = num_ports > 0 ? 1 : 0;
            context.slot.num_ports = num_ports;
            context.slot.multi_tt = multi_tt ? 1 : 0;
#pragma GCC diagnostic pop

            auto& ep = context.ep[ep_index];

            memset(&ep, 0, sizeof(ep));

            switch (ep_desc.transfer_type) {
                case USB_ENDPOINT_TRANSFER_TYPE_CONTROL:
                    ep.ep_type = (uint8_t)XHCI_EP_TYPE::CONTROL;
                    break;

                case USB_ENDPOINT_TRANSFER_TYPE_ISOCHRONOUS:
                    ep.ep_type = dep->dir ? (uint8_t)XHCI_EP_TYPE::ISOCH_IN : (uint8_t)XHCI_EP_TYPE::ISOCH_OUT;
                    break;

                case USB_ENDPOINT_TRANSFER_TYPE_BULK:
                    ep.ep_type = dep->dir ? (uint8_t)XHCI_EP_TYPE::BULK_IN : (uint8_t)XHCI_EP_TYPE::BULK_OUT;
                    break;

                case USB_ENDPOINT_TRANSFER_TYPE_INTERRUPT:
                    ep.ep_type = dep->dir ? (uint8_t)XHCI_EP_TYPE::INTERRUPT_IN : (uint8_t)XHCI_EP_TYPE::INTERRUPT_OUT;
                    break;
            }

            // FIXME - max_burst_size?
            // FIXME - max_esit_payload?

            ep.interval = find_interval(ep_desc, speed_class);
            ep.max_packet_size = ep_desc.wMaxPacketSize;
            ep.tr_dequeue_pointer = va_to_pa_phys(dep->queue.ring) | XHCI_EP_DCS;
            ep.cerr = 3;

            flush_cache(span((uint8_t*)&context, sizeof(xhci_input_context<LARGE>)),
                        false);

            flush_cache(span((uint8_t*)dep->queue.ring, (ENDPOINT_RING_SIZE + 1) * sizeof(xhci_trb_common)),
                        false);

            controller.command_queue.submit(req, true);

            wait_for_object(req.event.get(), false);

            if ((enum TRB_CODE)req.comp.completion_code == TRB_CODE::SUCCESS && new_dep) {
                lock_guard lock2(endpoint_spinlock);
                insert_tail_list(endpoints, dep->le);
            }
        }

        return true;
    };

    if (controller.large_contexts) {
        if (!func.template operator()<true>())
            return nullptr;
    } else {
        if (!func.template operator()<false>())
            return nullptr;
    }

    // FIXME - translate into nt_status?
    if ((enum TRB_CODE)req.comp.completion_code != TRB_CODE::SUCCESS) {
        print("usb_device::configure_endpoint: controller returned error {}\n", (enum TRB_CODE)req.comp.completion_code);
        return nullptr;
    }

    return &dep->queue;
}

TRB_CODE xhci_queue::dequeue_pointer_ep0() {
    xhci_request req;

    auto& s = *bit_cast<xhci_trb_set_tr_dequeue_pointer*>(&req.trb);

    memset(&s, 0, sizeof(s));

    s.trb_type = (uint16_t)xhci_trb_type::set_tr_dequeue_pointer_command;
    s.endpoint_id = 1;
    s.slot_id = dev->slot;
    s.new_tr_dequeue_pointer = va_to_pa_phys(&ring[index]) | (cycle_bit ? 1 : 0);

    dev->controller.command_queue.submit(req, true);

    wait_for_object(req.event.get(), false);

    return (TRB_CODE)req.comp.completion_code;
}

void xhci_queue::reset_ep0() {
    {
        xhci_request req;

        auto& s = *bit_cast<xhci_trb_reset_endpoint*>(&req.trb);

        memset(&s, 0, sizeof(s));

        s.trb_type = (uint16_t)xhci_trb_type::reset_endpoint_command;
        s.endpoint_id = 1;
        s.slot_id = dev->slot;

        dev->controller.command_queue.submit(req, true);

        wait_for_object(req.event.get(), false);

        auto code = (TRB_CODE)req.comp.completion_code;

        if (code != TRB_CODE::SUCCESS) {
            print("xhci_queue::reset_ep0: reset endpoint returned {}\n", code);
            return;
        }
    }

    auto code = dequeue_pointer_ep0();

    if (code != TRB_CODE::SUCCESS)
        print("xhci_queue::reset_ep0: set dequeue pointer returned {}\n", code);
}

void xhci_queue::recover_ep0_after_timeout() {
    if (!dev)
        return;

    {
        xhci_request req;

        auto& s = *bit_cast<xhci_trb_stop_endpoint*>(&req.trb);

        memset(&s, 0, sizeof(s));

        s.trb_type = (uint16_t)xhci_trb_type::stop_endpoint_command;
        s.endpoint_id = 1;
        s.slot_id = dev->slot;

        dev->controller.command_queue.submit(req, true);

        wait_for_object(req.event.get(), false);

        auto code = (TRB_CODE)req.comp.completion_code;

        if (code != TRB_CODE::SUCCESS && code != TRB_CODE::CONTEXT_STATE_ERROR) {
            print("xhci_queue::recover_ep0_after_timeout: stop endpoint returned {}\n", code);
            return;
        }
    }

    auto code = dequeue_pointer_ep0();

    if (code == TRB_CODE::CONTEXT_STATE_ERROR)
        reset_ep0();
    else if (code != TRB_CODE::SUCCESS)
        print("xhci_queue::recover_ep0_after_timeout: dequeue pointer returned {}\n", code);
}

bool xhci_queue::control_transfer(span<uint8_t> buf, uint8_t bmRequestType,
                                  enum usb_request bRequest, uint16_t wValue,
                                  uint16_t wIndex, uint64_t timeout,
                                  bool quiet) {
    xhci_request req_setup, req_data, req_status;
    uint8_t* bounce = nullptr;
    uint64_t pa;
    nt_status status;

    if (!buf.empty()) {
        pa = dma_setup(buf, !(bmRequestType & USB_REQUEST_TYPE_DEVICE_TO_HOST),
                       bounce);
        if (!pa)
            return false;
    } else
        pa = 0; // silence warning

    {
        auto& s = *bit_cast<xhci_trb_setup_stage*>(&req_setup.trb);

        memset(&s, 0, sizeof(s));

        s.bmRequestType = bmRequestType;
        s.bRequest = (uint8_t)bRequest;
        s.wValue = wValue;
        s.wIndex = wIndex;
        s.wLength = (uint16_t)buf.size();
        s.trb_transfer_length = 8;
        s.idt = 1;
        s.trb_type = (uint32_t)xhci_trb_type::setup_stage;

        if (buf.empty())
            s.transfer_type = 0; // no data
        else if (bmRequestType & USB_REQUEST_TYPE_DEVICE_TO_HOST)
            s.transfer_type = 3; // IN
        else
            s.transfer_type = 2; // OUT
    }

    if (!buf.empty()) {
        auto& s = *bit_cast<xhci_trb_data_stage*>(&req_data.trb);

        memset(&s, 0, sizeof(s));

        s.data_buffer = pa;
        s.trb_transfer_length = (uint16_t)buf.size();
        s.trb_type = (uint16_t)xhci_trb_type::data_stage;
        s.dir = bmRequestType & USB_REQUEST_TYPE_DEVICE_TO_HOST ? 1 : 0;
    }

    {
        auto& s = *bit_cast<xhci_trb_status_stage*>(&req_status.trb);

        memset(&s, 0, sizeof(s));

        s.ioc = 1;
        s.trb_type = (uint16_t)xhci_trb_type::status_stage;
        s.dir = (buf.empty() || !(bmRequestType & USB_REQUEST_TYPE_DEVICE_TO_HOST)) ? 1 : 0;
    }

    if (!buf.empty()) {
        array<xhci_request*, 3> reqs;

        reqs[0] = &req_setup;
        reqs[1] = &req_data;
        reqs[2] = &req_status;

        status = submit_multiple(reqs, timeout);
    } else {
        array<xhci_request*, 2> reqs;

        reqs[0] = &req_setup;
        reqs[1] = &req_status;

        status = submit_multiple(reqs, timeout);
    }

    if (!buf.empty()) {
        if (bmRequestType & USB_REQUEST_TYPE_DEVICE_TO_HOST)
            dma_finish_read(buf, bounce);
        else
            dma_finish_write(bounce);
    }

    if (dev && dev->dead)
        return false;

    if (status == nt_status::TIMEOUT) {
        if (!quiet)
            print("xhci_queue::control_transfer: request timed out ({})\n", (enum TRB_CODE)req_setup.ev.completion_code);

        recover_ep0_after_timeout();
        return false;
    }

    if (status == nt_status::SUCCESS) { // SETUP failed
        if (!quiet)
            print("xhci_queue::control_transfer: controller returned error {} on SETUP\n", (enum TRB_CODE)req_setup.ev.completion_code);

        if (dev && !dev->dead && (TRB_CODE)req_setup.ev.completion_code == TRB_CODE::STALL_ERROR)
            reset_ep0();

        return false;
    } else if (!buf.empty() && status == (nt_status)((uint32_t)nt_status::SUCCESS + 1)) { // DATA failed
        if (!quiet)
            print("xhci_queue::control_transfer: controller returned error {} on DATA\n", (enum TRB_CODE)req_data.ev.completion_code);

        if (dev && !dev->dead && (TRB_CODE)req_data.ev.completion_code == TRB_CODE::STALL_ERROR)
            reset_ep0();

        return false;
    }

    if ((enum TRB_CODE)req_status.ev.completion_code != TRB_CODE::SUCCESS) {
        if (!quiet)
            print("xhci_queue::control_transfer: controller returned error {}\n", (enum TRB_CODE)req_status.ev.completion_code);

        if (dev && !dev->dead && (TRB_CODE)req_status.ev.completion_code == TRB_CODE::STALL_ERROR)
            reset_ep0();

        return false;
    }

    return true;
}

bool usb_device::get_descriptor(span<uint8_t> buf, enum usb_descriptor desc_type,
                                uint8_t desc_index) {
    if (!ep0_queue.control_transfer(buf,
            USB_REQUEST_TYPE_DEVICE_TO_HOST | USB_REQUEST_TYPE_STANDARD | USB_REQUEST_TYPE_RECIPIENT_DEVICE,
            usb_request::GET_DESCRIPTOR, (uint16_t)(((uint8_t)desc_type << 8) | desc_index), 0)) {
        print("xhci_queue::control_transfer failed\n");
        return false;
    }

    return true;
}

bool usb_device::get_descriptor_class(span<uint8_t> buf, enum usb_descriptor desc_type,
                                      uint8_t desc_index) {
    if (!ep0_queue.control_transfer(buf,
            USB_REQUEST_TYPE_DEVICE_TO_HOST | USB_REQUEST_TYPE_CLASS | USB_REQUEST_TYPE_RECIPIENT_DEVICE,
            usb_request::GET_DESCRIPTOR, (uint16_t)(((uint8_t)desc_type << 8) | desc_index), 0)) {
        print("xhci_queue::control_transfer failed\n");
        return false;
    }

    return true;
}

bool usb_device::get_descriptor_interface(span<uint8_t> buf, enum usb_descriptor desc_type,
                                          uint8_t desc_index, uint16_t int_num) {
    if (!ep0_queue.control_transfer(buf,
            USB_REQUEST_TYPE_DEVICE_TO_HOST | USB_REQUEST_TYPE_STANDARD | USB_REQUEST_TYPE_RECIPIENT_INTERFACE,
            usb_request::GET_DESCRIPTOR, (uint16_t)(((uint8_t)desc_type << 8) | desc_index), int_num)) {
        print("xhci_queue::control_transfer failed\n");
        return false;
    }

    return true;
}

bool usb_device::set_configuration(uint8_t configuration_value) {
    if (!ep0_queue.control_transfer(span<uint8_t>(),
            USB_REQUEST_TYPE_HOST_TO_DEVICE | USB_REQUEST_TYPE_STANDARD | USB_REQUEST_TYPE_RECIPIENT_DEVICE,
            usb_request::SET_CONFIGURATION, configuration_value, 0)) {
        print("xhci_queue::control_transfer failed\n");
        return false;
    }

    return true;
}

bool usb_device::set_feature(uint8_t bmRequestType, uint8_t feature, uint16_t index) {
    if (!ep0_queue.control_transfer(span<uint8_t>(), bmRequestType,
                                    usb_request::SET_FEATURE, feature, index)) {
        print("xhci_queue::control_transfer failed\n");
        return false;
    }

    return true;
}

bool usb_device::clear_feature(uint8_t bmRequestType, uint8_t feature, uint16_t index) {
    if (!ep0_queue.control_transfer(span<uint8_t>(), bmRequestType,
                                    usb_request::CLEAR_FEATURE, feature, index)) {
        print("xhci_queue::control_transfer failed\n");
        return false;
    }

    return true;
}

bool usb_device::get_status(uint8_t bmRequestType, span<uint8_t> buf,
                            uint16_t wValue, uint16_t wIndex) {
    if (!ep0_queue.control_transfer(buf, bmRequestType, usb_request::GET_STATUS,
                                    wValue, wIndex)) {
        print("xhci_queue::control_transfer failed\n");
        return false;
    }

    return true;
}

bool usb_root_hub::is_port_disconnected(uint8_t port) {
    xhci_portsc portsc;

    volatile auto& cap_regs = *controller.cap_regs_ptr;
    volatile auto& op_regs = *(xhci_op_regs*)((uint8_t*)&cap_regs + cap_regs.CAPLENGTH);

    portsc.value = op_regs.ports[port - 1].PORTSC;

    return !portsc.CCS;
}

size_t xhci_queue::normal_transfer(span<uint8_t> buf) {
    xhci_request req;
    uint8_t* bounce = nullptr;

    // FIXME - can set IDT flag on short OUT transfers

    auto pa = dma_setup(buf, !dir, bounce);
    if (!pa)
        return 0;

    {
        auto& s = *bit_cast<xhci_trb_normal*>(&req.trb);

        memset(&s, 0, sizeof(s));

        s.data_buffer = pa;
        s.trb_transfer_length = (uint16_t)buf.size();

        s.ioc = 1;
        s.trb_type = (uint16_t)xhci_trb_type::normal;

        submit(req, true);
    }

    wait_for_object(req.event.get(), false);

    if (dir)
        dma_finish_read(buf, bounce);
    else
        dma_finish_write(bounce);

    if (dev && dev->dead)
        return 0;

    auto code = (enum TRB_CODE)req.ev.completion_code;

    if (code != TRB_CODE::SUCCESS && code != TRB_CODE::SHORT_PACKET) {
        // don't print message if transaction error due to disconnect
        if (code == TRB_CODE::USB_TRANSACTION_ERROR && dev && dev->parent.is_port_disconnected(dev->port))
            return 0;

        print("xhci_queue::normal_transfer: controller returned error {}\n", code);

        return 0;
    }

    return buf.size() - req.ev.trb_transfer_length;
}

size_t xhci_queue::normal_transfer_dio(pfn_iterator& it, uint64_t num_pages) {
    assert(num_pages <= size);

    auto reqs = new xhci_request[num_pages]{};
    if (!reqs) {
        print("new failed\n");
        return 0;
    }

    for (uint64_t i = 0; i < num_pages; i++) {
        auto& s = *bit_cast<xhci_trb_normal*>(&reqs[i].trb);

        if (i != 0)
            it++;

        s.data_buffer = *it;

        flush_cache(span((uint8_t*)phys_address(s.data_buffer), PAGE_SIZE), false);

        s.trb_transfer_length = PAGE_SIZE;
        s.trb_type = (uint16_t)xhci_trb_type::normal;

        if (i == num_pages - 1)
            s.ioc = 1;
        else
            s.ch = 1;

        if (i == num_pages - 1)
            reqs[i].event = create_event(false);
    }

    {
        lock_guard lock(submit_lock);

        for (uint64_t i = 0; i < num_pages; i++) {
            submit_holding_lock(reqs[i], i == num_pages - 1);
        }
    }

    wait_for_object(reqs[num_pages - 1].event.get(), false);

    if (dev && dev->dead) {
        delete[] reqs;
        return 0;
    }

    auto code = (enum TRB_CODE)reqs[num_pages - 1].ev.completion_code;

    if (code != TRB_CODE::SUCCESS && code != TRB_CODE::SHORT_PACKET) {
        print("xhci_queue::normal_transfer_dio: controller returned error {}\n", code);
        delete[] reqs;
        return 0;
    }

    if (dir) {
        for (uint64_t i = 0; i < num_pages; i++) {
            auto& s = *bit_cast<xhci_trb_normal*>(&reqs[i].trb);

            flush_cache(span((uint8_t*)phys_address(s.data_buffer), PAGE_SIZE),
                        true);
        }
    }

    size_t transferred = 0;

    for (uint64_t i = 0; i < num_pages; i++) {
        transferred += reqs[i].ev.trb_transfer_length;
    }

    delete[] reqs;

    return (num_pages << PAGE_SHIFT) - transferred;
}

uint8_t xhci::get_port_slot_type(uint8_t port_id) {
    volatile auto& cap_regs = *cap_regs_ptr;
    auto xecp = ((cap_regs.HCCPARAMS1) & XHCI_HCCPARAMS1_XECP) >> XHCI_HCCPARAMS1_XECP_SHIFT;

    volatile auto* ptr = (uint32_t*)((uint8_t*)&cap_regs + (xecp << 2));

    while (true) {
        xhci_extended_capability cap;

        auto off = (uintptr_t)((uint8_t*)ptr - (uint8_t*)cap_regs_ptr);
        if (off + sizeof(xhci_extended_capability) > cap_regs_max_size) // would overflow BAR
            break;

        copy_by_uint32(&cap, ptr, sizeof(cap));

        if (cap.cap_type == xhci_capability_type::supported_protocol) {
            xhci_supported_protocol prot;

            auto off = (uintptr_t)((uint8_t*)ptr - (uint8_t*)cap_regs_ptr);
            if (off + sizeof(xhci_supported_protocol) > cap_regs_max_size) // would overflow BAR
                break;

            // Qemu insists that we read 32 bits at a time
            copy_by_uint32(&prot, ptr, sizeof(prot));

            if (port_id >= prot.port_offset && port_id < prot.port_offset + prot.port_count)
                return prot.slot_type;
        }

        if (cap.next_cap == 0)
            break;

        ptr += cap.next_cap;
    }

    print("xhci::get_port_slot_type: could not find port {} in capabilities\n", port_id);

    return 0;
}

void print_usb_config(span<const uint8_t> buf) {
    while (!buf.empty()) {
        auto len = buf[0];

        if (len > buf.size() || len < 2) {
            print("buffer truncated\n");
            return;
        }

        auto type = (enum usb_descriptor)buf[1];

        switch (type) {
            case usb_descriptor::DEVICE: {
                auto& d = *(usb_device_descriptor*)buf.data();

                if (len < sizeof(usb_device_descriptor)) {
                    print("buffer truncated\n");
                    return;
                }

                print("DEVICE: bcdUSB = {:x}, bDeviceClass = {}, bDeviceSubClass = {}, bDeviceProtocol = {}, bMaxPacketSize0 = {}, idVendor = {:04x}, idProduct = {:04x}, bcdDevice = {:x}, iManufacturer = {}, iProduct = {}, iSerialNumber = {}, bNumConfigurations = {}\n",
                      +d.bcdUSB, d.bDeviceClass, +d.bDeviceSubClass, +d.bDeviceProtocol, +d.bMaxPacketSize0,
                      +d.idVendor, +d.idProduct, +d.bcdDevice, +d.iManufacturer, +d.iProduct, +d.iSerialNumber,
                      +d.bNumConfigurations);
                break;
            }

            case usb_descriptor::CONFIGURATION: {
                auto& c = *(usb_configuration_descriptor*)buf.data();

                if (len < sizeof(usb_configuration_descriptor)) {
                    print("buffer truncated\n");
                    return;
                }

                print("CONFIGURATION: wTotalLength = {}, bNumInterfaces = {}, bConfigurationValue = {}, iConfiguration = {}, bmAttributes = {:x}, bMaxPower = {}\n",
                      +c.wTotalLength, c.bNumInterfaces, c.bConfigurationValue, c.iConfiguration, c.bmAttributes, c.bMaxPower);
                break;
            }

            // FIXME - STRING

            case usb_descriptor::INTERFACE: {
                auto& i = *(usb_interface_descriptor*)buf.data();

                if (len < sizeof(usb_interface_descriptor)) {
                    print("buffer truncated\n");
                    return;
                }

                print("INTERFACE: bInterfaceNumber = {}, bAlternateSetting = {}, bNumEndpoints = {}, bInterfaceClass = {}, bInterfaceSubClass = {}, bInterfaceProtocol = {}, iInterface = {}\n",
                      i.bInterfaceNumber, i.bAlternateSetting, i.bNumEndpoints, i.bInterfaceClass, i.bInterfaceSubClass, i.bInterfaceProtocol, i.iInterface);
                break;
            }

            case usb_descriptor::ENDPOINT: {
                auto& e = *(usb_endpoint_descriptor*)buf.data();

                if (len < sizeof(usb_endpoint_descriptor)) {
                    print("buffer truncated\n");
                    return;
                }

                string_view transfer_type;

                switch (e.transfer_type) {
                    case 0:
                        transfer_type = "control";
                        break;

                    case 1:
                        transfer_type = "isochronous";
                        break;

                    case 2:
                        transfer_type = "bulk";
                        break;

                    case 3:
                        transfer_type = "interrupt";
                        break;
                }

                print("ENDPOINT: endpoint = {} {}, transfer_type = {}, synchronization_type = {:x}, usage_type = {:x}, wMaxPacketSize = {}, bInterval = {}\n",
                      +e.endpoint_number, e.direction == USB_ENDPOINT_DIRECTION_IN ? "IN" : "OUT",
                      transfer_type, +e.synchronization_type, +e.usage_type,
                      +e.wMaxPacketSize, e.bInterval);
                break;
            }

            // FIXME - INTERFACE_POWER
            // FIXME - OTG
            // FIXME - DEBUG

            case usb_descriptor::INTERFACE_ASSOCIATION: {
                auto& i = *(usb_interface_association_descriptor*)buf.data();

                if (len < sizeof(usb_interface_association_descriptor)) {
                    print("buffer truncated\n");
                    return;
                }

                print("INTERFACE ASSOCIATION: bFirstInterface = {}, bInterfaceCount = {}, bFunctionClass = {}, bFunctionSubClass = {}, bFunctionProtocol = {}, iFunction = {}\n",
                      i.bFirstInterface, i.bInterfaceCount, i.bFunctionClass, i.bFunctionSubClass, i.bFunctionProtocol, i.iFunction);
                break;
            }

            // FIXME - BOS
            // FIXME - DEVICE_CAPABILITY

            case usb_descriptor::HID: {
                auto& h = *(usb_hid_descriptor*)buf.data();

                if (len < offsetof(usb_hid_descriptor, entries)) {
                    print("buffer truncated\n");
                    return;
                }

                if (len < offsetof(usb_hid_descriptor, entries) + (sizeof(usb_hid_descriptor_entry) * h.bNumDescriptors)) {
                    print("buffer truncated\n");
                    return;
                }

                print("HID: bcdHID = {:x}, bCountryCode = {}, bNumDescriptors = {}",
                      +h.bcdHID, h.bCountryCode, h.bNumDescriptors);

                for (unsigned int i = 0; i < h.bNumDescriptors; i++) {
                    print(", bDescriptorType2 = {}, wDescriptorLength = {}",
                        (uint8_t)h.entries[i].bDescriptorType, +h.entries[i].wDescriptorLength);
                }
                print("\n");

                break;
            }

            case usb_descriptor::HUB: {
                auto& h = *(usb2_hub_descriptor*)buf.data();

                if (len < offsetof(usb2_hub_descriptor, DeviceRemovable)) {
                    print("buffer truncated\n");
                    return;
                }

                unsigned int dev_rem_bytes = (h.bNbrPorts / 8) + 1;

                if (len < offsetof(usb2_hub_descriptor, DeviceRemovable) + dev_rem_bytes) {
                    print("buffer truncated\n");
                    return;
                }

                print("HUB: bNbrPorts = {}, power_switching_mode = {}, compound_device = {}, overcurrent_protection_mode = {}, tt_think_time = {}, port_indicators_supported = {}, bPwrOn2PwrGood = {}, bHubContrCurrent = {}, DeviceRemovable = ",
                      h.bNbrPorts, +h.power_switching_mode, +h.compound_device, +h.overcurrent_protection_mode, +h.tt_think_time,
                      +h.port_indicators_supported, h.bPwrOn2PwrGood, h.bHubContrCurrent);

                for (unsigned int i = 0; i < dev_rem_bytes; i++) {
                    print("{:02x}", h.DeviceRemovable[i]);
                }

                print("\n");

                break;
            }

            case usb_descriptor::SUPERSPEED_HUB: {
                auto& h = *(usb3_hub_descriptor*)buf.data();

                if (len < sizeof(usb3_hub_descriptor)) {
                    print("buffer truncated\n");
                    return;
                }

                print("SUPERSPEED_HUB: bNbrPorts = {}, power_switching_mode = {}, compound_device = {}, overcurrent_protection_mode = {}, bPwrOn2PwrGood = {}, bHubContrCurrent = {}, bHubHdrDecLat = {}, wHubDelay = {}, DeviceRemovable = {:x}\n",
                      h.bNbrPorts, +h.power_switching_mode, +h.compound_device, +h.overcurrent_protection_mode, h.bPwrOn2PwrGood,
                      h.bHubContrCurrent, h.bHubHdrDecLat, +h.wHubDelay, +h.DeviceRemovable);

                break;
            }

            // FIXME - SUPERSPEED_USB_ENDPOINT_COMPANION
            // FIXME - SUPERSPEEDPLUS_ISOCHRONOUS_ENDPOINT_COMPANION

            default:
                print("unrecognized USB descriptor type {}\n", (uint8_t)type);
                break;
        }

        buf = buf.subspan(len);
    }
}

static void probe_interfaces(usb_device& d, usb_device_config& conf) {
    for (auto le = conf.interfaces.next; le != &conf.interfaces; le = le->next) {
        auto& i = containing_record<usb_interface>(le);

        for (auto le = usb_interfaces.next; le != &usb_interfaces; le = le->next) {
            auto& r = containing_record<usb_recognizer>(le);

            if (r.interface_code == i.desc.bInterfaceClass && r.func(d, conf, i))
                break;
        }
    }
}

uint8_t xhci::get_speed_num(uint8_t port_id, uint32_t psi_num) {
    volatile auto& cap_regs = *cap_regs_ptr;
    auto xecp = ((cap_regs.HCCPARAMS1) & XHCI_HCCPARAMS1_XECP) >> XHCI_HCCPARAMS1_XECP_SHIFT;

    static const uint32_t default_speeds[] = {
        0x000c0020, // full-speed
        0x05dc0010, // low-speed
        0x01e00020, // high-speed
        0x00050130, // superspeed gen1 x1
        0x000a4130, // superspeed+ gen2 x1
        0x000a4130, // superspeed+ gen1 x2
        0x00144130  // superspeed+ gen2 x2
    };

    psi_num &= 0xfffffff0;

    volatile auto* ptr = (uint32_t*)((uint8_t*)&cap_regs + (xecp << 2));

    while (true) {
        xhci_extended_capability cap;

        auto off = (uintptr_t)((uint8_t*)ptr - (uint8_t*)cap_regs_ptr);
        if (off + sizeof(xhci_extended_capability) > cap_regs_max_size) // would overflow BAR
            break;

        copy_by_uint32(&cap, ptr, sizeof(cap));

        if (cap.cap_type == xhci_capability_type::supported_protocol) {
            xhci_supported_protocol prot;

            auto off = (uintptr_t)((uint8_t*)ptr - (uint8_t*)cap_regs_ptr);
            if (off + sizeof(xhci_supported_protocol) > cap_regs_max_size) // would overflow BAR
                break;

            // Qemu insists that we read 32 bits at a time
            copy_by_uint32(&prot, ptr, sizeof(prot));

            // FIXME - check name string

            auto ptr2 = ptr + (sizeof(prot) / sizeof(uint32_t));
            if (port_id >= prot.port_offset && port_id < prot.port_offset + prot.port_count) {
                for (unsigned int i = 0; i < prot.psic; i++) {
                    uint32_t num;

                    copy_by_uint32(&num, ptr2, sizeof(num));
                    ptr2++;

                    if ((num & 0xfffffff0) == psi_num) {
                        auto& psi = *bit_cast<xhci_psi*>(&num);

                        return psi.psiv;
                    }
                }

                if (prot.psic == 0) {
                    if (prot.revision_major == 2 && prot.revision_minor == 0) { // USB 2.0
                        if (default_speeds[0] == psi_num)
                            return 1;
                        else if (default_speeds[1] == psi_num)
                            return 2;
                        else if (default_speeds[2] == psi_num)
                            return 3;
                    } else if (prot.revision_major == 3 && prot.revision_minor == 0) { // USB 3.0
                        if (default_speeds[3] == psi_num)
                            return 4;
                    } else if (prot.revision_major == 3 && prot.revision_minor == 0x10) { // USB 3.1
                        if (default_speeds[3] == psi_num)
                            return 4;
                        else if (default_speeds[4] == psi_num)
                            return 5;
                    } else if (prot.revision_major == 3 && prot.revision_minor == 0x20) { // USB 3.2
                        if (default_speeds[3] == psi_num)
                            return 4;
                        else if (default_speeds[4] == psi_num)
                            return 5;
                        else if (default_speeds[5] == psi_num)
                            return 6;
                        else if (default_speeds[6] == psi_num)
                            return 7;
                    }
                }

                print("xhci::get_speed_num: could not find PSIV for speed {:08x} for port {}\n", psi_num, port_id);

                return 0;
            }
        }

        if (cap.next_cap == 0)
            break;

        ptr += cap.next_cap;
    }

    print("xhci::get_speed_num: could not find port {} in capabilities\n", port_id);

    return 0;
}

pair<uint32_t, uint16_t> xhci::get_speed(uint8_t port_id, uint8_t speed) {
    volatile auto& cap_regs = *cap_regs_ptr;
    auto xecp = ((cap_regs.HCCPARAMS1) & XHCI_HCCPARAMS1_XECP) >> XHCI_HCCPARAMS1_XECP_SHIFT;

    static const uint32_t default_speeds[] = {
        0x000c0021, // full-speed
        0x05dc0012, // low-speed
        0x01e00023, // high-speed
        0x00050134, // superspeed gen1 x1
        0x000a4135, // superspeed+ gen2 x1
        0x000a4136, // superspeed+ gen1 x2
        0x00144137  // superspeed+ gen2 x2
    };

    volatile auto* ptr = (uint32_t*)((uint8_t*)&cap_regs + (xecp << 2));

    while (true) {
        xhci_extended_capability cap;

        auto off = (uintptr_t)((uint8_t*)ptr - (uint8_t*)cap_regs_ptr);
        if (off + sizeof(xhci_extended_capability) > cap_regs_max_size) // would overflow BAR
            break;

        copy_by_uint32(&cap, ptr, sizeof(cap));

        if (cap.cap_type == xhci_capability_type::supported_protocol) {
            xhci_supported_protocol prot;

            auto off = (uintptr_t)((uint8_t*)ptr - (uint8_t*)cap_regs_ptr);
            if (off + sizeof(xhci_supported_protocol) > cap_regs_max_size) // would overflow BAR
                break;

            // Qemu insists that we read 32 bits at a time
            copy_by_uint32(&prot, ptr, sizeof(prot));

            // FIXME - check name string

            auto ptr2 = ptr + (sizeof(prot) / sizeof(uint32_t));
            if (port_id >= prot.port_offset && port_id < prot.port_offset + prot.port_count) {
                optional<uint32_t> found_speed = 0;

                for (unsigned int i = 0; i < prot.psic; i++) {
                    xhci_psi psi;

                    copy_by_uint32(&psi, ptr2, sizeof(psi));
                    ptr2++;

                    if (psi.psiv == speed) {
                        found_speed = *bit_cast<uint32_t*>(&psi);
                        break;
                    }
                }

                if (prot.psic == 0) {
                    if (prot.revision_major == 2 && prot.revision_minor == 0) { // USB 2.0
                        if (speed >= 1 && speed <= 3)
                            found_speed = default_speeds[speed - 1];
                    } else if (prot.revision_major == 3 && prot.revision_minor == 0) { // USB 3.0
                        if (speed == 4)
                            found_speed = default_speeds[speed - 1];
                    } else if (prot.revision_major == 3 && prot.revision_minor == 0x10) { // USB 3.1
                        if (speed >= 4 && speed <= 5)
                            found_speed = default_speeds[speed - 1];
                    } else if (prot.revision_major == 3 && prot.revision_minor == 0x20) { // USB 3.2
                        if (speed >= 4 && speed <= 7)
                            found_speed = default_speeds[speed - 1];
                    }
                }

                if (!found_speed.has_value()) {
                    print("xhci::get_speed: could not translate PSIV {} into speed for port {}\n", speed, port_id);
                    return { 0, 0 };
                }

                return { *found_speed, (prot.revision_major << 8) | prot.revision_minor };
            }
        }

        if (cap.next_cap == 0)
            break;

        ptr += cap.next_cap;
    }

    print("xhci::get_speed: could not find port {} in capabilities\n", port_id);

    return { 0, 0 };
}

static bool parse_usb_config(usb_device_config& conf) {
    auto& cd = *(usb_configuration_descriptor*)conf.data;
    auto buf = span((uint8_t*)conf.data, cd.wTotalLength);
    usb_interface* intobj = nullptr;

    while (!buf.empty()) {
        auto len = buf[0];

        if (len > buf.size() || len < 2) {
            print("buffer truncated\n");
            return false;
        }

        auto type = (enum usb_descriptor)buf[1];

        switch (type) {
            case usb_descriptor::INTERFACE: {
                auto& i = *(usb_interface_descriptor*)buf.data();

                if (len < sizeof(usb_interface_descriptor)) {
                    print("buffer truncated\n");
                    return false;
                }

                intobj = new usb_interface(i);
                if (!intobj) {
                    print("new failed\n");
                    return false;
                }

                insert_tail_list(conf.interfaces, intobj->le);

                break;
            }

            case usb_descriptor::ENDPOINT: {
                auto& e = *(usb_endpoint_descriptor*)buf.data();

                if (len < sizeof(usb_endpoint_descriptor)) {
                    print("buffer truncated\n");
                    return false;
                }

                if (!intobj) {
                    print("parse_usb_config: ignoring ENDPOINT before first INTERFACE\n");
                    break;
                }

                auto ep = new usb_endpoint(e);
                if (!ep) {
                    print("new failed\n");
                    return false;
                }

                insert_tail_list(intobj->endpoints, ep->le);

                break;
            }

            default:
                break;
        }

        buf = buf.subspan(len);
    }

    return true;
}

void xhci_queue::break_waits() {
    lock_guard lock(submit_lock);

    for (uint16_t i = 0; i < size; i++) {
        if (reqs[i])
            set_event(static_cast<event&>(reqs[i]->event.get()));
    }
}

void usb_device::device_removed() {
    if (slot != 0) {
        parent.children[port - 1] = nullptr;

        controller.disable_slot(slot);
        controller.dcbaa[slot] = 0;

        slot = 0;
    }

    // FIXME - if hub, handle child devices

    print("USB device {:04x}:{:04x} removed\n", +device_descriptor.idVendor,
          +device_descriptor.idProduct);

    {
        lock_guard lock(endpoint_mutex);

        dead = true;
    }

    {
        lock_guard lock(endpoint_spinlock);

        for (auto le = endpoints.next; le != &endpoints; le = le->next) {
            auto& ep = containing_record<device_endpoint>(le);

            ep.queue.break_waits();
        }
    }

    ep0_queue.break_waits();

    // FIXME - do we need a lock for this?
    if (removal_callback)
        removal_callback(removal_callback_ctx);

    release();
}

void usb_hub::device_removed(uint8_t port) {
    if (port > num_ports) {
        print("usb_hub::device_removed: port was {}, expected 1 to {}\n", port, num_ports);
        return;
    }

    if (!children[port - 1]) {
        print("usb_hub::device_removed: no device currently in port {}\n", port);
        return;
    }

    children[port - 1]->device_removed();
}

void usb_hub::device_found(uint8_t slot, uint8_t port, enum usb_speed_class speed_class,
                           uint8_t port_speed) {
    usb_device* d;

    // wait 200ms for device to come up
    msleep(200);

    auto func = [&]<bool LARGE>() -> bool {
        auto devctx = new xhci_device_context<LARGE>;
        if (!devctx) {
            print("new failed\n");
            controller.disable_slot(slot);
            return false;
        }

        memset(devctx, 0, sizeof(*devctx));

        flush_cache(span((uint8_t*)devctx, sizeof(*devctx)), false);

        controller.dcbaa[slot] = va_to_pa_phys(devctx);

        flush_cache(span((uint8_t*)&controller.dcbaa[slot], sizeof(uint64_t)), false);

        acquire();

        d = new usb_device(controller, *this, devctx, slot, port_speed, port,
                           speed_class);
        if (!d) {
            release();
            print("new failed\n");
            delete devctx;
            controller.dcbaa[slot] = 0;
            controller.disable_slot(slot);
            return false;
        }

        if (!d->initialized) {
            print("usb_device::usb_device failed\n");
            d->release();
            return false;
        }

        return true;
    };

    if (controller.large_contexts) {
        if (!func.template operator()<true>())
            return;
    } else {
        if (!func.template operator()<false>())
            return;
    }

    memset(&d->device_descriptor, 0, sizeof(d->device_descriptor));

    auto buf = (uint8_t*)malloc(1024);
    auto sp = span(buf, 1024);

    if (!d->get_descriptor(sp, usb_descriptor::DEVICE, 0)) {
        print("usb_device::get_descriptor failed\n");
        d->release();
        free(buf);
        return;
    }

    memcpy(&d->device_descriptor, buf, sizeof(d->device_descriptor));

    print("found USB device {:04x}:{:04x}\n", +d->device_descriptor.idVendor,
          +d->device_descriptor.idProduct);

#if 0
    print_usb_config(span((uint8_t*)&d->device_descriptor, sizeof(d->device_descriptor)));
#endif

    if (d->device_descriptor.bNumConfigurations == 0) {
        print("usb_hub::device_found: device had no configurations\n");
        d->release();
        free(buf);
        return;
    }

    for (uint8_t i = 0; i < d->device_descriptor.bNumConfigurations; i++) {
        usb_configuration_descriptor cd;

        if (!d->get_descriptor(sp, usb_descriptor::CONFIGURATION, i)) {
            print("usb_device::get_descriptor failed\n");
            d->release();
            free(buf);
            return;
        }

        memcpy(&cd, buf, sizeof(cd));

        if (cd.wTotalLength < sizeof(cd)) {
            print("usb_hub::device_found: config descriptor was {} bytes, expected at least {}\n",
                  +cd.wTotalLength, sizeof(cd));
            d->release();
            free(buf);
            return;
        }

        usb_device_config* conf;
        auto cd_len = cd.wTotalLength;
        auto conf_size = offsetof(usb_device_config, data) + cd_len;
        auto conf_order = bytes_to_folio_order(conf_size);

        if (conf_size < PAGE_SIZE) {
            conf = (usb_device_config*)malloc(conf_size);
            if (!conf) {
                print("malloc failed\n");
                d->release();
                free(buf);
                return;
            }
        } else {
            auto f = allocate_folio(conf_order);
            if (!f) {
                d->release();
                free(buf);
                return;
            }

            conf = (usb_device_config*)phys_address(f);
        }

        new (conf) usb_device_config;

        auto f2 = allocate_folio(conf_order);

        if (!d->get_descriptor(span((uint8_t*)phys_address(f2), 1 << (conf_order + PAGE_SHIFT)),
                               usb_descriptor::CONFIGURATION, i)) {
            print("usb_device::get_descriptor failed\n");
            conf->~usb_device_config();

            if (conf_size < PAGE_SIZE)
                free(conf);
            else
                free_folio(phys_memory_pfn(conf) << PAGE_SHIFT);

            d->release();
            free(buf);
            free_folio(f2);
            return;
        }

        conf->len = cd_len;

        memcpy(&conf->data, phys_address(f2), cd_len);
        free_folio(f2);

        insert_tail_list(d->configs, conf->le);

        if (!parse_usb_config(*conf)) {
            print("parse_usb_config failed\n");
            d->release();
            free(buf);
            return;
        }

#if 0
        print_usb_config(span((uint8_t*)conf->data, cd_len));
#endif
    }

    free(buf);

    // FIXME - support multiple configurations?

    auto& config = containing_record<usb_device_config>(d->configs.next);
    auto& cd = *(usb_configuration_descriptor*)config.data;

    if (!d->set_configuration(cd.bConfigurationValue)) {
        print("usb_device::set_configuration failed\n");
        d->release();
        return;
    }

    probe_interfaces(*d, config);
}

void xhci::port_status_changed(uint8_t port_id) {
    if (port_id == 0 || port_id > num_ports) {
        print("xhci::port_status_changed: port_id {} out of range (expected 1 to {})\n", port_id, num_ports);
        return;
    }

    lock_guard lock(enum_mutex);

    volatile auto& cap_regs = *cap_regs_ptr;
    volatile auto& op_regs = *(xhci_op_regs*)((uint8_t*)&cap_regs + cap_regs.CAPLENGTH);
    volatile auto& p = op_regs.ports[port_id - 1];
    xhci_portsc portsc;

    portsc.value = p.PORTSC;

    // clear status bits

    {
        xhci_portsc portsc_write;

        portsc_write.value = 0;

        portsc_write.PLS = portsc.PLS;
        portsc_write.PIC = portsc.PIC;
        portsc_write.WCE = portsc.WCE;
        portsc_write.WDE = portsc.WDE;
        portsc_write.WOE = portsc.WOE;

        portsc_write.CSC = 1;
        portsc_write.PP = 1;
        portsc_write.PRC = 1;

        p.PORTSC = portsc_write.value;

#if 0
        xhci_portsc new_portsc;

        new_portsc.value = p.PORTSC;

        print("xhci::port_status_changed: writing {:08x} to PORTSC ({:08x} -> {:08x})\n",
              portsc_write.value, portsc.value, new_portsc.value);
#endif
    }

    bool device_present = portsc.CCS;

    if (!device_present) {
        root_hub.device_removed(port_id);
        return;
    }

    // ignore spurious messages from controller
    if (root_hub.children[port_id - 1])
        return;

    switch (portsc.PLS) {
        case xhci_portsc_pls::U0:
            // USB 3 port, no reset needed
        break;

        case xhci_portsc_pls::POLLING: {
            xhci_portsc portsc_write;

            portsc_write.value = 0;

            portsc_write.PLS = portsc.PLS;
            portsc_write.PP = portsc.PP;
            portsc_write.PIC = portsc.PIC;
            portsc_write.WCE = portsc.WCE;
            portsc_write.WDE = portsc.WDE;
            portsc_write.WOE = portsc.WOE;

            portsc_write.PR = 1;

            p.PORTSC = portsc_write.value;

            return;
        }

        default:
            print("xhci::port_status_changed: unexpected PLS value {}\n",
                  (enum xhci_portsc_pls)portsc.PLS);
            return;
    }

    portsc.value = p.PORTSC;

    if (!portsc.PED) {
        auto tim_ref = create_timer(true);
        if (!tim_ref) {
            print("xhci::port_status_changed: create_timer failed\n");
            return;
        }

        auto& tim = static_cast<timer&>(tim_ref.get());

        auto status = tim.set(-ONE_MS_TICKS, nullptr, nullptr, false, 1, nullptr);
        if (!success(status)) {
            print("xhci::port_status_changed: timer::set returned {}\n", status);
            return;
        }

        for (unsigned int i = 0; i < 500; i++) {
            portsc.value = p.PORTSC;

            if (portsc.PED)
                break;

            wait_for_object(tim, true); // wait 1ms
        }
    }

    if (!portsc.PED) {
        print("xhci::port_status_changed: timeout waiting for port reset\n");
        return;
    }

    auto slot = enable_slot(get_port_slot_type(port_id));
    if (slot == 0) {
        print("xhci::port_status_changed: could not enable slot\n");
        return;
    }

    auto [psi_num, usb_revision] = get_speed(port_id, portsc.port_speed);
    if (psi_num == 0) {
        print("xhci::get_speed failed\n");
        disable_slot(slot);
        return;
    }

    auto psi = *(xhci_psi*)&psi_num;
    auto speed_class = usb_speed_class::unknown;

    if (psi.psim == 1500 && psi.psie == 1)
        speed_class = usb_speed_class::low_speed;
    else if (psi.psim == 12 && psi.psie == 2)
        speed_class = usb_speed_class::full_speed;
    else if (psi.psim == 480 && psi.psie == 2)
        speed_class = usb_speed_class::high_speed;
    else if ((usb_revision & 0xff00) == 0x0300) {
        if (psi.lp == 0)
            speed_class = usb_speed_class::super_speed;
        else if (psi.lp == 1)
            speed_class = usb_speed_class::super_speed_plus;
    }

    root_hub.device_found(slot, port_id, speed_class, portsc.port_speed);
}

void xhci::port_status_change_thread() {
    while (true) {
        wait_for_object(port_status_change_event.get(), true);

        while (true) {
            port_status_change* ent = nullptr;

            {
                lock_guard lock(port_status_change_lock);

                if (!is_list_empty(port_status_change_list)) {
                    ent = &containing_record<port_status_change>(port_status_change_list.next);

                    remove_entry_list(ent->le);
                }
            }

            if (!ent)
                break;

            auto port_id = ent->port_id;
            delete ent;

            port_status_changed(port_id);
        }
    }
}

void xhci::isr_port_status_change(xhci_trb_port_status_change& psc) {
    // FIXME - doing allocation inside ISR isn't brilliant
    auto ent = new port_status_change;
    if (!ent) {
        print("new failed\n");
        return;
    }

    ent->port_id = psc.port_id;

    {
        lock_guard lock(port_status_change_lock);
        insert_tail_list(port_status_change_list, ent->le);
    }

    set_event(static_cast<event&>(port_status_change_event.get()));
}

void xhci_queue::isr_transfer_event(const xhci_trb_transfer_event& ev) {
    auto ring_pa = va_to_pa_phys(ring);

    if (ev.trb_pointer < ring_pa || ev.trb_pointer >= ring_pa + (size * sizeof(xhci_trb_common)))
        return;

    size_t index = (size_t)(ev.trb_pointer - ring_pa) / sizeof(xhci_trb_common);

    xhci_request* req;

    {
        lock_guard lock(submit_lock);

        if (!reqs[index])
            return;

        req = reqs[index];
        reqs[index] = nullptr;
    }

    memcpy(&req->ev, &ev, sizeof(ev));

    set_event(static_cast<event&>(req->event.get()));
}

void usb_device::isr_transfer_event(const xhci_trb_transfer_event& ev) {
    if (ev.endpoint_id == 1) {
        ep0_queue.isr_transfer_event(ev);
        return;
    }

    uint8_t num = ev.endpoint_id >> 1;
    bool dir = ev.endpoint_id & 1;

    {
        lock_guard lock(endpoint_spinlock);

        for (auto le = endpoints.next; le != &endpoints; le = le->next) {
            auto& ep = containing_record<device_endpoint>(le);

            if (ep.num == num && !!ep.dir == !!dir) {
                ep.queue.isr_transfer_event(ev);
                return;
            }
        }
    }
}

bool usb_device::set_interface(uint16_t interface, uint16_t alternate) {
    if (!ep0_queue.control_transfer(span<uint8_t>(),
            USB_REQUEST_TYPE_HOST_TO_DEVICE | USB_REQUEST_TYPE_STANDARD | USB_REQUEST_TYPE_RECIPIENT_INTERFACE,
            usb_request::SET_INTERFACE, alternate, interface)) {
        print("xhci_queue::control_transfer failed\n");
        return false;
    }

    return true;
}

bool xhci::isr() {
    bool handled = false;
    volatile auto& cap_regs = *cap_regs_ptr;
    volatile auto& run_regs = *(xhci_run_regs*)((uint8_t*)&cap_regs + cap_regs.RTSOFF);
    volatile auto& int_reg = run_regs.int_regs[0];

    while (true) {
        auto& ev = event_ring[event_ring_offset];

        flush_cache(span((uint8_t*)&ev, sizeof(ev)), true);

        if (!!ev.cycle != !!event_ring_cycle)
            break;

        switch ((enum xhci_trb_type)ev.trb_type) {
            case xhci_trb_type::command_completion_event:
                isr_command_completion(*reinterpret_cast<xhci_trb_command_completion*>(&ev));
                break;

            case xhci_trb_type::port_status_change_event:
                isr_port_status_change(*reinterpret_cast<xhci_trb_port_status_change*>(&ev));
                break;

            case xhci_trb_type::transfer_event: {
                auto& ev2 = *reinterpret_cast<xhci_trb_transfer_event*>(&ev);

                usb_device* found_device = nullptr;

                {
                    lock_guard lock(devices_lock);

                    for (auto le = devices.next; le != &devices; le = le->next) {
                        auto& d = containing_record<usb_device>(le);

                        if (d.slot == ev2.slot_id) {
                            found_device = &d;
                            d.acquire();
                            break;
                        }
                    }
                }

                if (found_device) {
                    found_device->isr_transfer_event(ev2);
                    found_device->release();
                }

                break;
            }

            default:
                print("xhci::isr: event type {}\n", +ev.trb_type); // FIXME?
                break;
        }

        handled = true;

        event_ring_offset++;

        if (event_ring_offset == EVENT_RING_SIZE) {
            event_ring_offset = 0;
            event_ring_cycle = !event_ring_cycle;
        }
    }

    if (!handled)
        return false;

    int_reg.IMAN |= XHCI_IMAN_IP;
    write_volatile64(&int_reg.ERDP, (va_to_pa_phys(event_ring) + (event_ring_offset * sizeof(xhci_trb_common))) | XHCI_ERDP_EHB);

    return true;
}

unsigned int xhci_queue::submit_holding_lock(xhci_request& req, bool ring_bell) {
    auto ret = index;
    auto& trb = ring[index];

    req.trb.cycle = cycle_bit ? 1 : 0;
    memcpy(&trb, &req.trb, sizeof(trb));

    reqs[index] = &req;

    if (index == (unsigned int)(size - 1)) {
        auto& cmd_link = *reinterpret_cast<xhci_trb_link*>(&ring[size]);
        cmd_link.cycle = cycle_bit ? 1 : 0;

        cycle_bit = !cycle_bit;

        flush_cache(span((uint8_t*)&cmd_link, sizeof(cmd_link)), false);
    }

    flush_cache(span((uint8_t*)&trb, sizeof(trb)), false);
    barrier();

    if (ring_bell)
        *doorbell = doorbell_val;

    index++;

    if (index == size)
        index = 0;

    return ret;
}

void xhci_queue::submit(xhci_request& req, bool ring_bell) {
    req.event = create_event(false);

    {
        lock_guard lock(submit_lock);

        submit_holding_lock(req, ring_bell);
    }
}

template<size_t N>
nt_status xhci_queue::submit_multiple(array<xhci_request*, N>& req, uint64_t timeout) {
    nt_status status;
    array<unsigned int, N> indices;

    for (auto r : req) {
        r->event = create_event(false);
    }

    {
        lock_guard lock(submit_lock);

        for (size_t i = 0; i < req.size(); i++) {
            indices[i] = submit_holding_lock(*req[i], i == req.size() - 1);
        }
    }

    array<object*, N> objs;

    for (size_t i = 0; i < N; i++) {
        objs[i] = &req[i]->event.get();
    }

    status = wait_for_objects(objs, true, false, timeout);

    {
        lock_guard lock(submit_lock);

        for (auto i : indices) {
            reqs[i] = nullptr;
        }
    }

    return status;
}

uint8_t xhci::enable_slot(uint8_t slot_type) {
    xhci_request req;

    auto& s = *bit_cast<xhci_trb_enable_slot*>(&req.trb);

    memset(&s, 0, sizeof(s));

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
    s.slot_type = slot_type;
#pragma GCC diagnostic pop
    s.trb_type = (uint32_t)xhci_trb_type::enable_slot_command;

    command_queue.submit(req, true);

    wait_for_object(req.event.get(), false);

    // FIXME - translate into nt_status?
    if ((enum TRB_CODE)req.comp.completion_code != TRB_CODE::SUCCESS) {
        print("xhci::enable_slot: controller returned error {}\n", (enum TRB_CODE)req.comp.completion_code);
        return 0;
    }

    return req.comp.slot_id;
}

bool xhci::disable_slot(uint8_t slot_id) {
    xhci_request req;

    auto& s = *bit_cast<xhci_trb_disable_slot*>(&req.trb);

    memset(&s, 0, sizeof(s));

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
    s.slot_id = slot_id;
#pragma GCC diagnostic pop
    s.trb_type = (uint32_t)xhci_trb_type::disable_slot_command;

    command_queue.submit(req, true);

    wait_for_object(req.event.get(), false);

    // FIXME - translate into nt_status?
    if ((enum TRB_CODE)req.comp.completion_code != TRB_CODE::SUCCESS) {
        print("xhci::disable_slot: controller returned error {}\n", (enum TRB_CODE)req.comp.completion_code);
        return false;
    }

    return true;
}

static bool init_xhci(const pci_device& dev) {
    if (dev.header->prog_if != 0x30)
        return false;

    auto x = new xhci(dev);
    if (!x) {
        print("new failed\n");
        return false;
    }

    if (!x->initialized) {
        delete x;
        return false;
    }

    x->init_ports();

    // FIXME - add to list of xhci controllers?

    return true;
}

static void found_xhci_acpi(const acpi_device& d) {
    optional<pair<uint64_t, uint64_t>> address;
    optional<uint32_t> irq;
    bool level_sensitive;

    for (auto le = d.resources.next; le != &d.resources; le = le->next) {
        auto& r = containing_record<acpi_device_resource>(le);

        switch (r.type) {
            case acpi_device_resource_type::extended_irq: {
                auto& i = ((acpi_device_resource_extended_irq&)r).extended_irq;

                // FIXME - what are we supposed to do if multiple IRQs?

                if (!irq.has_value()) {
                    // FIXME - check entry only covers one IRQ?
                    irq = i.Interrupt;
                    level_sensitive = i.Triggering == ACPI_LEVEL_SENSITIVE;
                }

                break;
            }

            case acpi_device_resource_type::address: {
                auto& a = ((acpi_device_resource_address&)r).address;

                // FIXME - check memory is read-write?

                if (address.has_value()) {
                    print("found_xhci_acpi: multiple address ranges provided\n");
                    return;
                }

                address = make_pair(a.Address.Minimum, a.Address.AddressLength);
                break;
            }

            case acpi_device_resource_type::fixed_memory32: {
                auto& m = ((acpi_device_resource_fixed_memory32&)r).fixed_memory32;

                // FIXME - check memory is read-write?

                if (address.has_value()) {
                    print("found_xhci_acpi: multiple address ranges provided\n");
                    return;
                }

                address = make_pair(m.Address, m.AddressLength);
                break;
            }

            default:
                print("resource: {}\n", r.type);
                break;
        }
    }

    if (!address.has_value()) {
        print("found_xhci_acpi: no memory address found\n");
        return;
    }

    if (!irq.has_value()) {
        print("found_xhci_acpi: no IRQ found\n");
        return;
    }

    if (address->second == 0) {
        print("found_xhci_acpi: memory address range has length of zero\n");
        return;
    }

    // map address range

    auto pa_start = address->first & ~(PAGE_SIZE - 1);
    auto pa_end = address->first + address->second;

    if (pa_end & (PAGE_SIZE - 1))
        pa_end = (pa_end & ~(PAGE_SIZE - 1)) + PAGE_SIZE;

    auto num_pages = (uint32_t)((pa_end - pa_start) >> PAGE_SHIFT);

    auto va = allocate_va(num_pages);
    if (!va) {
        print("allocate_va failed\n");
        return;
    }

    for (unsigned int i = 0; i < num_pages; i++) {
        if (!map_page(pa_start + (i << PAGE_SHIFT), va + (i << PAGE_SHIFT),
                      mapping_type::no_cache)) {
            print("map_page failed\n");

            for (unsigned int j = 0; j < i; j++) {
                unmap_page(va + (j << PAGE_SHIFT));
            }

            free_va(va, num_pages);
            return;
        }
    }

    auto mmio = span<volatile uint8_t>((volatile uint8_t*)(va + (address->first & (PAGE_SIZE - 1))),
                                       address->second);

    auto x = new xhci(mmio, *irq, level_sensitive);
    if (!x || !x->initialized) {
        if (!x)
            print("new failed\n");

        for (unsigned int i = 0; i < num_pages; i++) {
            unmap_page(va + (i << PAGE_SHIFT));
        }

        free_va(va, num_pages);

        if (x)
            delete x;

        return;
    }

    x->init_ports();

    // FIXME - add to list of xhci controllers?
}

uint8_t xhci_enable_slot(xhci& controller, uint8_t slot_type) {
    return controller.enable_slot(slot_type);
}

uint8_t xhci_get_speed_num(xhci& controller, uint8_t port_id, uint32_t psi_num) {
    return controller.get_speed_num(port_id, psi_num);
}
