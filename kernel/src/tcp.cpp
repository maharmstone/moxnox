// SPDX-License-Identifier: GPL-2.0-only
module;

#include <atomic>
#include <span>
#include <optional>
#include <mutex>
#include <bit>
#include <algorithm>
#include <type_traits>
#include <variant>
#include <stdlib.h>

module net;

import print;
import thread;
import event;
import scheduler;
import kernel_time_page;
import list;
import config;
import vfs;
import vec;

using namespace std;

static const uint64_t RETRANSMISSION_DELAY = 1; // seconds
static const uint64_t TIME_WAIT_LENGTH = 60; // seconds

static const uint16_t MSS_IPv4 = 536;
static const uint16_t MSS_IPv6 = 1220;

struct tcp_retransmission {
    tcp_retransmission(packet& pack) : pack(pack) {
        pack.acquire();
    }

    ~tcp_retransmission() {
        pack.release();
    }

    void acquire() {
        refcount++;
    }

    void release() {
        if (--refcount == 0)
            delete this;
    }

    list_entry le;
    atomic<unsigned int> refcount = 1;
    uint64_t deadline;
    unsigned int attempt = 0;
    packet& pack;
};

class tcp_seq {
public:
    constexpr tcp_seq() = default;

    explicit constexpr tcp_seq(uint32_t v) {
        data[0] = (uint8_t)((v >> 24) & 0xff);
        data[1] = (v >> 16) & 0xff;
        data[2] = (v >> 8) & 0xff;
        data[3] = v & 0xff;
    }

    constexpr uint32_t host() const noexcept {
        return (static_cast<uint32_t>(data[0]) << 24) |
                (static_cast<uint32_t>(data[1]) << 16) |
                (static_cast<uint32_t>(data[2]) << 8) |
                static_cast<uint32_t>(data[3]);
    }

    constexpr tcp_seq& operator+=(uint32_t n) {
        *this = tcp_seq(host() + n);
        return *this;
    }

    constexpr tcp_seq operator++(int) {
        tcp_seq old = *this;
        *this = tcp_seq(host() + 1);
        return old;
    }

    constexpr tcp_seq operator+(uint32_t n) const {
        return tcp_seq(host() + n);
    }

    constexpr int32_t operator-(tcp_seq other) const {
        return static_cast<int32_t>(host() - other.host());
    }

    constexpr strong_ordering operator<=>(tcp_seq other) const {
        int32_t diff = static_cast<int32_t>(host() - other.host());

        return diff <=> 0;
    }

    constexpr bool operator==(const tcp_seq& other) const {
        return !memcmp(data, other.data, sizeof(data));
    }

private:
    uint8_t data[4];
};

static_assert(is_standard_layout_v<tcp_seq>);
static_assert(is_trivial_v<tcp_seq>);

enum class tcp_state {
    CLOSED,
    SYN_SENT,
    ESTABLISHED,
    CLOSE_WAIT,
    FIN_WAIT_1,
    FIN_WAIT_2,
    TIME_WAIT,
    LAST_ACK,
    LISTEN,
    SYN_RECEIVED,
};

class base_tcp_socket : public socket {
public:
    base_tcp_socket(bool is_ipv4) : is_ipv4(is_ipv4) { }

    uint16_t allowed_send_bytes();

    list_entry le;
    bool is_ipv4;
    bool init = false;
    spinlock state_lock;
    object_ref state_change_event;
    bool bind_done = false;
    uint16_t bound_port, connected_port;
    uint32_t connected_scope = 0;
    tcp_state state = tcp_state::CLOSED;
    netif* pni = nullptr;
    int32_t recv_window = 0xffff;
    tcp_seq send_next, send_unacknowledged, recv_next;
    uint16_t send_window = 0;
    tcp_seq last_send_window_seq;
    mac_address dst_mac;
    packet* first_ack = nullptr;
    size_t mss;
    spinlock retransmission_list_lock;
    list_entry retransmission_list = { &retransmission_list, &retransmission_list };
    object_ref retransmission_thread_obj;
    object_ref retransmission_thread_quit_event;
    object_ref send_window_open_event;
    object_ref accept_queue_event;
    list_entry buffer = { &buffer, &buffer };
    list_entry ooo_buffer = { &ooo_buffer, &ooo_buffer };
    rwsem us_read_mutex;
    size_t us_read_offset = 0;
    spinlock io_lock;
    unsigned int listen_backlog;
    base_tcp_socket* parent = nullptr;
    spinlock list_lock;
    list_entry syn_queue = { &syn_queue, &syn_queue };
    list_entry accept_queue = { &accept_queue, &accept_queue };
    bool accepted = false;
    bool added_to_list = false;
    list_entry accepted_le;
    bool on_accepted_list = false;
    atomic<bool> dead = false, shutdown_write = false, shutdown_read = false;
    bool ipv6_only = false;
    bool present_as_ipv6 = false;
};

template<socket_family FAMILY>
class tcp_socket : public base_tcp_socket {
public:
    tcp_socket();
    ~tcp_socket();

    void last_handle_closed() override;

    nt_status recvfrom(span<const span<uint8_t>> v, int flags,
                       sockaddr_storage* from, size_t* fromlen,
                       uint64_t& recv_bytes, span<HANDLE>, size_t&) override {
        return do_recvfrom<false>(v, flags, from, fromlen, recv_bytes);
    }

    nt_status sendto(span<const span<const uint8_t>> v, int flags, sockaddr* to,
                     size_t tolen, uint64_t& sent_bytes, span<const HANDLE> handles) override {
        if (!handles.empty())
            return nt_status::NOT_SUPPORTED;

        return do_sendto<false>(v, flags, to, tolen, sent_bytes);
    }
    nt_status bind(const sockaddr* addr, size_t addrlen) override;
    nt_status connect(const sockaddr* addr, size_t addrlen) override;
    nt_status listen(int backlog) override;
    nt_status accept(HANDLE& sock, void* addr, int& addrlen) override;
    nt_status getsockname(span<uint8_t>, size_t&) override;
    nt_status getpeername(span<uint8_t> buf, size_t& len) override;

    nt_status recvfrom_nonblock(span<const span<uint8_t>> v, int flags,
                                sockaddr_storage* from, size_t* fromlen,
                                uint64_t& recv_bytes, span<HANDLE>, size_t&) override {
        return do_recvfrom<true>(v, flags, from, fromlen, recv_bytes);
    }

    nt_status sendto_nonblock(span<const span<const uint8_t>> v, int flags,
                              sockaddr* to, size_t tolen,
                              uint64_t& sent_bytes, span<const HANDLE> handles) override {
        if (!handles.empty())
            return nt_status::NOT_SUPPORTED;

        return do_sendto<true>(v, flags, to, tolen, sent_bytes);
    }

    nt_status setsockopt_tcp(sockopt_tcp opt, span<const uint8_t> buf) override;
    nt_status setsockopt_ipv6(sockopt_ipv6 opt, span<const uint8_t> buf) override;
    nt_status shutdown(int how) override;
    nt_status getsockopt_tcp(sockopt_tcp opt, span<uint8_t> buf,
                             socklen_t& retlen) override;
    nt_status getsockopt_ipv6(sockopt_ipv6 opt, span<uint8_t> buf,
                              socklen_t& retlen) override;

    void received_packet_ipv4(packet& pack, netif& ni, const mac_address& src_mac);
    nt_status send_syn_ack(const mac_address& mac);
    void received_listen_packet(packet& pack, netif& ni,
                                const mac_address& src_mac);

    typename FAMILY::address bound_address, connected_address;

private:
    void retransmission_thread();
    void do_retransmissions();

    template<bool NONBLOCK>
    nt_status do_recvfrom(span<const span<uint8_t>> v, int flags,
                          sockaddr_storage* from, size_t* fromlen,
                          uint64_t& recv_bytes);

    template<bool NONBLOCK>
    nt_status do_sendto(span<const span<const uint8_t>> v, int flags,
                        sockaddr* to, size_t tolen, uint64_t& sent_bytes);
};

class tcp_socket_packet : public socket_packet {
public:
    tcp_socket_packet(packet& pack) : socket_packet(pack) { }

    size_t skip;
};

template<socket_family FAMILY>
struct socket_variant;

template<>
struct socket_variant<ipv4> {
    using type = variant<tcp_socket<ipv4>*>;
    using parent_type = variant<tcp_socket<ipv4>*, tcp_socket<ipv6>*>;
};

template<>
struct socket_variant<ipv6> {
    using type = variant<tcp_socket<ipv4>*, tcp_socket<ipv6>*>;
    using parent_type = variant<tcp_socket<ipv6>*>;
};

static spinlock tcp_ports_lock;
static base_tcp_socket* tcp_ports_v4[65536];
static tcp_socket<ipv6>* tcp_ports_v6[65536];
static list_entry accepted_sockets_v4 = { &accepted_sockets_v4, &accepted_sockets_v4 };
static list_entry accepted_sockets_v6 = { &accepted_sockets_v6, &accepted_sockets_v6 };

struct tcp_header {
    uint16_t src_port;
    uint16_t dst_port;
    tcp_seq seq;
    tcp_seq ack;
    uint8_t : 4;
    uint8_t header_length : 4;
    union {
        struct {
            uint8_t FIN : 1;
            uint8_t SYN : 1;
            uint8_t RST : 1;
            uint8_t PSH : 1;
            uint8_t ACK : 1;
            uint8_t URG : 1;
            uint8_t ECE : 1;
            uint8_t CWR : 1;
        };
        uint8_t flags;
    };
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent_ptr;
};

static_assert(sizeof(tcp_header) == 20);

static const uint8_t TCP_SYN = 0x02;
static const uint8_t TCP_SYN_ACK = 0x12;

template<socket_family FAMILY>
static void received_packet(base_tcp_socket& sock, packet& pack,
                            netif& ni, const mac_address& src_mac);

template<socket_family FAMILY>
static void send_tcp_port_closed(packet& pack, netif& ni,
                                 const mac_address& mac) {
    if constexpr (is_same_v<FAMILY, ipv4>) {
        // FIXME
    } else if constexpr (is_same_v<FAMILY, ipv6>) {
        auto pack2 = new packet;
        if (!pack2) {
            print("new failed\n");
            return;
        }

        auto& orig_ip = *(ipv6_header*)pack.data;
        auto& orig_tcp = *(tcp_header*)(pack.data + sizeof(ipv6_header));

        pack2->size = sizeof(ipv6_header) + sizeof(tcp_header);

        auto& ip = *(ipv6_header*)pack2->data;
        ip.traffic_class1 = 0;
        ip.version = 6;
        ip.traffic_class2 = 0;
        ip.flow_label_high = 0;
        ip.flow_label_medium = 0;
        ip.flow_label_low = 0;
        ip.payload_length = byteswap((uint16_t)sizeof(tcp_header));
        ip.next_header = IPPROTO_TCP;
        ip.hop_limit = DEFAULT_HOP_LIMIT;
        ip.src = orig_ip.dst; // FIXME - make sure not multicast?
        ip.dst = orig_ip.src;

        auto& tcp = *(tcp_header*)(pack2->data + sizeof(ipv6_header));

        memset(&tcp, 0, sizeof(tcp));

        tcp.src_port = orig_tcp.dst_port;
        tcp.dst_port = orig_tcp.src_port;

        if (orig_tcp.ACK)
            tcp.seq = orig_tcp.ack;
        else {
            auto len = (uint32_t)(pack.size - sizeof(ipv6_header) - (orig_tcp.header_length * sizeof(uint32_t)));

            if (orig_tcp.SYN)
                len++;

            if (orig_tcp.FIN)
                len++;

            tcp.ack = orig_tcp.seq + len;
            tcp.ACK = 1;
        }

        tcp.header_length = sizeof(tcp_header) / sizeof(uint32_t);
        tcp.RST = 1;

        if (!ni.tx_tcpv6_csum_offload) {
            tcp.checksum = calc_ipv6_checksum(ip, span((uint8_t*)&tcp, pack2->size - sizeof(ipv6_header)),
                                              IPPROTO_TCP);
        } else {
            tcp.checksum = calc_ipv6_checksum_offload(ip, (uint32_t)(pack2->size - sizeof(ipv6_header)),
                                                      IPPROTO_TCP);
        }

        auto status = ni.send_packet(mac, ethertype::ipv6, *pack2);
        if (!success(status))
            print("netif::send_packet returned {}\n", status);

        pack2->release();
    }
}

void received_ipv4_tcp_packet(netif& ni, packet& pack,
                              const mac_address& src_mac, bool skip_checksum) {
    if (pack.size < sizeof(ipv4_header))
        return;

    auto& ip = *(ipv4_header*)pack.data;
    auto ip_header_size = ip.ihl * sizeof(uint32_t);

    if (ip_header_size < sizeof(ipv4_header))
        return;

    if (pack.size < ip_header_size + sizeof(tcp_header))
        return;

    if (is_multicast_address(ip.dst))
        return;

    auto& tcp = *(tcp_header*)(pack.data + ip_header_size);

    if (tcp.header_length * sizeof(uint32_t) < sizeof(tcp_header))
        return;

    if (pack.size < ip_header_size + (tcp.header_length * sizeof(uint32_t)))
        return;

    if (!skip_checksum) {
        if (calc_ipv4_checksum(ip, span((uint8_t*)&tcp, byteswap((uint16_t)ip.total_length) - ip_header_size), IPPROTO_TCP) != 0)
            return;
    }

#if 0
    print("received_tcp_packet (src {}, dst {}, {:04x})\n",
          byteswap(tcp.src_port), byteswap(tcp.dst_port), byteswap(tcp.checksum));
#endif

    tcp_socket<ipv4>* sock = nullptr;
    tcp_socket<ipv6>* sock6 = nullptr;

    {
        lock_guard lock(tcp_ports_lock);

        for (auto le = accepted_sockets_v4.next; le != &accepted_sockets_v4; le = le->next) {
            auto& c = *(tcp_socket<ipv4>*)((uint8_t*)le - offsetof(tcp_socket<ipv4>, accepted_le));

            if (c.bound_address == ip.dst && c.bound_port == tcp.dst_port &&
                c.connected_address == ip.src && c.connected_port == tcp.src_port) {
                sock = &c;
                sock->acquire();
                break;
            }
        }

        if (!sock && tcp_ports_v4[tcp.dst_port]) {
            if (tcp_ports_v4[tcp.dst_port]->is_ipv4) {
                sock = static_cast<tcp_socket<ipv4>*>(tcp_ports_v4[tcp.dst_port]);

                {
                    lock_guard lock(sock->state_lock);

                    if (!sock->bind_done || (sock->bound_address != parse_ipv4_address("0.0.0.0") && sock->bound_address != ip.dst))
                        sock = nullptr;
                    else
                        sock->acquire();
                }
            } else {
                sock6 = static_cast<tcp_socket<ipv6>*>(tcp_ports_v4[tcp.dst_port]);

                {
                    lock_guard lock(sock6->state_lock);

                    if (!sock6->bind_done || (sock6->bound_address != parse_ipv6_address("::") && sock6->bound_address != map_ipv4_address_to_ipv6(ip.dst)))
                        sock6 = nullptr;
                    else
                        sock6->acquire();
                }
            }
        }
    }

    if (!sock && !sock6) {
        if (!tcp.RST)
            send_tcp_port_closed<ipv4>(pack, ni, src_mac);
        return;
    } else if (sock) {
        received_packet<ipv4>(*sock, pack, ni, src_mac);

        sock->release();
    } else if (sock6) {
        received_packet<ipv4>(*sock6, pack, ni, src_mac);

        sock6->release();
    }
}

void received_ipv6_tcp_packet(netif& ni, packet& pack,
                              const mac_address& src_mac, bool skip_checksum) {
    if (pack.size < sizeof(ipv6_header) + sizeof(tcp_header))
        return;

    auto& ip = *(ipv6_header*)pack.data;

    if (is_multicast_address(ip.dst))
        return;

    auto& tcp = *(tcp_header*)(pack.data + sizeof(ipv6_header));

    if (!skip_checksum) {
        if (calc_ipv6_checksum(ip, span((uint8_t*)&tcp, pack.size - sizeof(ipv6_header)), IPPROTO_TCP) != 0)
            return;
    }

#if 0
    print("received_tcp_packet (src {}, dst {}, {:04x})\n",
          byteswap(tcp.src_port), byteswap(tcp.dst_port), byteswap(tcp.checksum));
#endif

    tcp_socket<ipv6>* sock = nullptr;

    {
        lock_guard lock(tcp_ports_lock);

        for (auto le = accepted_sockets_v6.next; le != &accepted_sockets_v6; le = le->next) {
            auto& c = *(tcp_socket<ipv6>*)((uint8_t*)le - offsetof(tcp_socket<ipv6>, accepted_le));

            if (c.bound_address == ip.dst && c.bound_port == tcp.dst_port &&
                c.connected_address == ip.src && c.connected_port == tcp.src_port) {
                sock = &c;
                sock->acquire();
                break;
            }
        }

        if (!sock && tcp_ports_v6[tcp.dst_port]) {
            sock = tcp_ports_v6[tcp.dst_port];

            {
                lock_guard lock(sock->state_lock);

                if (!sock->bind_done || (sock->bound_address != parse_ipv6_address("::") && sock->bound_address != ip.dst))
                    sock = nullptr;
                else
                    sock->acquire();
            }
        }
    }

    if (!sock) {
        if (!tcp.RST)
            send_tcp_port_closed<ipv6>(pack, ni, src_mac);
        return;
    }

    received_packet<ipv6>(*sock, pack, ni, src_mac);

    sock->release();
}

template<socket_family FAMILY>
static uint16_t find_ephemeral_tcp_port(bool dual_stack) {
    auto rnd = kernel_time_page.system_time / (SYSTEM_TIME_RESOLUTION / TICK_RATE);
    uint16_t port = 49152 + (rnd & 0x3fff);
    uint16_t orig_port = port;

    while (true) {
        if (dual_stack) {
            if (!tcp_ports_v4[byteswap(port)] && !tcp_ports_v6[byteswap(port)])
                return byteswap(port);
        } else {
            if constexpr (is_same_v<FAMILY, ipv4>) {
                if (!tcp_ports_v4[byteswap(port)])
                    return byteswap(port);
            } else if constexpr (is_same_v<FAMILY, ipv6>) {
                if (!tcp_ports_v6[byteswap(port)])
                    return byteswap(port);
            }
        }

        if (port == 65535)
            port = 49152;
        else
            port++;

        if (port == orig_port)
            return 0;
    }
}

static uint16_t tcp_window(base_tcp_socket& sock) {
    if (sock.recv_window < 0)
        return 0;
    else
        return byteswap((uint16_t)sock.recv_window);
}

template<socket_family FAMILY>
static nt_status send_syn(base_tcp_socket& sock,
                          const typename FAMILY::sockaddr& c,
                          typename FAMILY::address& src) {
    nt_status status;

    typename FAMILY::address dst, gateway;

    if constexpr (is_same_v<FAMILY, ipv4>)
        dst = *(typename FAMILY::address*)&c.sin_addr;
    else if constexpr (is_same_v<FAMILY, ipv6>)
        dst = *(typename FAMILY::address*)&c.sin6_addr;

    {
        netif* newni;

        if constexpr (is_same_v<FAMILY, ipv4>)
            status = ipv4_choose_interface(dst, newni, src, gateway);
        else if constexpr (is_same_v<FAMILY, ipv6>)
            status = ipv6_choose_interface(dst, c.sin6_scope_id, newni, src);

        if (!success(status))
            return status;

        {
            lock_guard lock(sock.state_lock);

            swap(sock.pni, newni);
        }

        if (newni)
            newni->release();
    }

    {
        lock_guard lock(sock.state_lock);

        if constexpr (is_same_v<FAMILY, ipv4>) {
            if (!sock.is_ipv4) {
                auto& s = static_cast<tcp_socket<ipv6>&>(sock);

                if (s.bound_address == parse_ipv6_address("::") && src != parse_ipv4_address("0.0.0.0"))
                    s.bound_address = map_ipv4_address_to_ipv6(src);
            } else {
                auto& s = static_cast<tcp_socket<ipv4>&>(sock);

                if (s.bound_address == parse_ipv4_address("0.0.0.0"))
                    s.bound_address = src;
            }
        } else if constexpr (is_same_v<FAMILY, ipv6>) {
            auto& s = static_cast<tcp_socket<ipv6>&>(sock);

            if (s.bound_address == parse_ipv6_address("::"))
                s.bound_address = src;
        }
    }

    auto& ni = *sock.pni;

    if (src == dst)
        sock.dst_mac = ni.mac;
    else {
        if (ni.loopback)
            sock.dst_mac = NULL_MAC_ADDRESS;
        else {
            if constexpr (is_same_v<FAMILY, ipv4>)
                sock.dst_mac = find_mac_address(ni, gateway == parse_ipv4_address("0.0.0.0") ? dst : gateway);
            else if constexpr (is_same_v<FAMILY, ipv6>)
                sock.dst_mac = find_mac_address(ni, dst);
        }
    }

    ni.acquire();

    auto pack = new packet;
    if (!pack) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    pack->size = sizeof(typename FAMILY::header) + sizeof(tcp_header);

    if constexpr (is_same_v<FAMILY, ipv4>) {
        auto& ip = *(ipv4_header*)pack->data;

        ip.ihl = sizeof(ipv4_header) / sizeof(uint32_t);
        ip.version = 4;
        ip.ecn = 0;
        ip.dscp = 0;
        ip.total_length = byteswap((uint16_t)(sizeof(ipv4_header) + sizeof(tcp_header)));
        ip.identification = 0;
        ip.fragment_offset1 = 0;
        ip.MF = 0;
        ip.DF = 1;
        ip.R = 0;
        ip.fragment_offset2 = 0;
        ip.ttl = DEFAULT_HOP_LIMIT;
        ip.protocol = IPPROTO_TCP;
        ip.header_checksum = 0;
        ip.src = src;
        ip.dst = dst;

        ip.header_checksum = calc_ipv4_checksum(span(pack->data, sizeof(ipv4_header)));
    } else if constexpr (is_same_v<FAMILY, ipv6>) {
        auto& ip = *(ipv6_header*)pack->data;

        ip.traffic_class1 = 0;
        ip.version = 6;
        ip.traffic_class2 = 0;
        ip.flow_label_high = 0;
        ip.flow_label_medium = 0;
        ip.flow_label_low = 0;
        ip.payload_length = byteswap((uint16_t)sizeof(tcp_header));
        ip.next_header = IPPROTO_TCP;
        ip.hop_limit = DEFAULT_HOP_LIMIT;
        ip.src = src;
        ip.dst = dst;
    }

    auto& tcp = *(tcp_header*)(pack->data + sizeof(typename FAMILY::header));

    memset(&tcp, 0, sizeof(tcp));

    tcp.src_port = sock.bound_port;
    tcp.dst_port = sock.connected_port;
    tcp.seq = sock.send_next;
    tcp.header_length = sizeof(tcp_header) / sizeof(uint32_t);
    tcp.SYN = 1;
    tcp.window = tcp_window(sock);

    if constexpr (is_same_v<FAMILY, ipv4>) {
        auto& ip = *(ipv4_header*)pack->data;

        tcp.checksum = calc_ipv4_checksum(ip, span((uint8_t*)&tcp, pack->size - sizeof(ipv4_header)),
                                          IPPROTO_TCP);
    } else if constexpr (is_same_v<FAMILY, ipv6>) {
        auto& ip = *(ipv6_header*)pack->data;

        if (!ni.tx_tcpv6_csum_offload) {
            tcp.checksum = calc_ipv6_checksum(ip, span((uint8_t*)&tcp, pack->size - sizeof(ipv6_header)),
                                              IPPROTO_TCP);
        } else {
            tcp.checksum = calc_ipv6_checksum_offload(ip, (uint32_t)(pack->size - sizeof(ipv6_header)),
                                                      IPPROTO_TCP);
        }
    }

    if (sock.dst_mac == NULL_MAC_ADDRESS && !ni.loopback) {
        send_address_unreachable(ni, *pack, src);

        pack->release();
        ni.release();
        return nt_status::HOST_UNREACHABLE;
    }

    auto& ev = static_cast<event&>(sock.state_change_event.get());

    for (unsigned int i = 0; i < 3; i++) {
        status = dispatch_packet<FAMILY>(ni, *pack, sock.dst_mac);
        if (!success(status)) {
            pack->release();
            ni.release();
            return status;
        }

        status = wait_for_object(ev, true, SYSTEM_TIME_RESOLUTION); // wait 1 second

        if (status == nt_status::TIMEOUT)
            continue;
        else if (success(status) && status != nt_status::USER_APC && status != nt_status::ALERTED)
            break;
        else {
            pack->release();
            ni.release();
            return status;
        }
    }

    pack->release();
    ni.release();

    {
        lock_guard lock(sock.state_lock);

        if (sock.state == tcp_state::CLOSED)
            return nt_status::CONNECTION_REFUSED;
    }

    if (status == nt_status::TIMEOUT)
        return nt_status::HOST_UNREACHABLE;

    return nt_status::SUCCESS;
}

static tcp_seq rand_tcp_seq() {
    auto rnd = (uint64_t)(kernel_time_page.system_time / (SYSTEM_TIME_RESOLUTION / TICK_RATE));

    rnd = rnd * (uint64_t)1103515245 + 12345; // from Knuth

    return tcp_seq{(uint32_t)(rnd >> 16)};
}

template<socket_family FAMILY>
nt_status tcp_socket<FAMILY>::connect(const sockaddr* addr, size_t addrlen) {
    nt_status status;
    bool mapped_address = false;

    if (addr->sa_family != FAMILY::family)
        return nt_status::INVALID_PARAMETER;

    if (addrlen < sizeof(typename FAMILY::sockaddr))
        return nt_status::BUFFER_TOO_SMALL;

    auto& c = *(typename FAMILY::sockaddr*)addr;
    typename FAMILY::address src;

    if constexpr (is_same_v<FAMILY, ipv6>) {
        mapped_address = is_mapped_address(*(ipv6_address*)&c.sin6_addr);

        if (ipv6_only && mapped_address)
            return nt_status::INVALID_PARAMETER;
    }

    status = nt_status::SUCCESS;

    {
        uint16_t port;
        lock_guard lock(tcp_ports_lock);
        lock_guard lock2(state_lock);

        if (state != tcp_state::CLOSED)
            return nt_status::INVALID_PARAMETER;

        if (!bind_done) { // implicit bind if not already done
            if constexpr (is_same_v<FAMILY, ipv4>) {
                port = find_ephemeral_tcp_port<FAMILY>(false);
                if (port == 0) // no ephemeral ports left
                    return nt_status::ADDRESS_ALREADY_ASSOCIATED;

                tcp_ports_v4[port] = this;
            } else if constexpr (is_same_v<FAMILY, ipv6>) {
                if (mapped_address) {
                    port = find_ephemeral_tcp_port<ipv4>(false);
                    if (port == 0) // no ephemeral ports left
                        return nt_status::ADDRESS_ALREADY_ASSOCIATED;

                    tcp_ports_v4[port] = this;
                } else {
                    port = find_ephemeral_tcp_port<FAMILY>(false);
                    if (port == 0) // no ephemeral ports left
                        return nt_status::ADDRESS_ALREADY_ASSOCIATED;

                    tcp_ports_v6[port] = this;
                }
            }

            this->acquire();

            bound_port = port;

            if constexpr (is_same_v<FAMILY, ipv4>)
                bound_address = parse_ipv4_address("0.0.0.0");
            else if constexpr (is_same_v<FAMILY, ipv6>)
                bound_address = parse_ipv6_address("::");

            bind_done = true;
        } else {
            if constexpr (is_same_v<FAMILY, ipv6>) {
                if (mapped_address && !is_mapped_address(bound_address))
                    return nt_status::INVALID_PARAMETER;
            }
        }

        state = tcp_state::SYN_SENT;

        if constexpr (is_same_v<FAMILY, ipv4>) {
            connected_port = c.sin_port;
            connected_address = *(ipv4_address*)&c.sin_addr;
        } else if constexpr (is_same_v<FAMILY, ipv6>) {
            connected_port = c.sin6_port;
            connected_address = *(ipv6_address*)&c.sin6_addr;
            connected_scope = c.sin6_scope_id;
        }

        src = bound_address;

        send_next = rand_tcp_seq();
        send_unacknowledged = send_next;
    }

    if constexpr (is_same_v<FAMILY, ipv4>)
        status = send_syn<FAMILY>(*this, c, src);
    else if constexpr (is_same_v<FAMILY, ipv6>) {
        if (mapped_address) {
            sockaddr_in c2;
            ipv4_address src2;

            if (src == parse_ipv6_address("::"))
                src2 = parse_ipv4_address("0.0.0.0");
            else
                src2 = extract_mapped_address(src);

            memset(&c2, 0, sizeof(c2));

            c2.sin_family = AF_INET;
            c2.sin_port = c.sin6_port;
            *(ipv4_address*)&c2.sin_addr = extract_mapped_address(connected_address);

            mss = MSS_IPv4;

            status = send_syn<ipv4>(*this, c2, src2);
        } else {
            mss = MSS_IPv6;

            status = send_syn<FAMILY>(*this, c, src);
        }
    }

    if (!success(status)) {
        netif* oldni = nullptr;

        // FIXME - should also undo bind?

        {
            lock_guard lock(state_lock);
            state = tcp_state::CLOSED;
            swap(oldni, pni);
        }

        if (oldni)
            oldni->release();

        return status;
    }

    return nt_status::SUCCESS;
}

template<socket_family FAMILY>
static void reset_child_socket(tcp_socket<FAMILY>& s) {
    auto pack2 = new packet;
    if (!pack2) {
        print("new failed\n");
        return;
    }

    pack2->size = sizeof(typename FAMILY::header) + sizeof(tcp_header);

    if constexpr (is_same_v<FAMILY, ipv4>) {
        auto& ip = *(ipv4_header*)pack2->data;

        ip.ihl = sizeof(ipv4_header) / sizeof(uint32_t);
        ip.version = 4;
        ip.ecn = 0;
        ip.dscp = 0;
        ip.total_length = byteswap((uint16_t)(sizeof(ipv4_header) + sizeof(tcp_header)));
        ip.identification = 0;
        ip.fragment_offset1 = 0;
        ip.MF = 0;
        ip.DF = 1;
        ip.R = 0;
        ip.fragment_offset2 = 0;
        ip.ttl = DEFAULT_HOP_LIMIT;
        ip.protocol = IPPROTO_TCP;
        ip.header_checksum = 0;
        ip.src = s.bound_address;
        ip.dst = s.connected_address;

        ip.header_checksum = calc_ipv4_checksum(span(pack2->data, sizeof(ipv4_header)));
    } else if constexpr (is_same_v<FAMILY, ipv6>) {
        auto& ip = *(ipv6_header*)pack2->data;

        ip.traffic_class1 = 0;
        ip.version = 6;
        ip.traffic_class2 = 0;
        ip.flow_label_high = 0;
        ip.flow_label_medium = 0;
        ip.flow_label_low = 0;
        ip.payload_length = byteswap((uint16_t)sizeof(tcp_header));
        ip.next_header = IPPROTO_TCP;
        ip.hop_limit = DEFAULT_HOP_LIMIT;
        ip.src = s.bound_address; // FIXME - make sure not multicast?
        ip.dst = s.connected_address;
    }

    auto& tcp = *(tcp_header*)(pack2->data + sizeof(typename FAMILY::header));

    memset(&tcp, 0, sizeof(tcp));

    tcp.src_port = s.bound_port;
    tcp.dst_port = s.connected_port;

    tcp.seq = s.send_next;
    tcp.ack = s.recv_next;

    tcp.header_length = sizeof(tcp_header) / sizeof(uint32_t);
    tcp.RST = 1;
    tcp.ACK = 1;

    if constexpr (is_same_v<FAMILY, ipv4>) {
        auto& ip = *(ipv4_header*)pack2->data;

        tcp.checksum = calc_ipv4_checksum(ip, span((uint8_t*)&tcp, pack2->size - sizeof(ipv4_header)),
                                          IPPROTO_TCP);
    } else if constexpr (is_same_v<FAMILY, ipv6>) {
        auto& ip = *(ipv6_header*)pack2->data;

        if (!s.pni->tx_tcpv6_csum_offload) {
            tcp.checksum = calc_ipv6_checksum(ip, span((uint8_t*)&tcp, pack2->size - sizeof(ipv6_header)),
                                              IPPROTO_TCP);
        } else {
            tcp.checksum = calc_ipv6_checksum_offload(ip, (uint32_t)(pack2->size - sizeof(ipv6_header)),
                                                      IPPROTO_TCP);
        }
    }

    auto status = s.pni->send_packet(s.dst_mac, FAMILY::ethertype, *pack2);
    if (!success(status))
        print("netif::send_packet returned {}\n", status);

    pack2->release();
}

template<socket_family FAMILY>
void tcp_socket<FAMILY>::last_handle_closed() {
    acquire();

    add_kernel_thread([](void* ctx) {
        auto& s = *(tcp_socket<FAMILY>*)ctx;

        bool do_fin = false;

        {
            lock_guard lock(s.state_lock);

            if (s.state == tcp_state::ESTABLISHED) {
                s.state = tcp_state::FIN_WAIT_1;
                do_fin = true;
            } else if (s.state == tcp_state::CLOSE_WAIT) {
                s.state = tcp_state::LAST_ACK;
                do_fin = true;
            }
        }

        if (do_fin) {
            if constexpr (is_same_v<FAMILY, ipv4>)
                send_fin<FAMILY>(s);
            else if constexpr (is_same_v<FAMILY, ipv6>) {
                if (is_mapped_address(s.connected_address))
                    send_fin<ipv4>(s);
                else
                    send_fin<FAMILY>(s);
            }
        }

        while (true) {
            base_tcp_socket* s2 = nullptr;

            {
                lock_guard lock(s.list_lock);

                if (!is_list_empty(s.syn_queue)) {
                    s2 = &containing_record<base_tcp_socket>(s.syn_queue.next);
                    remove_entry_list(s2->le);
                } else if (!is_list_empty(s.accept_queue)) {
                    s2 = &containing_record<base_tcp_socket>(s.accept_queue.next);
                    remove_entry_list(s2->le);
                }
            }

            if (!s2)
                break;

            typename socket_variant<FAMILY>::type v;

            if constexpr (is_same_v<FAMILY, ipv4>)
                v = static_cast<tcp_socket<ipv4>*>(s2);
            else if constexpr (is_same_v<FAMILY, ipv6>) {
                if (s2->is_ipv4)
                    v = static_cast<tcp_socket<ipv4>*>(s2);
                else
                    v = static_cast<tcp_socket<ipv6>*>(s2);
            }

            visit([&](auto* s2) {
                reset_child_socket(*s2);

                s2->release();
            }, v);
        }

        if (do_fin && !s.parent) {
            wait_for_object(s.retransmission_thread_quit_event.get(), false,
                            60 * SYSTEM_TIME_RESOLUTION);
        }

        if (!s.parent) {
            lock_guard lock(tcp_ports_lock);
            lock_guard lock2(s.state_lock);

            if (s.bind_done && tcp_ports_v4[s.bound_port] == &s) {
                tcp_ports_v4[s.bound_port] = nullptr;
                s.release();
            }

            if constexpr (is_same_v<FAMILY, ipv6>) {
                if (s.bind_done && tcp_ports_v6[s.bound_port] == &s) {
                    tcp_ports_v6[s.bound_port] = nullptr;
                    s.release();
                }
            }
        }

        s.dead = true;

        {
            sync_lock sl;
            lock_guard lock(sl);

            if (!s.signalled) {
                wake_up_waiters(s.waiters, WAIT_TYPE_READING);
                s.signalled = true;
            }

            auto& e = static_cast<event&>(s.send_window_open_event.get());
            e.set_holding_sync_lock();

            if (!s.write_signalled) {
                s.write_signalled = true;
                wake_up_waiters(s.waiters, WAIT_TYPE_WRITING);
            }
        }

        if (s.accept_queue_event)
            set_event(static_cast<event&>(s.accept_queue_event.get()));

        s.release();
    }, this);
}

template<socket_family FAMILY>
static void send_ack(base_tcp_socket& sock, bool first, tcp_seq ack) {
    auto pack = new packet;
    if (!pack) {
        print("new failed\n");
        return;
    }

    auto& ni = *sock.pni;

    ni.acquire();

    pack->size = sizeof(typename FAMILY::header) + sizeof(tcp_header);

    if constexpr (is_same_v<FAMILY, ipv4>) {
        auto& ip = *(ipv4_header*)pack->data;

        ip.ihl = sizeof(ipv4_header) / sizeof(uint32_t);
        ip.version = 4;
        ip.ecn = 0;
        ip.dscp = 0;
        ip.total_length = byteswap((uint16_t)(sizeof(ipv4_header) + sizeof(tcp_header)));
        ip.identification = 0;
        ip.fragment_offset1 = 0;
        ip.MF = 0;
        ip.DF = 1;
        ip.R = 0;
        ip.fragment_offset2 = 0;
        ip.ttl = DEFAULT_HOP_LIMIT;
        ip.protocol = IPPROTO_TCP;
        ip.header_checksum = 0;

        if (sock.is_ipv4) {
            auto& s2 = static_cast<tcp_socket<ipv4>&>(sock);

            ip.src = s2.bound_address;
            ip.dst = s2.connected_address;
        } else {
            auto& s2 = static_cast<tcp_socket<ipv6>&>(sock);

            ip.src = extract_mapped_address(s2.bound_address);
            ip.dst = extract_mapped_address(s2.connected_address);
        }

        ip.header_checksum = calc_ipv4_checksum(span(pack->data, sizeof(ipv4_header)));
    } else if constexpr (is_same_v<FAMILY, ipv6>) {
        auto& ip = *(ipv6_header*)pack->data;
        auto& s2 = static_cast<tcp_socket<ipv6>&>(sock);

        ip.traffic_class1 = 0;
        ip.version = 6;
        ip.traffic_class2 = 0;
        ip.flow_label_high = 0;
        ip.flow_label_medium = 0;
        ip.flow_label_low = 0;
        ip.payload_length = byteswap((uint16_t)sizeof(tcp_header));
        ip.next_header = IPPROTO_TCP;
        ip.hop_limit = DEFAULT_HOP_LIMIT;
        ip.src = s2.bound_address;
        ip.dst = s2.connected_address;
    }

    auto& tcp = *(tcp_header*)(pack->data + sizeof(typename FAMILY::header));

    memset(&tcp, 0, sizeof(tcp));

    {
        lock_guard lock(sock.io_lock);

        if (first)
            sock.send_next++;

        tcp.src_port = sock.bound_port;
        tcp.dst_port = sock.connected_port;
        tcp.seq = sock.send_next;
        tcp.ack = ack;
        tcp.header_length = sizeof(tcp_header) / sizeof(uint32_t);
        tcp.ACK = 1;
        tcp.window = tcp_window(sock);

        if constexpr (is_same_v<FAMILY, ipv4>) {
            auto& ip = *(ipv4_header*)pack->data;

            tcp.checksum = calc_ipv4_checksum(ip, span((uint8_t*)&tcp, pack->size - sizeof(ipv4_header)),
                                              IPPROTO_TCP);
        } else if constexpr (is_same_v<FAMILY, ipv6>) {
            auto& ip = *(ipv6_header*)pack->data;

            if (!ni.tx_tcpv6_csum_offload) {
                tcp.checksum = calc_ipv6_checksum(ip, span((uint8_t*)&tcp, pack->size - sizeof(ipv6_header)),
                                                  IPPROTO_TCP);
            } else {
                tcp.checksum = calc_ipv6_checksum_offload(ip, (uint32_t)(pack->size - sizeof(ipv6_header)),
                                                          IPPROTO_TCP);
            }
        }

        if (first) {
            pack->acquire();
            sock.first_ack = pack;
        } else if (sock.first_ack) {
            sock.first_ack->release();
            sock.first_ack = nullptr;
        }
    }

    dispatch_packet<FAMILY>(ni, *pack, sock.dst_mac);

    pack->release();
    ni.release();
}

template<socket_family FAMILY>
static void send_fin(base_tcp_socket& sock) {
    // FIXME - send RST if new fails?

    auto pack = new packet;
    if (!pack) {
        print("new failed\n");
        return;
    }

    auto r = new tcp_retransmission(*pack);
    if (!r) {
        print("new failed\n");
        pack->release();
        return;
    }

    auto& ni = *sock.pni;

    ni.acquire();

    pack->size = sizeof(typename FAMILY::header) + sizeof(tcp_header);

    if constexpr (is_same_v<FAMILY, ipv4>) {
        auto& ip = *(ipv4_header*)pack->data;

        ip.ihl = sizeof(ipv4_header) / sizeof(uint32_t);
        ip.version = 4;
        ip.ecn = 0;
        ip.dscp = 0;
        ip.total_length = byteswap((uint16_t)(sizeof(ipv4_header) + sizeof(tcp_header)));
        ip.identification = 0;
        ip.fragment_offset1 = 0;
        ip.MF = 0;
        ip.DF = 1;
        ip.R = 0;
        ip.fragment_offset2 = 0;
        ip.ttl = DEFAULT_HOP_LIMIT;
        ip.protocol = IPPROTO_TCP;
        ip.header_checksum = 0;

        if (sock.is_ipv4) {
            auto& s2 = static_cast<tcp_socket<ipv4>&>(sock);

            ip.src = s2.bound_address;
            ip.dst = s2.connected_address;
        } else {
            auto& s2 = static_cast<tcp_socket<ipv6>&>(sock);

            ip.src = extract_mapped_address(s2.bound_address);
            ip.dst = extract_mapped_address(s2.connected_address);
        }

        ip.header_checksum = calc_ipv4_checksum(span(pack->data, sizeof(ipv4_header)));
    } else if constexpr (is_same_v<FAMILY, ipv6>) {
        auto& s2 = static_cast<tcp_socket<ipv6>&>(sock);
        auto& ip = *(ipv6_header*)pack->data;

        ip.traffic_class1 = 0;
        ip.version = 6;
        ip.traffic_class2 = 0;
        ip.flow_label_high = 0;
        ip.flow_label_medium = 0;
        ip.flow_label_low = 0;
        ip.payload_length = byteswap((uint16_t)sizeof(tcp_header));
        ip.next_header = IPPROTO_TCP;
        ip.hop_limit = DEFAULT_HOP_LIMIT;
        ip.src = s2.bound_address;
        ip.dst = s2.connected_address;
    }

    auto& tcp = *(tcp_header*)(pack->data + sizeof(typename FAMILY::header));

    memset(&tcp, 0, sizeof(tcp));

    {
        lock_guard lock(sock.io_lock);

        tcp.src_port = sock.bound_port;
        tcp.dst_port = sock.connected_port;
        tcp.seq = sock.send_next;
        tcp.ack = sock.recv_next;
        tcp.header_length = sizeof(tcp_header) / sizeof(uint32_t);
        tcp.FIN = 1;
        tcp.ACK = 1;
        tcp.window = tcp_window(sock);

        sock.send_next++;

        if constexpr (is_same_v<FAMILY, ipv4>) {
            auto& ip = *(ipv4_header*)pack->data;

            tcp.checksum = calc_ipv4_checksum(ip, span((uint8_t*)&tcp, pack->size - sizeof(ipv4_header)),
                                              IPPROTO_TCP);
        } else if constexpr (is_same_v<FAMILY, ipv6>) {
            auto& ip = *(ipv6_header*)pack->data;

            if (!ni.tx_tcpv6_csum_offload) {
                tcp.checksum = calc_ipv6_checksum(ip, span((uint8_t*)&tcp, pack->size - sizeof(ipv6_header)),
                                                  IPPROTO_TCP);
            } else {
                tcp.checksum = calc_ipv6_checksum_offload(ip, (uint32_t)(pack->size - sizeof(ipv6_header)),
                                                          IPPROTO_TCP);
            }
        }

        r->deadline = (uint64_t)kernel_time_page.system_time + (RETRANSMISSION_DELAY * SYSTEM_TIME_RESOLUTION);

        {
            lock_guard lock(sock.retransmission_list_lock);
            insert_tail_list(sock.retransmission_list, r->le);
        }
    }

    dispatch_packet<FAMILY>(ni, *pack, sock.dst_mac);

    pack->release();
    ni.release();
}

template<socket_family FAMILY>
typename socket_variant<FAMILY>::parent_type get_socket_parent(base_tcp_socket& sock) {
    if constexpr (is_same_v<FAMILY, ipv4>) {
        if (sock.parent->is_ipv4)
            return static_cast<tcp_socket<ipv4>*>(sock.parent);
        else
            return static_cast<tcp_socket<ipv6>*>(sock.parent);
    } else if constexpr (is_same_v<FAMILY, ipv6>)
        return static_cast<tcp_socket<ipv6>*>(sock.parent);
}

template<socket_family FAMILY>
void tcp_socket<FAMILY>::received_packet_ipv4(packet& pack, netif& ni,
                                              const mac_address& src_mac) {
    if constexpr (is_same_v<FAMILY, ipv4>)
        return;

    auto& ip = *(ipv4_header*)pack.data;
    size_t ip_header_size = ip.ihl * sizeof(uint32_t);

    auto& tcp = *(tcp_header*)(pack.data + ip_header_size);
    size_t header_length = tcp.header_length * sizeof(uint32_t);

    if (header_length < sizeof(tcp_header))
        return;

    size_t size = byteswap((uint16_t)ip.total_length);

    if (size < ip_header_size + header_length)
        return;

    {
        lock_guard lock(state_lock);

        if (state != tcp_state::LISTEN)
            return;
    }

    // FIXME - merge this code with received_listen_packet

    tcp_socket<ipv4>* found = nullptr;

    {
        lock_guard lock(list_lock);

        for (auto le = syn_queue.next; le != &syn_queue; le = le->next) {
            auto& s = containing_record<base_tcp_socket>(le);

            if (!s.is_ipv4)
                continue;

            auto& s2 = *static_cast<tcp_socket<ipv4>*>(&s);

            if (s2.connected_address == ip.src && s2.connected_port == tcp.src_port) {
                found = &s2;
                s2.acquire();
                break;
            }
        }

        if (!found) {
            for (auto le = accept_queue.next; le != &accept_queue; le = le->next) {
                auto& s = containing_record<base_tcp_socket>(le);

                if (!s.is_ipv4)
                    continue;

                auto& s2 = *static_cast<tcp_socket<ipv4>*>(&s);

                if (s2.connected_address == ip.src && s2.connected_port == tcp.src_port) {
                    found = &s2;
                    s2.acquire();
                    break;
                }
            }
        }
    }

    if (found) {
        received_packet<FAMILY>(*found, pack, ni, src_mac);
        found->release();
        return;
    }

    // FIXME - if receive SYN from something already on SYN queue, send SYN-ACK again (already handled by retransmissions?)

    // FIXME - also return RST if backlog exhausted
    if (tcp.flags != TCP_SYN) {
        send_tcp_port_closed<ipv4>(pack, ni, src_mac);
        return;
    }

    auto c = new tcp_socket<ipv4>;
    if (!c) {
        print("new failed\n");
        return;
    }

    if (!c->init) {
        print("tcp_socket init failed\n");
        delete c;
        return;
    }

    c->state = tcp_state::SYN_RECEIVED;
    c->bind_done = true;
    c->bound_address = ip.dst;
    c->bound_port = bound_port;
    c->connected_address = ip.src;
    c->connected_port = tcp.src_port;
    c->connected_scope = ni.num;
    c->recv_next = tcp.seq;

    c->send_next = rand_tcp_seq();
    c->send_unacknowledged = c->send_next;

    c->last_send_window_seq = tcp.seq;
    c->send_window = byteswap(tcp.window);

    if (c->allowed_send_bytes() > 0) {
        sync_lock sl;
        lock_guard lock(sl);

        auto& e = static_cast<event&>(c->send_window_open_event.get());
        e.set_holding_sync_lock();

        if (!c->write_signalled) {
            c->write_signalled = true;
            wake_up_waiters(c->waiters, WAIT_TYPE_WRITING);
        }
    }

    c->pni = &ni;
    ni.acquire();

    c->parent = this;
    this->acquire();

    bool done = false;

    {
        lock_guard lock(list_lock);

        if (listen_backlog > 0) {
            insert_tail_list(syn_queue, c->le);
            listen_backlog--;
            done = true;
        }
    }

    if (!done) {
        // syn_queue is full, drop
        set_event(static_cast<event&>(c->retransmission_thread_quit_event.get()));
        this->release();
        c->release();
        return;
    }

    c->added_to_list = true;
    c->present_as_ipv6 = true;

    c->send_syn_ack(src_mac);

    // FIXME - decrease backlog value

    // FIXME - free child socket if stays at SYN_RECEIVED too long
    // FIXME - increase backlog value when child goes away
}

template<socket_family FAMILY>
static void received_packet(base_tcp_socket& sock, packet& pack,
                            netif& ni, const mac_address& src_mac) {
    auto& ip = *(typename FAMILY::header*)pack.data;
    size_t ip_header_size;

    if constexpr (is_same_v<FAMILY, ipv4>)
        ip_header_size = ip.ihl * sizeof(uint32_t);
    else if constexpr (is_same_v<FAMILY, ipv6>)
        ip_header_size = sizeof(ipv6_header);

    auto& tcp = *(tcp_header*)(pack.data + ip_header_size);
    size_t header_length = tcp.header_length * sizeof(uint32_t);

    if (header_length < sizeof(tcp_header))
        return;

    size_t size;

    if constexpr (is_same_v<FAMILY, ipv4>)
        size = byteswap((uint16_t)ip.total_length);
    else if constexpr (is_same_v<FAMILY, ipv6>)
        size = pack.size;

    if (size < ip_header_size + header_length)
        return;

    tcp_state cur_state;

    {
        lock_guard lock(sock.state_lock);

        if (sock.state == tcp_state::CLOSED)
            return;

        if (sock.state != tcp_state::LISTEN) {
            if (sock.connected_port != tcp.src_port)
                return;

            if constexpr (is_same_v<FAMILY, ipv4>) {
                if (sock.is_ipv4) {
                    auto& s2 = static_cast<tcp_socket<ipv4>&>(sock);

                    if (s2.connected_address != ip.src)
                        return;
                } else {
                    auto& s2 = static_cast<tcp_socket<ipv6>&>(sock);

                    if (sock.ipv6_only || !is_mapped_address(s2.connected_address))
                        return;

                    if (ip.src != extract_mapped_address(s2.connected_address))
                        return;
                }
            } else if constexpr (is_same_v<FAMILY, ipv6>) {
                auto& s2 = static_cast<tcp_socket<ipv6>&>(sock);

                if (s2.connected_address != ip.src)
                    return;
            }
        }

        cur_state = sock.state;
    }

    if (cur_state == tcp_state::LISTEN) {
        if constexpr (is_same_v<FAMILY, ipv4>) {
            if (sock.is_ipv4) {
                auto& s2 = static_cast<tcp_socket<ipv4>&>(sock);

                s2.received_listen_packet(pack, ni, src_mac);
            } else {
                auto& s2 = static_cast<tcp_socket<ipv6>&>(sock);

                s2.received_packet_ipv4(pack, ni, src_mac);
            }
        } else if constexpr (is_same_v<FAMILY, ipv6>) {
            auto& s2 = static_cast<tcp_socket<ipv6>&>(sock);

            s2.received_listen_packet(pack, ni, src_mac);
        }

        return;
    }

    if (tcp.RST) {
        tcp_state old_state;
        bool was_accepted;

        {
            lock_guard lock(sock.state_lock);
            old_state = sock.state;
            sock.state = tcp_state::CLOSED;
            was_accepted = sock.accepted;
        }

        if (sock.parent) {
            visit([&](auto* parent) {
                sync_lock sl;
                lock_guard lock1(sl);
                lock_guard lock2(parent->list_lock);

                if (sock.added_to_list) {
                    remove_entry_list(sock.le);
                    sock.added_to_list = false;
                }

                if (old_state == tcp_state::SYN_RECEIVED || (old_state == tcp_state::ESTABLISHED && !was_accepted))
                    parent->listen_backlog++;

                if (is_list_empty(parent->accept_queue)) {
                    auto& e = static_cast<event&>(parent->accept_queue_event.get());
                    e.signalled = false;
                    parent->signalled = false;
                }
            }, get_socket_parent<FAMILY>(sock));

            if (sock.on_accepted_list) {
                {
                    lock_guard lock(tcp_ports_lock);
                    remove_entry_list(sock.accepted_le);
                }

                sock.on_accepted_list = false;
                sock.release();
            }

            if (old_state == tcp_state::SYN_RECEIVED || (old_state == tcp_state::ESTABLISHED && !was_accepted))
                sock.release();
        }

        set_event(static_cast<event&>(sock.state_change_event.get()));
        set_event(static_cast<event&>(sock.retransmission_thread_quit_event.get()));
        return;
    }

    {
        lock_guard lock(sock.io_lock);

        if ((cur_state == tcp_state::SYN_SENT && tcp.flags == TCP_SYN_ACK) || tcp.seq >= sock.last_send_window_seq) {
            sock.last_send_window_seq = tcp.seq;

            auto old_usable = sock.allowed_send_bytes();

            sock.send_window = byteswap(tcp.window);

            auto new_usable = sock.allowed_send_bytes();

            auto& e = static_cast<event&>(sock.send_window_open_event.get());

            {
                sync_lock sl;
                lock_guard lock2(sl);

                if (old_usable > 0 && new_usable == 0) {
                    e.signalled = false;
                    sock.write_signalled = false;
                } else if (old_usable == 0 && new_usable > 0) {
                    e.set_holding_sync_lock();

                    if (!sock.write_signalled) {
                        sock.write_signalled = true;
                        wake_up_waiters(sock.waiters, WAIT_TYPE_WRITING);
                    }
                }
            }
        }
    }

    if (cur_state == tcp_state::SYN_SENT && tcp.flags == TCP_SYN_ACK) {
        tcp_seq ack;

        // FIXME - parse options

        {
            lock_guard lock(sock.state_lock);

            sock.recv_next = tcp.seq + 1;

            ack = sock.recv_next;
        }

        {
            lock_guard lock(sock.io_lock);
            sock.send_unacknowledged = tcp.ack;
        }

        send_ack<FAMILY>(sock, true, ack);

        {
            lock_guard lock(sock.state_lock);
            sock.state = cur_state = tcp_state::ESTABLISHED;
        }

        set_event(static_cast<event&>(sock.state_change_event.get()));
    } else if (cur_state == tcp_state::ESTABLISHED && tcp.flags == TCP_SYN_ACK) {
        // first ACK must have been lost, retransmit
        if (sock.first_ack)
            dispatch_packet<FAMILY>(*sock.pni, *sock.first_ack, sock.dst_mac);
    }

    if (tcp.ACK) {
        bool do_cleanup = false, just_established = false;
        auto ack = tcp.ack;

        while (true) {
            tcp_retransmission* r = nullptr;

            {
                lock_guard lock(sock.retransmission_list_lock);

                if (is_list_empty(sock.retransmission_list)) {
                    {
                        lock_guard lock(sock.state_lock);

                        if (sock.state == tcp_state::FIN_WAIT_1)
                            sock.state = tcp_state::FIN_WAIT_2;
                        else if (sock.state == tcp_state::LAST_ACK)
                            do_cleanup = true;
                        else if (sock.state == tcp_state::SYN_RECEIVED) {
                            sock.state = tcp_state::ESTABLISHED;
                            just_established = true;
                        }
                    }
                    break;
                }

                auto& r2 = containing_record<tcp_retransmission>(sock.retransmission_list.next);

                auto& rtcp = *(tcp_header*)(r2.pack.data + sizeof(typename FAMILY::header));

                if (ack <= rtcp.seq)
                    break;

                remove_entry_list(r2.le);
                r = &r2;
            }

            r->release();

            {
                lock_guard lock(sock.io_lock);

                auto old_usable = sock.allowed_send_bytes();

                sock.send_unacknowledged = ack;

                auto new_usable = sock.allowed_send_bytes();

                auto& e = static_cast<event&>(sock.send_window_open_event.get());

                {
                    sync_lock sl;
                    lock_guard lock2(sl);

                    if (old_usable > 0 && new_usable == 0) {
                        e.signalled = false;
                        sock.write_signalled = false;
                    } else if (old_usable == 0 && new_usable > 0) {
                        e.set_holding_sync_lock();

                        if (!sock.write_signalled) {
                            sock.write_signalled = true;
                            wake_up_waiters(sock.waiters, WAIT_TYPE_WRITING);
                        }
                    }
                }
            }
        }

        if (do_cleanup) {
            if (!sock.parent) {
                lock_guard lock(tcp_ports_lock);
                lock_guard lock2(sock.state_lock);

                if (sock.bind_done && tcp_ports_v4[sock.bound_port] == &sock) {
                    tcp_ports_v4[sock.bound_port] = nullptr;
                    sock.release();
                }

                if (sock.bind_done && tcp_ports_v6[sock.bound_port] == &sock) {
                    tcp_ports_v6[sock.bound_port] = nullptr;
                    sock.release();
                }
            } else if (sock.on_accepted_list) {
                {
                    lock_guard lock(tcp_ports_lock);
                    remove_entry_list(sock.accepted_le);
                }

                sock.on_accepted_list = false;
                sock.release();
            }

            set_event(static_cast<event&>(sock.retransmission_thread_quit_event.get()));
        } else if (just_established) {
            visit([&](auto* parent) {
                sync_lock sl;
                lock_guard lock1(sl);
                lock_guard lock2(parent->list_lock);

                remove_entry_list(sock.le);
                insert_tail_list(parent->accept_queue, sock.le);

                auto& e = static_cast<event&>(parent->accept_queue_event.get());

                e.set_holding_sync_lock();

                if (!parent->signalled) {
                    wake_up_waiters(parent->waiters, WAIT_TYPE_READING);
                    parent->signalled = true;
                }
            }, get_socket_parent<FAMILY>(sock));
        }
    }

    if (size > header_length + ip_header_size || tcp.FIN) { // data or FIN
        auto p = new tcp_socket_packet(pack);
        if (!p) {
            print("new failed\n");
            return;
        }

        p->scope_id = sock.connected_scope;
        p->skip = ip_header_size + header_length;

        bool added = false;
        optional<tcp_seq> ack;
        bool clear_ooo = false;
        auto data_len = (uint32_t)(size > header_length + ip_header_size ? (size - header_length - ip_header_size) : 0);

        {
            lock_guard lock(sock.io_lock);
            lock_guard lock2(sock.state_lock);

            auto seq = tcp.seq;
            tcp_seq end = seq + data_len;

            if (seq > sock.recv_next) {
                if ((int32_t)data_len <= sock.recv_window && !sock.shutdown_read) {
                    sock.recv_window -= (int32_t)data_len;

                    bool drop = false;
                    for (auto le = sock.ooo_buffer.next; le != &sock.ooo_buffer; le = le->next) {
                        auto& p2 = containing_record<tcp_socket_packet>(le);

                        auto& ip2 = *(typename FAMILY::header*)p2.pack.data;
                        size_t ip_header_size2, size2;

                        if constexpr (is_same_v<FAMILY, ipv4>) {
                            ip_header_size2 = ip2.ihl * sizeof(uint32_t);
                            size2 = byteswap((uint16_t)ip2.total_length);
                        } else if constexpr (is_same_v<FAMILY, ipv6>) {
                            ip_header_size2 = sizeof(ipv6_header);
                            size2 = p2.pack.size;
                        }

                        auto& tcp2 = *(tcp_header*)(p2.pack.data + ip_header_size2);
                        tcp_seq seq2 = tcp2.seq + (uint32_t)(p2.skip - ip_header_size2 - (tcp2.header_length * sizeof(uint32_t)));
                        tcp_seq end2 = seq2 + (uint32_t)(size2 - p2.skip);

                        if (end2 <= seq)
                            continue;

                        if (seq2 >= end) {
                            insert_tail_list(*le, p->le);
                            added = true;
                            break;
                        } else if (seq2 <= seq) {
                            p->skip += min(end2, end) - seq;
                            data_len -= min(end2, end) - seq;
                            sock.recv_window += (int32_t)(min(end2, end) - seq);
                            seq = end2;

                            if (data_len == 0) {
                                drop = true;
                                break;
                            }
                        } else {
                            sock.recv_window += (int32_t)min(size2 - p2.skip, (size_t)(end - seq2));
                            p2.skip += min(size2 - p2.skip, (size_t)(end - seq2));

                            if (p2.skip == size2) {
                                le = le->prev;
                                remove_entry_list(p2.le);
                                delete &p2;
                            } else {
                                insert_tail_list(*le, p->le);
                                added = true;
                                break;
                            }
                        }
                    }

                    if (!added && !drop) {
                        insert_tail_list(sock.ooo_buffer, p->le);
                        added = true;
                    }
                }

                // send ACK for where we're up to so far
                ack = sock.recv_next;
            } else if (seq + data_len + (tcp.FIN ? 1 : 0) <= sock.recv_next) {
                // send ACK for packet we've already received
                ack = sock.recv_next;
            } else {
                if (seq < sock.recv_next) {
                    p->skip += sock.recv_next - seq;
                    data_len -= sock.recv_next - seq;
                }

                sock.recv_next += data_len;

                if (data_len > 0 && !sock.shutdown_read) {
                    insert_tail_list(sock.buffer, p->le);
                    added = true;
                }

                if (tcp.FIN && (int32_t)data_len <= sock.recv_window) {
                    sock.recv_next++;

                    if (sock.state == tcp_state::ESTABLISHED)
                        sock.state = tcp_state::CLOSE_WAIT;
                    else if (sock.state == tcp_state::FIN_WAIT_2)
                        sock.state = tcp_state::TIME_WAIT;

                    clear_ooo = true;
                } else {
                    // check out-of-order list for anything now okay
                    while (true) {
                        if (is_list_empty(sock.ooo_buffer))
                            break;

                        auto& p2 = containing_record<tcp_socket_packet>(sock.ooo_buffer.next);
                        auto& ip2 = *(typename FAMILY::header*)p2.pack.data;
                        size_t ip_header_size2;

                        if constexpr (is_same_v<FAMILY, ipv4>)
                            ip_header_size2 = ip2.ihl * sizeof(uint32_t);
                        else if constexpr (is_same_v<FAMILY, ipv6>)
                            ip_header_size2 = sizeof(ipv6_header);

                        auto& tcp2 = *(tcp_header*)(p2.pack.data + ip_header_size2);
                        size_t header_length2 = tcp2.header_length * sizeof(uint32_t);
                        size_t size2;

                        if constexpr (is_same_v<FAMILY, ipv4>)
                            size2 = byteswap((uint16_t)ip2.total_length);
                        else if constexpr (is_same_v<FAMILY, ipv6>)
                            size2 = p2.pack.size;

                        tcp_seq seq2 = tcp2.seq + (uint32_t)(p2.skip - ip_header_size2 - header_length2);
                        tcp_seq end2 = seq2 + (uint32_t)(size2 - p2.skip);

                        if (end2 <= sock.recv_next) {
                            remove_entry_list(p2.le);
                            sock.recv_window += (int32_t)(size2 - p2.skip);

                            if (tcp2.FIN) {
                                sock.recv_next++;

                                if (sock.state == tcp_state::ESTABLISHED)
                                    sock.state = tcp_state::CLOSE_WAIT;
                                else if (sock.state == tcp_state::FIN_WAIT_2)
                                    sock.state = tcp_state::TIME_WAIT;

                                clear_ooo = true;
                                delete &p2;

                                break;
                            }

                            delete &p2;
                            continue;
                        }

                        if (seq2 > sock.recv_next)
                            break;

                        if (seq2 < sock.recv_next) {
                            p2.skip += sock.recv_next - seq2;
                            sock.recv_window += (int32_t)(sock.recv_next - seq2);

                            sock.recv_next = seq2;
                        }

                        sock.recv_next = end2;

                        if (tcp2.FIN) {
                            sock.recv_next++;

                            if (sock.state == tcp_state::ESTABLISHED)
                                sock.state = tcp_state::CLOSE_WAIT;
                            else if (sock.state == tcp_state::FIN_WAIT_2)
                                sock.state = tcp_state::TIME_WAIT;

                            clear_ooo = true;
                            break;
                        } else {
                            remove_entry_list(p2.le);

                            if (!sock.shutdown_read)
                                insert_tail_list(sock.buffer, p2.le);
                            else
                                delete &p2;
                        }
                    }
                }

                ack = sock.recv_next;

                // may go negative
                if (!sock.shutdown_read)
                    sock.recv_window -= (int32_t)data_len;
            }
        }

        if (clear_ooo || sock.shutdown_read) {
            while (true) {
                tcp_socket_packet* p;

                {
                    lock_guard lock(sock.state_lock);

                    if (!is_list_empty(sock.ooo_buffer))
                        p = &containing_record<tcp_socket_packet>(sock.ooo_buffer.next);
                    else
                        break;

                    remove_entry_list(p->le);
                }

                delete p;
            }

            if (sock.on_accepted_list && sock.state == tcp_state::TIME_WAIT) {
                {
                    lock_guard lock(tcp_ports_lock);
                    remove_entry_list(sock.accepted_le);
                }

                sock.on_accepted_list = false;
                sock.release();
            }
        }

        if (!added)
            delete p;

        if (ack.has_value()) {
            send_ack<FAMILY>(sock, false, *ack);

            {
                sync_lock sl;
                lock_guard lock(sl);
                lock_guard lock2(sock.state_lock);

                if (!is_list_empty(sock.buffer) || sock.state != tcp_state::ESTABLISHED) {
                    if (!sock.signalled) {
                        wake_up_waiters(sock.waiters, WAIT_TYPE_READING);
                        sock.signalled = true;
                    }
                } else
                    sock.signalled = false;
            }
        }
    }
}

uint16_t base_tcp_socket::allowed_send_bytes() {
    int32_t raw = (send_unacknowledged - send_next) + send_window;

    if (raw < 0)
        return 0;

    return (uint16_t)raw;
}

template<socket_family FAMILY>
template<bool NONBLOCK>
nt_status tcp_socket<FAMILY>::do_sendto(span<const span<const uint8_t>> v,
                                        int flags, sockaddr*, size_t,
                                        uint64_t& sent_bytes) {
    nt_status status = nt_status::SUCCESS;

    // sockaddr is ignored

    if (flags != 0)
        return nt_status::INVALID_PARAMETER;

    {
        lock_guard lock(state_lock);

        if (state != tcp_state::ESTABLISHED && state != tcp_state::CLOSE_WAIT)
            return nt_status::CONNECTION_DISCONNECTED;
    }

    // FIXME - Nagle's algorithm (batching of outgoing data)

    auto vec_size = get_vec_size(v);
    size_t done = 0;

    auto& ni = *pni;

    span<const uint8_t> top = vec_size == 0 ? span<const uint8_t>() : v.front();

    ni.acquire();

    while (vec_size > 0) {
        size_t size;

        if constexpr (NONBLOCK) {
            status = wait_for_object(send_window_open_event.get(), true, 0);
            if (status == nt_status::TIMEOUT) {
                ni.release();

                sent_bytes = done;

                return done == 0 ? nt_status::CANT_WAIT : nt_status::SUCCESS;
            }
        } else
            status = wait_for_object(send_window_open_event.get(), true);

        if (!success(status)) {
            ni.release();
            return status;
        } else if (status == nt_status::USER_APC || status == nt_status::ALERTED) {
            ni.release();

            sent_bytes = done;

            return done == 0 ? status : nt_status::SUCCESS;
        }

        if (dead || shutdown_write) {
            status = nt_status::CONNECTION_ABORTED;
            break;
        }

        packet* pack;
        tcp_retransmission* r;
        bool send_ipv4;

        {
            lock_guard lock(io_lock);

            size = min(vec_size, mss);

            auto usable = allowed_send_bytes();

            if (size > usable)
                size = usable;

            if (size == 0)
                continue;

            pack = new packet;
            if (!pack) {
                ni.release();

                if (done == 0)
                    return nt_status::INSUFFICIENT_RESOURCES;
                else {
                    sent_bytes = done;
                    return nt_status::SUCCESS;
                }
            }

            r = new tcp_retransmission(*pack);
            if (!r) {
                pack->release();
                ni.release();

                if (done == 0)
                    return nt_status::INSUFFICIENT_RESOURCES;
                else {
                    sent_bytes = done;
                    return nt_status::SUCCESS;
                }
            }

            if constexpr (is_same_v<FAMILY, ipv4>)
                send_ipv4 = true;
            else if constexpr (is_same_v<FAMILY, ipv6>)
                send_ipv4 = is_mapped_address(connected_address);

            size_t ip_header = send_ipv4 ? sizeof(ipv4_header) : sizeof(ipv6_header);

            pack->size = ip_header + sizeof(tcp_header) + size;

            auto& tcp = *(tcp_header*)(pack->data + ip_header);

            memset(&tcp, 0, sizeof(tcp));

            tcp.src_port = bound_port;
            tcp.dst_port = connected_port;
            tcp.seq = send_next;
            tcp.ack = recv_next;
            tcp.header_length = sizeof(tcp_header) / sizeof(uint32_t);
            tcp.PSH = 1;
            tcp.ACK = 1;
            tcp.window = tcp_window(*this);

            send_next += (uint32_t)size;

            memcpy_vec(pack->data + ip_header + (tcp.header_length * sizeof(uint32_t)),
                       v, top, size);

            if (send_ipv4) {
                auto& ip = *(ipv4_header*)pack->data;

                ip.ihl = sizeof(ipv4_header) / sizeof(uint32_t);
                ip.version = 4;
                ip.ecn = 0;
                ip.dscp = 0;
                ip.total_length = byteswap((uint16_t)(sizeof(ipv4_header) + sizeof(tcp_header) + size));
                ip.identification = 0;
                ip.fragment_offset1 = 0;
                ip.MF = 0;
                ip.DF = 1;
                ip.R = 0;
                ip.fragment_offset2 = 0;
                ip.ttl = DEFAULT_HOP_LIMIT;
                ip.protocol = IPPROTO_TCP;
                ip.header_checksum = 0;

                if constexpr (is_same_v<FAMILY, ipv6>) {
                    ip.src = extract_mapped_address(bound_address);
                    ip.dst = extract_mapped_address(connected_address);
                } else if constexpr (is_same_v<FAMILY, ipv4>) {
                    ip.src = bound_address;
                    ip.dst = connected_address;
                }

                ip.header_checksum = calc_ipv4_checksum(span(pack->data, sizeof(ipv4_header)));

                tcp.checksum = calc_ipv4_checksum(ip, span((uint8_t*)&tcp, pack->size - sizeof(ipv4_header)),
                                                  IPPROTO_TCP);
            } else if constexpr (is_same_v<FAMILY, ipv6>) {
                auto& ip = *(ipv6_header*)pack->data;

                ip.traffic_class1 = 0;
                ip.version = 6;
                ip.traffic_class2 = 0;
                ip.flow_label_high = 0;
                ip.flow_label_medium = 0;
                ip.flow_label_low = 0;
                ip.payload_length = byteswap((uint16_t)(sizeof(tcp_header) + size));
                ip.next_header = IPPROTO_TCP;
                ip.hop_limit = DEFAULT_HOP_LIMIT;
                ip.src = bound_address;
                ip.dst = connected_address;

                if (!ni.tx_tcpv6_csum_offload) {
                    tcp.checksum = calc_ipv6_checksum(ip, span((uint8_t*)&tcp, pack->size - sizeof(ipv6_header)),
                                                      IPPROTO_TCP);
                } else {
                    tcp.checksum = calc_ipv6_checksum_offload(ip, (uint32_t)(pack->size - sizeof(ipv6_header)),
                                                              IPPROTO_TCP);
                }
            }

            r->deadline = (uint64_t)kernel_time_page.system_time + (RETRANSMISSION_DELAY * SYSTEM_TIME_RESOLUTION);

            {
                lock_guard lock(retransmission_list_lock);

                insert_tail_list(retransmission_list, r->le);
            }
        }

        if (send_ipv4)
            dispatch_packet<ipv4>(ni, *pack, dst_mac);
        else
            dispatch_packet<ipv6>(ni, *pack, dst_mac);

        pack->release();

        {
            lock_guard lock(io_lock);

            if (allowed_send_bytes() == 0) {
                sync_lock sl;
                lock_guard lock(sl);

                auto& e = static_cast<event&>(send_window_open_event.get());

                e.signalled = false;
                write_signalled = false;
            }
        }

        done += size;
        vec_size -= size;
    }

    ni.release();

    sent_bytes = done;

    return status;
}

template<socket_family FAMILY>
tcp_socket<FAMILY>::tcp_socket() : base_tcp_socket(is_same_v<FAMILY, ipv4> ? true : false) {
    if constexpr (is_same_v<FAMILY, ipv4>)
        mss = MSS_IPv4;
    else if constexpr (is_same_v<FAMILY, ipv6>)
        mss = MSS_IPv6;

    state_change_event = create_event(true);
    if (!state_change_event) {
        print("create_event failed\n");
        return;
    }

    retransmission_thread_quit_event = create_event(false);
    if (!retransmission_thread_quit_event) {
        print("create_event failed\n");
        return;
    }

    send_window_open_event = create_event(false);
    if (!send_window_open_event) {
        print("create_event failed\n");
        return;
    }

    this->acquire();

    retransmission_thread_obj = add_kernel_thread([](void* ctx) {
        auto& s = *(tcp_socket<FAMILY>*)ctx;

        s.retransmission_thread();

        s.release();
    }, this);

    init = true;
}

template<socket_family FAMILY>
tcp_socket<FAMILY>::~tcp_socket() {
    while (true) {
        tcp_retransmission* r;

        {
            lock_guard lock(retransmission_list_lock);

            if (is_list_empty(retransmission_list))
                break;

            r = &containing_record<tcp_retransmission>(retransmission_list.next);

            remove_entry_list(r->le);
        }

        r->release();
    }

    while (true) {
        tcp_socket_packet* p;

        {
            lock_guard lock(state_lock);

            if (!is_list_empty(buffer))
                p = &containing_record<tcp_socket_packet>(buffer.next);
            else if (!is_list_empty(ooo_buffer))
                p = &containing_record<tcp_socket_packet>(ooo_buffer.next);
            else
                break;

            remove_entry_list(p->le);
        }

        delete p;
    }

    if (pni)
        pni->release();

    if (parent) {
        visit([&](auto* parent) {
            if (added_to_list) {
                lock_guard lock(parent->list_lock);
                remove_entry_list(le);
            }

            parent->release();
        }, get_socket_parent<FAMILY>(*this));
    }
}

template<socket_family FAMILY>
void tcp_socket<FAMILY>::retransmission_thread() {
    unsigned int time_wait_count = 0;

    auto& ev = static_cast<event&>(retransmission_thread_quit_event.get());

    while (true) {
        bool time_wait_done = false;

        auto status = wait_for_object(ev, false, SYSTEM_TIME_RESOLUTION);

        if (status == nt_status::SUCCESS)
            break;

        if (!success(status)) {
            print("wait_for_object returned {}, exiting network thread\n", status);
            break;
        }

        {
            lock_guard lock(state_lock);

            if (state == tcp_state::TIME_WAIT) {
                time_wait_count++;
                if (time_wait_count == TIME_WAIT_LENGTH)
                    time_wait_done = true;
            }
        }

        if (time_wait_done) {
            if (!parent) {
                lock_guard lock(tcp_ports_lock);
                lock_guard lock2(state_lock);

                if (bind_done && tcp_ports_v4[bound_port] == this) {
                    tcp_ports_v4[bound_port] = nullptr;
                    release();
                }

                if constexpr (is_same_v<FAMILY, ipv6>) {
                    if (bind_done && tcp_ports_v6[bound_port] == this) {
                        tcp_ports_v6[bound_port] = nullptr;
                        release();
                    }
                }
            }

            break;
        }

        do_retransmissions();
    }
}

template<socket_family FAMILY>
void tcp_socket<FAMILY>::do_retransmissions() {
    unsigned int num = 0, added = 0;
    auto now = (uint64_t)kernel_time_page.system_time;

    {
        lock_guard lock(retransmission_list_lock);

        for (auto le = retransmission_list.next; le != &retransmission_list; le = le->next) {
            auto& r = containing_record<tcp_retransmission>(le);

            if (now >= r.deadline)
                num++;
            else
                break;
        }
    }

    if (num == 0)
        return;

    auto arr = (tcp_retransmission**)malloc(sizeof(tcp_retransmission*) * num);
    if (!arr) {
        print("malloc failed\n");
        return;
    }

    {
        lock_guard lock(retransmission_list_lock);

        for (auto le = retransmission_list.next; le != &retransmission_list; le = le->next) {
            auto& r = containing_record<tcp_retransmission>(le);

            if (now >= r.deadline) {
                r.acquire();
                arr[added] = &r;
                added++;

                if (added == num)
                    break;
            } else
                break;
        }
    }

    for (unsigned int i = 0; i < added; i++) {
        auto& r = *arr[i];

        if constexpr (is_same_v<FAMILY, ipv4>)
            dispatch_packet<FAMILY>(*pni, r.pack, dst_mac);
        else if constexpr (is_same_v<FAMILY, ipv6>) {
            if (is_mapped_address(connected_address))
                dispatch_packet<ipv4>(*pni, r.pack, dst_mac);
            else
                dispatch_packet<FAMILY>(*pni, r.pack, dst_mac);
        }

        // FIXME - delay increase should be exponential

        r.attempt++;
        r.deadline = (uint64_t)kernel_time_page.system_time + (RETRANSMISSION_DELAY * SYSTEM_TIME_RESOLUTION);

        r.release();
    }

    free(arr);
}

template<socket_family FAMILY>
template<bool NONBLOCK>
nt_status tcp_socket<FAMILY>::do_recvfrom(span<const span<uint8_t>> v, int flags,
                                          sockaddr_storage* from, size_t* fromlen,
                                          uint64_t& recv_bytes) {
    nt_status status;

    if (flags & ~MSG_PEEK)
        return nt_status::INVALID_PARAMETER;

    recv_bytes = 0;

    auto vec_size = get_vec_size(v);
    span<uint8_t> top = vec_size == 0 ? span<uint8_t>() : v.front();

    acquire();

    while (true) {
        if constexpr (NONBLOCK) {
            if (!us_read_mutex.try_lock()) {
                release();
                return recv_bytes == 0 ? nt_status::CANT_WAIT : nt_status::SUCCESS;
            }
        } else
            us_read_mutex.lock();

        lock_guard lock(us_read_mutex, adopt_lock_t{});

        if constexpr (NONBLOCK) {
            status = wait_for_object(*this, true, 0);

            if (status == nt_status::TIMEOUT) {
                release();
                return recv_bytes == 0 ? nt_status::CANT_WAIT : nt_status::SUCCESS;
            }
        } else
            status = wait_for_object(*this, true);

        if (!success(status) || status == nt_status::USER_APC || status == nt_status::ALERTED) {
            release();
            return status;
        }

        if (dead || shutdown_read)
            break;

        size_t removed_bytes = 0;

        auto tcp_read = [&](tcp_socket_packet& r, size_t& off, bool peek) -> bool {
            bool do_ipv4;
            size_t pack_size;

            if constexpr (is_same_v<FAMILY, ipv4>)
                do_ipv4 = true;
            else if constexpr (is_same_v<FAMILY, ipv6>)
                do_ipv4 = is_mapped_address(connected_address);

            if (do_ipv4) {
                auto& ip = *(ipv4_header*)r.pack.data;

                pack_size = byteswap((uint16_t)ip.total_length);
            } else
                pack_size = r.pack.size;

            auto to_copy = (uint32_t)min(pack_size - r.skip - off, vec_size);

            memcpy_vec(v, top, r.pack.data + r.skip + off, to_copy);

            recv_bytes += to_copy;

            if (r.skip + off + to_copy < pack_size) {
                off += to_copy;
                return true;
            }

            if (!peek) {
                lock_guard lock(state_lock);
                remove_entry_list(r.le);
            }

            vec_size -= to_copy;
            off = 0;

            if (!peek) {
                removed_bytes += pack_size - r.skip;

                delete &r;
            }

            if (vec_size == 0)
                return true;

            return false;
        };

        if (flags & MSG_PEEK) {
            lock_guard lock(state_lock);

            auto off = us_read_offset;

            for (auto le = buffer.next; le != &buffer; le = le->next) {
                auto& r = containing_record<tcp_socket_packet>(le);

                if (tcp_read(r, off, true))
                    break;
            }
        } else {
            while (true) {
                tcp_socket_packet* r;

                {
                    lock_guard lock(state_lock);

                    if (is_list_empty(buffer))
                        break;

                    r = &containing_record<tcp_socket_packet>(buffer.next);
                }

                if (tcp_read(*r, us_read_offset, false))
                    break;
            }
        }

        if (removed_bytes != 0) {
            {
                sync_lock sl;
                lock_guard lock(sl);
                lock_guard lock2(state_lock);

                if (is_list_empty(buffer) && state == tcp_state::ESTABLISHED)
                    signalled = false;
            }

            bool do_send_ack = false;
            tcp_seq ack;

            {
                lock_guard lock(io_lock);

                // FIXME - should actually be other side's MSS
                if (recv_window < (int32_t)mss)
                    do_send_ack = true;

                recv_window += (int32_t)removed_bytes;
                ack = recv_next;
            }

            if (do_send_ack) {
                if constexpr (is_same_v<FAMILY, ipv4>)
                    send_ack<FAMILY>(*this, false, ack);
                else if constexpr (is_same_v<FAMILY, ipv6>) {
                    if (is_mapped_address(connected_address))
                        send_ack<ipv4>(*this, false, ack);
                    else
                        send_ack<FAMILY>(*this, false, ack);
                }
            }
        }

        if (!success(status) && recv_bytes == 0) {
            release();
            return status;
        }

        if (from && fromlen && *fromlen > 0) {
            auto& addr = *(typename FAMILY::sockaddr*)from;

            memset(&addr, 0, sizeof(addr));

            if constexpr (is_same_v<FAMILY, ipv4>) {
                addr.sin_family = AF_INET;
                addr.sin_port = connected_port;
                *(ipv4_address*)&addr.sin_addr = connected_address;
            } else if constexpr (is_same_v<FAMILY, ipv6>) {
                addr.sin6_family = AF_INET6;
                addr.sin6_port = connected_port;
                addr.sin6_flowinfo = 0; // FIXME? Allowed by RFC3493, but not necessarily what Linux does
                *(ipv6_address*)&addr.sin6_addr = connected_address;
                addr.sin6_scope_id = connected_scope;
            }

            *fromlen = sizeof(addr);
        }

        break;
    }

    release();

    if (dead)
        return nt_status::CONNECTION_ABORTED;

    return nt_status::SUCCESS;
}

template<socket_family FAMILY>
nt_status tcp_socket<FAMILY>::bind(const sockaddr* addr, size_t addrlen) {
    if (addr->sa_family != FAMILY::family)
        return nt_status::INVALID_PARAMETER;

    if (addrlen < sizeof(typename FAMILY::sockaddr))
        return nt_status::BUFFER_TOO_SMALL;

    auto& b = *(typename FAMILY::sockaddr*)addr;
    typename FAMILY::address ip;

    if constexpr (is_same_v<FAMILY, ipv4>) {
        ip = *(ipv4_address*)&b.sin_addr;

        if (ip != parse_ipv4_address("0.0.0.0")) {
            if (!is_our_ip(ip))
                return nt_status::INVALID_ADDRESS_COMPONENT;
        }
    } else if constexpr (is_same_v<FAMILY, ipv6>) {
        ip = *(ipv6_address*)&b.sin6_addr;

        if (ip != parse_ipv6_address("::")) {
            if (!is_our_ip(ip))
                return nt_status::INVALID_ADDRESS_COMPONENT;
        }
    }

    {
        uint16_t port;
        lock_guard lock(tcp_ports_lock);
        lock_guard lock2(state_lock);

        if (bind_done)
            return nt_status::INVALID_PARAMETER;

        if constexpr (is_same_v<FAMILY, ipv4>) {
            if (b.sin_port == 0) {
                port = find_ephemeral_tcp_port<FAMILY>(false);
                if (port == 0) // no ephemeral ports left
                    return nt_status::ADDRESS_ALREADY_ASSOCIATED;
            } else {
                port = b.sin_port;

                if (tcp_ports_v4[b.sin_port])
                    return nt_status::ADDRESS_ALREADY_ASSOCIATED;
            }

            tcp_ports_v4[port] = this;
        } else if constexpr (is_same_v<FAMILY, ipv6>) {
            if (b.sin6_port == 0) {
                port = find_ephemeral_tcp_port<FAMILY>(ip == parse_ipv6_address("::") && !ipv6_only);
                if (port == 0) // no ephemeral ports left
                    return nt_status::ADDRESS_ALREADY_ASSOCIATED;
            } else {
                port = b.sin6_port;

                if (tcp_ports_v6[b.sin6_port])
                    return nt_status::ADDRESS_ALREADY_ASSOCIATED;

                if (tcp_ports_v4[b.sin6_port] && ip == parse_ipv6_address("::") && !ipv6_only)
                    return nt_status::ADDRESS_ALREADY_ASSOCIATED;
            }

            tcp_ports_v6[port] = this;

            if (ip == parse_ipv6_address("::") && !ipv6_only) {
                tcp_ports_v4[port] = this;
                this->acquire();
            }
        }

        this->acquire();

        bound_port = port;
        bound_address = ip;

        bind_done = true;
    }

    return nt_status::SUCCESS;
}

nt_status create_new_ipv4_tcp_socket(object_ref& ref) {
    auto s = new tcp_socket<ipv4>;
    if (!s) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    if (!s->init) {
        s->release();
        return nt_status::INTERNAL_ERROR;
    }

    ref = object_ref{*s};

    return nt_status::SUCCESS;
}

nt_status create_new_ipv6_tcp_socket(object_ref& ref) {
    auto s = new tcp_socket<ipv6>;
    if (!s) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    if (!s->init) {
        s->release();
        return nt_status::INTERNAL_ERROR;
    }

    ref = object_ref{*s};

    return nt_status::SUCCESS;
}

template<socket_family FAMILY>
nt_status tcp_socket<FAMILY>::listen(int backlog) {
    if (backlog < 0)
        return nt_status::INVALID_PARAMETER;

    auto ev = create_event(false);
    if (!ev) {
        print("create_event failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    {
        lock_guard lock(state_lock);

        if (state != tcp_state::CLOSED || !bind_done)
            return nt_status::INVALID_PARAMETER;

        listen_backlog = (unsigned int)backlog;
        state = tcp_state::LISTEN;

        swap(accept_queue_event, ev);

        return nt_status::SUCCESS;
    }
}

template<socket_family FAMILY>
nt_status tcp_socket<FAMILY>::accept(HANDLE& sock, void* us_addr, int& addrlen) {
    nt_status status;

    if (addrlen < 0 || (us_addr && (size_t)addrlen < sizeof(typename FAMILY::sockaddr)))
        return nt_status::INVALID_PARAMETER;

    auto& t = *get_current_thread();
    if (!t.proc)
        return nt_status::INTERNAL_ERROR;

    {
        lock_guard lock(state_lock);

        if (state != tcp_state::LISTEN)
            return nt_status::INVALID_PARAMETER;
    }

    typename socket_variant<FAMILY>::type c;

    while (true) {
        status = wait_for_object(accept_queue_event.get(), true);
        if (!success(status) || status == nt_status::USER_APC || status == nt_status::ALERTED)
            return status;

        if (dead)
            return nt_status::CONNECTION_DISCONNECTED;

        {
            sync_lock sl;
            lock_guard lock1(sl);
            lock_guard lock2(list_lock);

            if (is_list_empty(accept_queue))
                continue;

            auto& s = containing_record<base_tcp_socket>(accept_queue.next);

            remove_entry_list(s.le);
            s.accepted = true;
            s.added_to_list = false;

            if constexpr (is_same_v<FAMILY, ipv4>) {
                auto& s2 = static_cast<tcp_socket<ipv4>&>(s);
                c = &s2;
            } else {
                if (s.is_ipv4) {
                    auto& s2 = static_cast<tcp_socket<ipv4>&>(s);
                    c = &s2;
                } else {
                    auto& s2 = static_cast<tcp_socket<ipv6>&>(s);
                    c = &s2;
                }
            }

            if (is_list_empty(accept_queue)) {
                auto& e = static_cast<event&>(accept_queue_event.get());
                e.signalled = false;
                this->signalled = false;
            }

            listen_backlog++;
        }

        break;
    }

    return visit([&](auto* c) {
        if (us_addr) {
            typename FAMILY::sockaddr addr;

            memset(&addr, 0, sizeof(addr));

            if constexpr (is_same_v<FAMILY, ipv4>) {
                addr.sin_family = AF_INET;
                addr.sin_port = c->connected_port;
                *(ipv4_address*)&addr.sin_addr = c->connected_address;
            } else if constexpr (is_same_v<FAMILY, ipv6>) {
                addr.sin6_family = AF_INET6;
                addr.sin6_port = c->connected_port;
                addr.sin6_flowinfo = 0; // FIXME? Allowed by RFC3493, but not necessarily what Linux does
                addr.sin6_scope_id = c->connected_scope;

                if constexpr (is_same_v<remove_cvref_t<decltype(*c)>, tcp_socket<ipv4>>)
                    *(ipv6_address*)&addr.sin6_addr = map_ipv4_address_to_ipv6(c->connected_address);
                else
                    *(ipv6_address*)&addr.sin6_addr = c->connected_address;
            }

            status = t.proc->write_vm(us_addr, &addr, (uint32_t)sizeof(addr),
                                    nullptr);
            if (!success(status)) {
                c->release();
                return status;
            }

            addrlen = sizeof(addr);
        }

        {
            lock_guard lock(tcp_ports_lock);

            if constexpr (is_same_v<remove_cvref_t<decltype(*c)>, tcp_socket<ipv4>>)
                insert_tail_list(accepted_sockets_v4, c->accepted_le);
            else
                insert_tail_list(accepted_sockets_v6, c->accepted_le);

            c->on_accepted_list = true;
        }

        auto f = new file(*c, 0, 0);
        if (!f) {
            print("new failed\n");

            {
                lock_guard lock(tcp_ports_lock);
                remove_entry_list(c->accepted_le);
                c->on_accepted_list = false;
            }

            c->release();

            return nt_status::INSUFFICIENT_RESOURCES;
        }

        object_ref ref{*f};

        status = t.proc->add_handle(ref.get(), FILE_READ_DATA | FILE_WRITE_DATA,
                                    true, sock);
        if (!success(status)) {
            {
                lock_guard lock(tcp_ports_lock);
                remove_entry_list(c->accepted_le);
                c->on_accepted_list = false;
            }

            c->release();

            return status;
        }

        return nt_status::SUCCESS;
    }, c);
}

template<socket_family FAMILY>
nt_status tcp_socket<FAMILY>::send_syn_ack(const mac_address& mac) {
    nt_status status;

    auto& ni = *pni;

    if (connected_address == bound_address)
        dst_mac = ni.mac;
    else {
        if constexpr (is_same_v<FAMILY, ipv6>)
            maybe_add_to_neighbour_cache(ni, connected_address, mac);

        dst_mac = mac;
    }

    auto pack = new packet;
    if (!pack) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    auto r = new tcp_retransmission(*pack);
    if (!r) {
        print("new failed\n");
        pack->release();
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    pack->size = sizeof(typename FAMILY::header) + sizeof(tcp_header);

    if constexpr (is_same_v<FAMILY, ipv4>) {
        auto& ip = *(ipv4_header*)pack->data;

        ip.ihl = sizeof(ipv4_header) / sizeof(uint32_t);
        ip.version = 4;
        ip.ecn = 0;
        ip.dscp = 0;
        ip.total_length = byteswap((uint16_t)(sizeof(ipv4_header) + sizeof(tcp_header)));
        ip.identification = 0;
        ip.fragment_offset1 = 0;
        ip.MF = 0;
        ip.DF = 1;
        ip.R = 0;
        ip.fragment_offset2 = 0;
        ip.ttl = DEFAULT_HOP_LIMIT;
        ip.protocol = IPPROTO_TCP;
        ip.header_checksum = 0;
        ip.src = bound_address;
        ip.dst = connected_address;

        ip.header_checksum = calc_ipv4_checksum(span(pack->data, sizeof(ipv4_header)));
    } else if constexpr (is_same_v<FAMILY, ipv6>) {
        auto& ip = *(ipv6_header*)pack->data;

        ip.traffic_class1 = 0;
        ip.version = 6;
        ip.traffic_class2 = 0;
        ip.flow_label_high = 0;
        ip.flow_label_medium = 0;
        ip.flow_label_low = 0;
        ip.payload_length = byteswap((uint16_t)sizeof(tcp_header));
        ip.next_header = IPPROTO_TCP;
        ip.hop_limit = DEFAULT_HOP_LIMIT;
        ip.src = bound_address;
        ip.dst = connected_address;
    }

    auto& tcp = *(tcp_header*)(pack->data + sizeof(typename FAMILY::header));

    memset(&tcp, 0, sizeof(tcp));

    recv_next++;

    tcp.src_port = bound_port;
    tcp.dst_port = connected_port;
    tcp.seq = send_next;
    tcp.ack = recv_next;
    tcp.header_length = sizeof(tcp_header) / sizeof(uint32_t);
    tcp.SYN = 1;
    tcp.ACK = 1;
    tcp.window = tcp_window(*this);

    send_next++;

    if constexpr (is_same_v<FAMILY, ipv4>) {
        auto& ip = *(ipv4_header*)pack->data;

        tcp.checksum = calc_ipv4_checksum(ip, span((uint8_t*)&tcp, pack->size - sizeof(ipv4_header)),
                                          IPPROTO_TCP);
    } else if constexpr (is_same_v<FAMILY, ipv6>) {
        auto& ip = *(ipv6_header*)pack->data;

        if (!ni.tx_tcpv6_csum_offload) {
            tcp.checksum = calc_ipv6_checksum(ip, span((uint8_t*)&tcp, pack->size - sizeof(ipv6_header)),
                                              IPPROTO_TCP);
        } else {
            tcp.checksum = calc_ipv6_checksum_offload(ip, (uint32_t)(pack->size - sizeof(ipv6_header)),
                                                      IPPROTO_TCP);
        }
    }

    status = dispatch_packet<FAMILY>(ni, *pack, dst_mac);
    if (!success(status)) {
        pack->release();
        r->release();
        return status;
    }

    r->deadline = (uint64_t)kernel_time_page.system_time + (RETRANSMISSION_DELAY * SYSTEM_TIME_RESOLUTION);

    {
        lock_guard lock(retransmission_list_lock);
        insert_tail_list(retransmission_list, r->le);
    }

    pack->release();

    return nt_status::SUCCESS;
}

template<socket_family FAMILY>
void tcp_socket<FAMILY>::received_listen_packet(packet& pack, netif& ni,
                                                const mac_address& src_mac) {
    auto& ip = *(typename FAMILY::header*)pack.data;
    auto& tcp = *(tcp_header*)(pack.data + sizeof(typename FAMILY::header));

    tcp_socket<FAMILY>* found = nullptr;

    {
        lock_guard lock(list_lock);

        for (auto le = syn_queue.next; le != &syn_queue; le = le->next) {
            auto& s = containing_record<base_tcp_socket>(le);

            if (!!is_same_v<FAMILY, ipv4> != !!s.is_ipv4)
                continue;

            auto& s2 = static_cast<tcp_socket<FAMILY>&>(s);

            if (s2.connected_address == ip.src && s2.connected_port == tcp.src_port) {
                found = &s2;
                s2.acquire();
                break;
            }
        }

        if (!found) {
            for (auto le = accept_queue.next; le != &accept_queue; le = le->next) {
                auto& s = containing_record<base_tcp_socket>(le);

                if (!!is_same_v<FAMILY, ipv4> != !!s.is_ipv4)
                    continue;

                auto& s2 = static_cast<tcp_socket<FAMILY>&>(s);

                if (s2.connected_address == ip.src && s2.connected_port == tcp.src_port) {
                    found = &s2;
                    s2.acquire();
                    break;
                }
            }
        }
    }

    if (found) {
        received_packet<FAMILY>(*found, pack, ni, src_mac);
        found->release();
        return;
    }

    // FIXME - if receive SYN from something already on SYN queue, send SYN-ACK again (already handled by retransmissions?)

    // FIXME - also return RST if backlog exhausted
    if (tcp.flags != TCP_SYN) {
        send_tcp_port_closed<FAMILY>(pack, ni, src_mac);
        return;
    }

    auto c = new tcp_socket<FAMILY>;
    if (!c) {
        print("new failed\n");
        return;
    }

    if (!c->init) {
        print("tcp_socket init failed\n");
        delete c;
        return;
    }

    c->state = tcp_state::SYN_RECEIVED;
    c->bind_done = true;
    c->bound_address = ip.dst;
    c->bound_port = bound_port;
    c->connected_address = ip.src;
    c->connected_port = tcp.src_port;
    c->connected_scope = ni.num;
    c->recv_next = tcp.seq;

    c->send_next = rand_tcp_seq();
    c->send_unacknowledged = c->send_next;

    c->last_send_window_seq = tcp.seq;
    c->send_window = byteswap(tcp.window);

    if (c->allowed_send_bytes() > 0) {
        sync_lock sl;
        lock_guard lock(sl);

        auto& e = static_cast<event&>(c->send_window_open_event.get());
        e.set_holding_sync_lock();

        if (!c->write_signalled) {
            c->write_signalled = true;
            wake_up_waiters(c->waiters, WAIT_TYPE_WRITING);
        }
    }

    c->pni = &ni;
    ni.acquire();

    c->parent = this;
    this->acquire();

    bool done = false;

    {
        lock_guard lock(list_lock);

        if (listen_backlog > 0) {
            insert_tail_list(syn_queue, c->le);
            listen_backlog--;
            done = true;
        }
    }

    if (!done) {
        // syn_queue is full, drop
        set_event(static_cast<event&>(c->retransmission_thread_quit_event.get()));
        this->release();
        c->release();
        return;
    }

    c->added_to_list = true;

    c->send_syn_ack(src_mac);

    // FIXME - decrease backlog value

    // FIXME - free child socket if stays at SYN_RECEIVED too long
    // FIXME - increase backlog value when child goes away
}

template<socket_family FAMILY>
nt_status tcp_socket<FAMILY>::getsockname(span<uint8_t> buf, size_t& len) {
    if constexpr (is_same_v<FAMILY, ipv4>) {
        if (present_as_ipv6) {
            if (buf.size() < sizeof(sockaddr_in6))
                return nt_status::BUFFER_TOO_SMALL;

            auto& addr = *(sockaddr_in6*)buf.data();

            memset(&addr, 0, sizeof(addr));
            addr.sin6_family = AF_INET6;

            {
                lock_guard lock(state_lock);

                if (bind_done) {
                    addr.sin6_port = bound_port;
                    *(ipv6_address*)&addr.sin6_addr = map_ipv4_address_to_ipv6(bound_address);
                }

                addr.sin6_scope_id = connected_scope;
            }

            len = sizeof(addr);

            return nt_status::SUCCESS;
        }
    }

    if (buf.size() < sizeof(typename FAMILY::sockaddr))
        return nt_status::BUFFER_TOO_SMALL;

    auto& addr = *(typename FAMILY::sockaddr*)buf.data();

    memset(&addr, 0, sizeof(addr));

    if constexpr (is_same_v<FAMILY, ipv4>)
        addr.sin_family = AF_INET;
    else if constexpr (is_same_v<FAMILY, ipv6>)
        addr.sin6_family = AF_INET6;

    {
        lock_guard lock(state_lock);

        if (bind_done) {
            if constexpr (is_same_v<FAMILY, ipv4>) {
                addr.sin_port = bound_port;
                *(ipv4_address*)&addr.sin_addr = bound_address;
            } else if constexpr (is_same_v<FAMILY, ipv6>) {
                addr.sin6_port = bound_port;
                *(ipv6_address*)&addr.sin6_addr = bound_address;
            }
        }

        if constexpr (is_same_v<FAMILY, ipv6>)
            addr.sin6_scope_id = connected_scope;
    }

    len = sizeof(addr);

    return nt_status::SUCCESS;
}

template<socket_family FAMILY>
nt_status tcp_socket<FAMILY>::getpeername(span<uint8_t> buf, size_t& len) {
    if constexpr (is_same_v<FAMILY, ipv4>) {
        if (present_as_ipv6) {
            if (buf.size() < sizeof(sockaddr_in6))
                return nt_status::BUFFER_TOO_SMALL;

            auto& addr = *(sockaddr_in6*)buf.data();

            memset(&addr, 0, sizeof(addr));

            addr.sin6_family = AF_INET6;

            {
                lock_guard lock(state_lock);

                if (state == tcp_state::CLOSED || state == tcp_state::LISTEN)
                    return nt_status::CONNECTION_DISCONNECTED;

                addr.sin6_port = connected_port;
                addr.sin6_scope_id = connected_scope;

                *(ipv6_address*)&addr.sin6_addr = map_ipv4_address_to_ipv6(connected_address);
            }

            len = sizeof(addr);

            return nt_status::SUCCESS;
        }
    }

    if (buf.size() < sizeof(typename FAMILY::sockaddr))
        return nt_status::BUFFER_TOO_SMALL;

    auto& addr = *(typename FAMILY::sockaddr*)buf.data();

    memset(&addr, 0, sizeof(addr));

    if constexpr (is_same_v<FAMILY, ipv4>)
        addr.sin_family = AF_INET;
    else if constexpr (is_same_v<FAMILY, ipv4>)
        addr.sin6_family = AF_INET6;

    {
        lock_guard lock(state_lock);

        if (state == tcp_state::CLOSED || state == tcp_state::LISTEN)
            return nt_status::CONNECTION_DISCONNECTED;

        if constexpr (is_same_v<FAMILY, ipv4>) {
            addr.sin_port = connected_port;
            *(ipv4_address*)&addr.sin_addr = connected_address;
        } else if constexpr (is_same_v<FAMILY, ipv6>) {
            addr.sin6_port = connected_port;
            *(ipv6_address*)&addr.sin6_addr = connected_address;
            addr.sin6_scope_id = connected_scope;
        }
    }

    len = sizeof(addr);

    return nt_status::SUCCESS;
}

template<socket_family FAMILY>
nt_status tcp_socket<FAMILY>::setsockopt_tcp(sockopt_tcp opt, span<const uint8_t> buf) {
    switch (opt) {
        using enum sockopt_tcp;

        case NODELAY:
            if (buf.size() < sizeof(int))
                return nt_status::INVALID_PARAMETER;

            // FIXME - enabling and disabling Nagle's algorithm

            return nt_status::SUCCESS;

        case KEEPIDLE:
        case KEEPINTVL:
        case KEEPCNT:
            if (buf.size() < sizeof(int))
                return nt_status::INVALID_PARAMETER;

            // FIXME - TCP keep-alives

            return nt_status::SUCCESS;

        default:
            print("FIXME - setsockopt(IPPROTO_TCP, {}, {})\n", opt, buf.size());
            return nt_status::NOT_SUPPORTED;
    }
}

template<socket_family FAMILY>
nt_status tcp_socket<FAMILY>::shutdown(int how) {
    bool do_fin = false;

    if (how != SHUT_RD && how != SHUT_WR && how != SHUT_RDWR)
        return nt_status::INVALID_PARAMETER;

    {
        lock_guard lock(state_lock);

        if ((how == SHUT_WR || how == SHUT_RDWR) && !shutdown_write) {
            if (state == tcp_state::ESTABLISHED)
                state = tcp_state::FIN_WAIT_1;
            else if (state == tcp_state::CLOSE_WAIT)
                state = tcp_state::LAST_ACK;
            else
                return nt_status::CONNECTION_DISCONNECTED;

            do_fin = true;
            shutdown_write = true;
        }

        if ((how == SHUT_RD || how == SHUT_RDWR) && !shutdown_read)
            shutdown_read = true;
    }

    if (do_fin) {
        if constexpr (is_same_v<FAMILY, ipv4>)
            send_fin<FAMILY>(*this);
        else if constexpr (is_same_v<FAMILY, ipv6>) {
            if (is_mapped_address(connected_address))
                send_fin<ipv4>(*this);
            else
                send_fin<FAMILY>(*this);
        }
    }

    if (how == SHUT_WR || how == SHUT_RDWR) {
        sync_lock sl;
        lock_guard lock(sl);

        auto& e = static_cast<event&>(send_window_open_event.get());
        e.set_holding_sync_lock();

        if (!write_signalled) {
            write_signalled = true;
            wake_up_waiters(waiters, WAIT_TYPE_WRITING);
        }
    }

    if (how == SHUT_RD || how == SHUT_RDWR) {
        sync_lock sl;
        lock_guard lock(sl);

        if (!signalled) {
            wake_up_waiters(waiters, WAIT_TYPE_READING);
            signalled = true;
        }
    }

    return nt_status::SUCCESS;
}

template<socket_family FAMILY>
nt_status tcp_socket<FAMILY>::setsockopt_ipv6(sockopt_ipv6 opt, span<const uint8_t> buf) {
    if constexpr (is_same_v<FAMILY, ipv6>) {
        switch (opt) {
            using enum sockopt_ipv6;

            case V6ONLY:
                if (buf.size() != sizeof(int))
                    return nt_status::INVALID_PARAMETER;

                ipv6_only = !!*(int*)buf.data();

                return nt_status::SUCCESS;

            default:
                print("FIXME - setsockopt(IPPROTO_IPV6, {}, {})\n", opt, buf.size());
                return nt_status::NOT_SUPPORTED;
        }
    } else
        return nt_status::NOT_SUPPORTED;
}

template<socket_family FAMILY>
nt_status tcp_socket<FAMILY>::getsockopt_tcp(sockopt_tcp opt, span<uint8_t> buf,
                                             socklen_t& retlen) {
    switch (opt) {
        using enum sockopt_tcp;

        case NODELAY: {
            if (buf.size() < sizeof(int))
                return nt_status::BUFFER_TOO_SMALL;

            *(int*)buf.data() = 0;
            retlen = sizeof(int);

            return nt_status::SUCCESS;
        }

        default:
            print("FIXME - getsockopt(IPPROTO_TCP, {})\n", opt);
            return nt_status::NOT_SUPPORTED;
    }
}

template<socket_family FAMILY>
nt_status tcp_socket<FAMILY>::getsockopt_ipv6(sockopt_ipv6 opt, span<uint8_t> buf,
                                              socklen_t& retlen) {
    if constexpr (is_same_v<FAMILY, ipv6>) {
        switch (opt) {
            using enum sockopt_ipv6;

            case V6ONLY:
                if (buf.size() != sizeof(int))
                    return nt_status::INVALID_PARAMETER;

                *(int*)buf.data() = ipv6_only ? 1 : 0;
                retlen = sizeof(int);

                return nt_status::SUCCESS;

            default:
                print("FIXME - getsockopt(IPPROTO_IPV6, {}, {})\n", opt, buf.size());
                return nt_status::NOT_SUPPORTED;
        }
    } else
        return nt_status::NOT_SUPPORTED;
}
