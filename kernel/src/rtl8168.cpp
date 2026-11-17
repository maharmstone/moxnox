// SPDX-License-Identifier: GPL-2.0-only
#include <stdint.h>
#include <string.h>
#include <span>
#include <string_view>
#include <mutex>
#include <bit>
#include <atomic>

import nt;
import format;
import print;
import pci;
import list;
import module_init;
import net;
import timer;
import ipv6;
import thread;
import mem;
import interrupts;
import event;
import object_ref;
import objects;
import scheduler;
import spinlock;
import semaphore;
import ws2_32;
import crc32;

using namespace std;

static const uint16_t PCI_VENDOR_ID_REALTEK = 0x10ec;

union tcr_bits {
    uint32_t value;
    struct {
        uint32_t : 4;
        uint32_t tx_retry_count : 4;
        uint32_t dma_burst : 3;
        uint32_t : 5;
        uint32_t crc_append : 1;
        uint32_t loopback : 2;
        uint32_t : 5;
        uint32_t ifg : 2;
        uint32_t : 6;
    };
};

union rcr_bits {
    uint32_t value;
    struct {
        uint32_t accept_all : 1;
        uint32_t accept_phys : 1;
        uint32_t accept_multicast : 1;
        uint32_t accept_broadcast : 1;
        uint32_t accept_runt : 1;
        uint32_t accept_error : 1;
        uint32_t eeprom_type : 1;
        uint32_t : 1;
        uint32_t rx_dma_burst : 3;
        uint32_t : 2;
        uint32_t rx_fifo_thresh : 3;
        uint32_t : 16;
    };
} __attribute__((packed));

union int_bits {
    uint16_t value;
    struct {
        uint16_t rx_ok : 1;
        uint16_t rx_err : 1;
        uint16_t tx_ok : 1;
        uint16_t tx_err : 1;
        uint16_t rx_desc_unavail : 1;
        uint16_t link_change : 1;
        uint16_t rx_fifo_overflow : 1;
        uint16_t tx_desc_unavail : 1;
        uint16_t sw_int : 1;
        uint16_t rx_fifo_empty_under_fifo_full: 1;
        uint16_t : 4;
        uint16_t timeout : 1;
        uint16_t : 1;
    };
} __attribute__((packed));

union int_bits_8125 {
    uint32_t value;
    struct {
        uint32_t rx_ok : 1;
        uint32_t rx_err : 1;
        uint32_t tx_ok : 1;
        uint32_t tx_err : 1;
        uint32_t rx_desc_unavail : 1;
        uint32_t link_change : 1;
        uint32_t rx_fifo_overflow : 1;
        uint32_t tx_desc_unavail : 1;
        uint32_t sw_int : 1;
        uint32_t : 5;
        uint32_t pcs_timeout : 1;
        uint32_t sys_error : 1;
        uint32_t : 16;
    };
} __attribute__((packed));

union rtl8168_phy_status {
    uint8_t value;
    struct {
        uint8_t full_duplex : 1;
        uint8_t link_status : 1;
        uint8_t speed_10M : 1;
        uint8_t speed_100M : 1;
        uint8_t speed_1G: 1;
        uint8_t rxflow : 1;
        uint8_t txflow : 1;
        uint8_t : 1;
    };
};

struct rtl8168_regs {
    uint8_t mac_addr[6];
    uint8_t padding1[2];
    uint32_t multicast_filter[2];
    uint32_t dump_tally_counter_low;
    uint32_t dump_tally_counter_high;
    uint8_t padding2[8];
    uint32_t tx_desc_addr_low;
    uint32_t tx_desc_addr_high;
    uint32_t tx_hpq_desc_addr_low;
    uint32_t tx_hpq_desc_addr_high;
    uint8_t padding3[7];
    uint8_t cmd;
    union {
        int_bits_8125 interrupt_mask_8125;
        struct {
            uint8_t tx_poll;
            uint8_t padding4[3];
        };
    };
    union {
        int_bits_8125 interrupt_status_8125;
        struct {
            int_bits interrupt_mask;
            int_bits interrupt_status;
        };
    };
    tcr_bits tx_config;
    rcr_bits rx_config;
    uint32_t time_counter_register;
    uint32_t padding5;
    uint8_t eeprom_command_register;
    uint8_t config0;
    uint8_t config1;
    uint8_t config2;
    uint8_t config3;
    uint8_t config4;
    uint8_t config5;
    uint8_t padding6;
    uint32_t timer_interrupt;
    uint8_t padding7[4];
    uint32_t phy_access;
    uint8_t padding8[8];
    rtl8168_phy_status phy_status;
    uint8_t padding9[23];
    uint32_t wakeup_frame0_low;
    uint32_t wakeup_frame0_high;
    uint32_t wakeup_frame1_low;
    union {
        struct {
            uint16_t tx_poll_8125;
            uint8_t padding10[2];
        };
        uint32_t wakeup_frame1_high;
    };
    uint32_t wakeup_frame2[4];
    uint32_t wakeup_frame3[4];
    uint32_t wakeup_frame4[4];
    uint16_t wakeup_frame0_crc;
    uint16_t wakeup_frame1_crc;
    uint16_t wakeup_frame2_crc;
    uint16_t wakeup_frame3_crc;
    uint16_t wakeup_frame4_crc;
    uint8_t padding11[12];
    uint16_t rx_max_size;
    uint32_t padding12;
    uint16_t cplus_command_register;
    uint16_t padding13;
    uint32_t rx_desc_addr_low;
    uint32_t rx_desc_addr_high;
    uint8_t tx_max_size;
    uint8_t padding14[19];
} __attribute__((packed));

static_assert(sizeof(rtl8168_regs) == 0x100);

static const uint8_t TXPOLL_NPQ = 1 << 6;

static const uint16_t CPLUS_RXCHKSUM = 1 << 5;

struct rtl8168_tx_desc {
    union {
        uint32_t opts1;

        struct {
            uint32_t packet_length : 16;
            uint32_t v1_tx_csum_tcp : 1;
            uint32_t v1_tx_csum_udp : 1;
            uint32_t v1_tx_csum_ip : 1;
            uint32_t : 6;
            uint32_t v2_gtsenv6 : 1;
            uint32_t v2_gtsenv4 : 1;
            uint32_t large_send : 1;
            uint32_t last_segment : 1;
            uint32_t first_segment : 1;
            uint32_t end_of_ring : 1;
            uint32_t owned_by_nic : 1;
        };
    };

    union {
        uint32_t opts2;

        struct {
            uint32_t vlan_tag : 16;
            uint32_t : 1;
            uint32_t tagc : 1;
            uint32_t v2_transport_offset : 10;
            uint32_t v2_tx_csum_ipv6 : 1;
            uint32_t v2_tx_csum_ipv4 : 1;
            uint32_t v2_tx_csum_tcp : 1;
            uint32_t v2_tx_csum_udp : 1;
        };
    };

    uint64_t addr;
} __attribute__((packed));

static_assert(sizeof(rtl8168_tx_desc) == 0x10);

struct rtl8168_rx_desc {
    union {
        uint32_t opts1;
        struct {
            uint32_t packet_length : 14;
            uint32_t tcp_checksum_failed : 1;
            uint32_t udp_checksum_failed : 1;
            uint32_t ip_checksum_failed : 1;
            uint32_t pid : 2;
            uint32_t crc_error : 1;
            uint32_t runt_packet : 1;
            uint32_t receive_error_summary : 1;
            uint32_t receive_watchdog_timer_expired : 1;
            uint32_t : 2;
            uint32_t broadcast_address_received : 1;
            uint32_t physical_address_matched : 1;
            uint32_t multicast_address_received : 1;

            uint32_t last_segment : 1;
            uint32_t first_segment : 1;
            uint32_t end_of_ring : 1;
            uint32_t owned_by_nic : 1;
        };
    };

    union {
        uint32_t opts2;
        struct {
            uint32_t vlan_id : 16;
            uint32_t vlan_tagged : 1;
            uint32_t : 15;
        };
    };

    uint64_t addr;
} __attribute__((packed));

static_assert(sizeof(rtl8168_rx_desc) == 0x10);

static const uint8_t RTL_CMD_RESET = 0x10;
static const uint8_t RTL_CMD_RX_ENABLE = 0x08;
static const uint8_t RTL_CMD_TX_ENABLE = 0x04;

static const uint16_t DEFAULT_MTU = 1500; // FIXME - put in net module?

static const size_t NUM_RX_DESC = 256;
static const size_t NUM_TX_DESC = 256;
static const uint16_t RX_BUF_SIZE = 2048;
static const uint16_t TX_BUF_SIZE = 2048;

static const uint8_t RTL_CFG9346_UNLOCK = 0xc0;
static const uint8_t RTL_CFG9346_LOCK = 0x00;

class rtl8168_netif : public netif {
public:
    rtl8168_netif(pci_device& dev, volatile rtl8168_regs& regs) : dev(dev), regs(regs) { }
    ~rtl8168_netif();

    void recv_thread();
    nt_status send_packet(const mac_address& dst_mac, ethertype type,
                          packet& pack) override;
    bool init();
    bool isr();
    void add_multicast(const mac_address& mac);

    pci_device& dev;
    volatile rtl8168_regs& regs;
    bool is_8125;

private:
    bool setup_descriptors();
    void enable_interrupts();
    void receive_packets();
    void handle_received_packet(span<const uint8_t> data,
                                const rtl8168_rx_desc& r);

    object_ref receive_packet_event, link_change_event;
    object_ref tx_semaphore;
    uint16_t rx_index = 0, tx_index = 0, tx_ack_index = 0;
    rtl8168_rx_desc* rx_ring = nullptr;
    rtl8168_tx_desc* tx_ring = nullptr;
    spinlock tx_lock;
    bool isr_registered = false;
    bool link_up;
#ifdef __aarch64__
    uint16_t msi_irq;
#endif
};

static void found_actual_rtl8168(pci_device& dev);
static void found_rtl8125(pci_device& dev);

static pci_recognizer mod_pci_ids[] = {
    { {}, PCI_VENDOR_ID_REALTEK, 0x8125, true, found_rtl8125 },
    { {}, PCI_VENDOR_ID_REALTEK, 0x8168, true, found_actual_rtl8168 },
};

static void init_rtl8168() {
    for (unsigned int i = 0; i < sizeof(mod_pci_ids) / sizeof(*mod_pci_ids); i++) {
        insert_tail_list(pci_ids, mod_pci_ids[i].le);
    }
}

static module_init module_init_rtl8168(init_rtl8168);

bool rtl8168_netif::setup_descriptors() {
    rx_ring = (rtl8168_rx_desc*)malloc_aligned(NUM_RX_DESC * sizeof(rtl8168_rx_desc), 256);
    if (!rx_ring)
        return false;

    memset(rx_ring, 0, NUM_RX_DESC * sizeof(rtl8168_rx_desc));

    for (size_t i = 0; i < NUM_RX_DESC; i++) {
        void* rx_buf = malloc(RX_BUF_SIZE);
        if (!rx_buf) {
            for (size_t j = 0; j < i; j++) {
                free(phys_address(rx_ring[j].addr));
            }
            free(rx_ring);
            return false;
        }

        rx_ring[i].addr = va_to_pa_phys(rx_buf);
        rx_ring[i].owned_by_nic = 1;
        rx_ring[i].packet_length = RX_BUF_SIZE;

        if (i == NUM_RX_DESC - 1)
            rx_ring[i].end_of_ring = 1;

        flush_cache(span((uint8_t*)rx_buf, RX_BUF_SIZE), true);
    }

    flush_cache(span((uint8_t*)rx_ring, NUM_RX_DESC * sizeof(rtl8168_rx_desc)),
                false);

    tx_ring = (rtl8168_tx_desc*)malloc_aligned(NUM_TX_DESC * sizeof(rtl8168_tx_desc), 256);
    if (!tx_ring) {
        for (size_t i = 0; i < NUM_RX_DESC; i++) {
            free(phys_address(rx_ring[i].addr));
        }
        free(rx_ring);
        return false;
    }

    memset(tx_ring, 0, NUM_TX_DESC * sizeof(rtl8168_tx_desc));

    for (size_t i = 0; i < NUM_TX_DESC; i++) {
        void* tx_buf = malloc(TX_BUF_SIZE);
        if (!tx_buf) {
            for (size_t j = 0; j < i; j++) {
                free(phys_address(tx_ring[j].addr));
            }

            for (size_t j = 0; j < NUM_RX_DESC; j++) {
                free(phys_address(rx_ring[j].addr));
            }

            free(rx_ring);
            free(tx_ring);

            return false;
        }

        tx_ring[i].addr = va_to_pa_phys(tx_buf);

        if (i == NUM_TX_DESC - 1)
            tx_ring[i].end_of_ring = 1;
    }

    flush_cache(span((uint8_t*)tx_ring, NUM_TX_DESC * sizeof(rtl8168_tx_desc)),
                false);

    auto tx_phys = va_to_pa_phys(tx_ring);
    auto rx_phys = va_to_pa_phys(rx_ring);

    regs.tx_desc_addr_low = (uint32_t)tx_phys;
    regs.tx_desc_addr_high = (uint32_t)(tx_phys >> 32);
    regs.rx_desc_addr_low = (uint32_t)rx_phys;
    regs.rx_desc_addr_high = (uint32_t)(rx_phys >> 32);

    return true;
}

void rtl8168_netif::enable_interrupts() {
    if (is_8125) {
        regs.interrupt_status_8125.value = 0xffffffff; // clear pending

        int_bits_8125 b;

        b.value = 0;
        b.rx_ok = 1;
        b.tx_ok = 1;
        b.link_change = 1;

        regs.interrupt_mask_8125.value = b.value;
    } else {
        regs.interrupt_status.value = 0xffff; // clear pending

        int_bits b;

        b.value = 0;
        b.rx_ok = 1;
        b.tx_ok = 1;
        b.link_change = 1;

        regs.interrupt_mask.value = b.value;
    }
}

bool rtl8168_netif::isr() {
    bool handled = false;

    while (true) {
        bool rx_ok, link_change, tx_ok;

        if (is_8125) {
            int_bits_8125 status;

            status.value = regs.interrupt_status_8125.value;

            if (status.value == 0)
                break;

            regs.interrupt_status_8125.value = status.value; // acknowledge interrupts
            handled = true;

            rx_ok = status.rx_ok;
            link_change = status.link_change;
            tx_ok = status.tx_ok;
        } else {
            int_bits status;

            status.value = regs.interrupt_status.value;

            if (status.value == 0)
                break;

            regs.interrupt_status.value = status.value; // acknowledge interrupts
            handled = true;

            rx_ok = status.rx_ok;
            link_change = status.link_change;
            tx_ok = status.tx_ok;
        }

        // FIXME - rx_err, tx_err, rx_fifo_overflow?

        if (rx_ok)
            set_event(static_cast<event&>(receive_packet_event.get()));

        if (link_change)
            set_event(static_cast<event&>(link_change_event.get()));

        if (tx_ok) {
            unsigned int to_release = 0;

            {
                lock_guard lock(tx_lock);

                while (!tx_ring[tx_ack_index].owned_by_nic) {
                    to_release++;

                    tx_ack_index = (tx_ack_index + 1) % NUM_TX_DESC;

                    if (tx_ack_index == tx_index)
                        break;
                }
            }

            for (unsigned int i = 0; i < to_release; i++) {
                release_semaphore(static_cast<semaphore&>(tx_semaphore.get()));
            }
        }
    }

    return handled;
}

static bool rtl8168_isr_func(void* ctx) {
    auto& r = *(rtl8168_netif*)ctx;

    return r.isr();
}

static constexpr uint32_t bitrev32(uint32_t x) {
    x = ((x & 0xaaaaaaaa) >> 1)  | ((x & 0x55555555) << 1);
    x = ((x & 0xcccccccc) >> 2)  | ((x & 0x33333333) << 2);
    x = ((x & 0xf0f0f0f0) >> 4)  | ((x & 0x0f0f0f0f) << 4);
    x = ((x & 0xff00ff00) >> 8)  | ((x & 0x00ff00ff) << 8);

    return (x >> 16) | (x << 16);
}

static constexpr uint32_t ether_crc32(const mac_address& mac) {
    return bitrev32(calc_crc32(0xffffffff, span(mac.v.data(), mac.v.size())));
}

void rtl8168_netif::add_multicast(const mac_address& mac) {
    auto crc = ether_crc32(mac);
    auto index = crc >> 26;

    auto v = byteswap(regs.multicast_filter[1 - (index >> 5)]);

    v |= 1 << (index & 0x1f);

    regs.multicast_filter[1 - (index >> 5)] = byteswap(v);
}

bool rtl8168_netif::init() {
    tcr_bits tx_config;
    rcr_bits rx_config;

    regs.eeprom_command_register = RTL_CFG9346_UNLOCK;

    tx_config.value = 0;
    tx_config.ifg = 3; // standard IFG
    tx_config.dma_burst = 7; // unlimited

    regs.tx_config.value = tx_config.value;

    regs.multicast_filter[0] = 0;
    regs.multicast_filter[1] = 0;

    rx_config.value = 0;
    rx_config.accept_phys = 1; // our MAC
    rx_config.accept_multicast = 1; // multicast
    rx_config.accept_broadcast = 1; // broadcast
    rx_config.rx_dma_burst = 7; // unlimited
    rx_config.rx_fifo_thresh = 7; // no threshold

    regs.rx_config.value = rx_config.value;

    add_multicast({0x33, 0x33, 0x00, 0x00, 0x00, 0x01}); // all nodes, ff02::1
    add_multicast({0x01, 0x00, 0x5e, 0x00, 0x00, 0x01}); // all nodes, 224.0.0.1

    regs.rx_max_size = RX_BUF_SIZE;
    regs.tx_max_size = TX_BUF_SIZE / 128;

    receive_packet_event = create_event(true);
    if (!receive_packet_event) {
        print("create_event failed\n");
        return false;
    }

    link_change_event = create_event(true);
    if (!link_change_event) {
        print("create_event failed\n");
        return false;
    }

    tx_semaphore = create_semaphore(NUM_TX_DESC);
    if (!tx_semaphore) {
        print("create_semaphore failed\n");
        return false;
    }

    if (!setup_descriptors()) {
        print("setup_descriptors failed\n");
        return false;
    }

    link_up = regs.phy_status.link_status;

    if (!dev.msi.empty() && msi_supported) {
        // FIXME - MSI-X?
        auto int_num = setup_msi(dev, false, rtl8168_isr_func, this);
        if (int_num == 0)
            return false;

        isr_registered = true;
#ifdef __aarch64__
        msi_irq = int_num;
#endif
    } else if (dev.irq.has_value()) {
        if (!add_irq_handler(*dev.irq, true, rtl8168_isr_func, this)) {
            print("add_irq_handler failed\n");
            return false;
        }

        isr_registered = true;
    } else {
        print("rtl8168_netif::init: no MSI and no IRQ found\n");
        return false;
    }

    enable_interrupts();

    regs.cmd = RTL_CMD_TX_ENABLE | RTL_CMD_RX_ENABLE;

    regs.cplus_command_register |= CPLUS_RXCHKSUM;

    regs.eeprom_command_register = RTL_CFG9346_LOCK;

    auto xid = (regs.tx_config.value >> 20) & 0xfcf;

    if ((xid & 0x7c8) != 0x380) { // RTL8168B only supports v1 offload
        tx_udpv6_csum_offload = true;
        tx_tcpv6_csum_offload = true;
    }

    return true;
}

static void found_rtl8168(pci_device& dev, bool is_8125) {
    // FIXME - is it definitely always BAR2? What does Linux do?

    if (dev.bar[2].empty()) {
        print("found_rtl8168: BAR2 not set\n");
        return;
    }

    if (dev.bar[2].size() < sizeof(rtl8168_regs)) {
        print("found_rtl8168: BAR2 was {} bytes, expected at least {}\n",
              dev.bar[2].size(), sizeof(rtl8168_regs));
        return;
    }

    volatile auto& regs = *(rtl8168_regs*)dev.bar[2].data();
    bool reset_done = false;

    regs.cmd = RTL_CMD_RESET;

    // wait up to 100ms
    for (unsigned int i = 0; i < 100; i++) {
        if (!(regs.cmd & RTL_CMD_RESET)) {
            reset_done = true;
            break;
        }

        msleep(1);
    }

    if (!reset_done) {
        print("found_rtl8168: card unresponsive, reset did not complete\n");
        return;
    }

    mac_address mac;

    for (unsigned int i = 0; i < 6; i++) {
        mac.v[i] = regs.mac_addr[i];
    }

    auto pc = new rtl8168_netif(dev, regs);
    if (!pc) {
        print("new failed\n");
        return;
    }

    auto& c = *pc;

    c.is_8125 = is_8125;

    if (!c.init()) {
        delete pc;
        return;
    }

    memcpy(&c.mac, &mac, sizeof(mac));
    c.mtu = DEFAULT_MTU; // FIXME?

    c.ll_addr = find_ll_address(mac);

    auto a = new ip_addr(c.ll_addr);
    if (!a) {
        print("new failed\n");
        delete pc;
        return;
    }

    {
        lock_guard lg(c.addr_lock);
        insert_tail_list(c.addrs, a->le);
    }

    c.add_multicast({0x33, 0x33, 0xff, c.ll_addr.v[13], c.ll_addr.v[14], c.ll_addr.v[15]}); // solicited address
    // FIXME - should do solicited address for each of the actual IPv6 addresses too

    print("found {} device with MAC {}, MTU {}, link-local address {}\n",
          is_8125 ? "rtl8125"sv : "rtl8168"sv, c.mac, c.mtu, c.ll_addr);

    c.num = next_netif_num.fetch_add(1);

    auto eth_num = next_eth_num.fetch_add(1);

    {
        char s[50];
        auto sp = span(s, sizeof(s));

        sp = format(sp, "eth{}", eth_num);

        auto sv = string_view(s, (size_t)(sp.data() - s));

        memcpy(c.name, sv.data(), sv.size());
        memset(c.name + sv.size(), 0, sizeof(c.name) - sv.size());
    }

    {
        lock_guard lg(netif_lock);
        insert_tail_list(netif_list, c.le);
    }

    c.acquire();

    add_kernel_thread([](void* ctx) {
        auto& dev = *(rtl8168_netif*)ctx;

        dev.recv_thread();
        dev.release();
    }, &c);
}

static void found_actual_rtl8168(pci_device& dev) {
    found_rtl8168(dev, false);
}

static void found_rtl8125(pci_device& dev) {
    found_rtl8168(dev, true);
}

void rtl8168_netif::recv_thread() {
    while (true) {
        array<object*, 2> objs;

        objs[0] = &link_change_event.get();
        objs[1] = &receive_packet_event.get();

        auto status = wait_for_objects(objs, true, false);
        if (!success(status)) {
            print("wait_for_objects returned {}\n", status);
            break;
        }

        if ((unsigned int)status == (unsigned int)nt_status::SUCCESS) {
            bool new_link_status = regs.phy_status.link_status;

            if (!!new_link_status != !!link_up) {
                if (new_link_status)
                    print("{}: link up\n", static_cast<netif&>(*this));
                else
                    print("{}: link down\n", static_cast<netif&>(*this));

                link_up = new_link_status;
            }
        } else if ((unsigned int)status == (unsigned int)nt_status::SUCCESS + 1)
            receive_packets();
    }
}

void rtl8168_netif::handle_received_packet(span<const uint8_t> data,
                                           const rtl8168_rx_desc& r) {
    bool checksum_okay = false;

    // ignoring the 4-byte "frame check sequence" the rtl8168 sticks at the end

    if (data.size() < sizeof(ethernet_header) + sizeof(uint32_t))
        return;

    rx_packets.fetch_add(1, memory_order::relaxed);
    rx_bytes.fetch_add(data.size() - sizeof(uint32_t), memory_order::relaxed);

    // FIXME - IPv4 checksums
    if (r.pid == 1) {
        if (r.tcp_checksum_failed)
            return;

        checksum_okay = true;
    } else if (r.pid == 2) {
        if (r.udp_checksum_failed)
            return;

        checksum_okay = true;
    }

    auto& eth = *(ethernet_header*)data.data();

    if (drop_packet(eth.dst_mac, mac))
        return;

    auto pack = new packet;
    if (!pack) {
        print("new failed\n");
        return;
    }

    pack->size = data.size() - sizeof(ethernet_header) - sizeof(uint32_t);
    memcpy(pack->data, data.data() + sizeof(ethernet_header), pack->size);

    received_packet(*this, *pack, eth.src_mac, eth.type, checksum_okay);

    pack->release();
}

void rtl8168_netif::receive_packets() {
    while (true) {
        auto& r = rx_ring[rx_index];

        flush_cache(span((uint8_t*)&r, sizeof(r)), true);

        if (rx_ring[rx_index].owned_by_nic)
            break;

        auto data = span((uint8_t*)phys_address(r.addr), r.packet_length);

        flush_cache(data, true);

        handle_received_packet(data, r);

        r.packet_length = RX_BUF_SIZE;
        r.owned_by_nic = 1;
        flush_cache(span((uint8_t*)&r, sizeof(r)), false);

        rx_index = (rx_index + 1) % NUM_RX_DESC;
    }
}

nt_status rtl8168_netif::send_packet(const mac_address& dst_mac, ethertype type,
                                     packet& pack) {
    wait_for_object(tx_semaphore.get(), false);

    tx_packets.fetch_add(1, memory_order::relaxed);
    tx_bytes.fetch_add(sizeof(ethernet_header) + pack.size, memory_order::relaxed);

    {
        lock_guard lock(tx_lock);

        auto& d = tx_ring[tx_index];

        d.opts1 = 0;
        d.opts2 = 0;

        d.packet_length = (uint16_t)(sizeof(ethernet_header) + pack.size);
        d.last_segment = 1;
        d.first_segment = 1;
        d.end_of_ring = tx_index == NUM_TX_DESC - 1 ? 1 : 0;
        d.owned_by_nic = 1;

        // FIXME - IPv4 checksumming

        if (type == ethertype::ipv6) {
            const auto& ip = *(ipv6_header*)pack.data;

            if (tx_udpv6_csum_offload && ip.next_header == IPPROTO_UDP) {
                d.v2_tx_csum_ipv6 = 1;
                d.v2_tx_csum_udp = 1;
                d.v2_transport_offset = sizeof(ethernet_header) + sizeof(ipv6_header);
            } else if (tx_tcpv6_csum_offload && ip.next_header == IPPROTO_TCP) {
                d.v2_tx_csum_ipv6 = 1;
                d.v2_tx_csum_tcp = 1;
                d.v2_transport_offset = sizeof(ethernet_header) + sizeof(ipv6_header);
            }
        }

        flush_cache(span((uint8_t*)&d, sizeof(d)), false);

        auto buf = phys_address(d.addr);

        auto& eth = *(ethernet_header*)buf;
        eth.src_mac = mac;
        eth.dst_mac = dst_mac;
        eth.type = type;

        memcpy((uint8_t*)buf + sizeof(ethernet_header), pack.data, pack.size);

        flush_cache(span((uint8_t*)buf, pack.size + sizeof(ethernet_header)),
                    false);

        if (is_8125)
            regs.tx_poll_8125 = 1;
        else
            regs.tx_poll = TXPOLL_NPQ;

        tx_index = (tx_index + 1) % NUM_TX_DESC;
    }

    return nt_status::SUCCESS;
}

rtl8168_netif::~rtl8168_netif() {
    if (isr_registered) {
        if (msi_supported && !dev.msi.empty()) {
#ifdef __aarch64__
            remove_irq_handler(msi_irq, rtl8168_isr_func, this);
            free_msi_irq(msi_irq);
#elifdef __x86_64__
            remove_isr(rtl8168_isr_func, this);
#endif
        } else if (dev.irq.has_value())
            remove_irq_handler(*dev.irq, rtl8168_isr_func, this);
    }

    for (size_t i = 0; i < NUM_TX_DESC; i++) {
        free(phys_address(tx_ring[i].addr));
    }

    for (size_t i = 0; i < NUM_RX_DESC; i++) {
        free(phys_address(rx_ring[i].addr));
    }

    free(rx_ring);
    free(tx_ring);
}
