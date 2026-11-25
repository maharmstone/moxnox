// SPDX-License-Identifier: GPL-2.0-only
module;

#include <string_view>
#include <span>
#include <bit>
#include <algorithm>
#include <atomic>
#include <variant>
#include <stddef.h>
#include <stdint.h>

export module net;

import format;
import crt;
import nt;
import list;
import spinlock;
import objects;
import chardev;
import ws2_32;
import ipv4;
import ipv6;
import object_ref;
import rwsem;
import mem;

using namespace std;

export const size_t MTU = 1500; // FIXME - allow jumbo packets
const uint8_t DEFAULT_HOP_LIMIT = 64;

export struct mac_address {
    auto operator<=>(const mac_address& other) const {
        return lexicographical_compare_three_way(v.begin(), v.end(),
                                                 other.v.begin(), other.v.end());
    }

    bool operator==(const mac_address& other) const {
        return equal(v.begin(), v.end(), other.v.begin());
    }

    array<uint8_t, 6> v;
};

const mac_address NULL_MAC_ADDRESS = { 0, 0, 0, 0, 0, 0 };
const mac_address BROADCAST_MAC_ADDRESS = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

export enum class ethertype : uint16_t { // bytes flipped
    ipv4 = 0x0008,
    arp = 0x0608,
    ipv6 = 0xdd86,
};

enum class arp_opcode : uint16_t { // bytes flipped
    request = 0x0100,
    reply = 0x0200,
};

export struct ethernet_header {
    mac_address dst_mac;
    mac_address src_mac;
    ethertype type;
} __attribute((packed));

static_assert(sizeof(ethernet_header) == 14);

export enum class ip_proto : uint8_t {
    icmpv4 = 1,
    tcp = 6,
    udp = 17,
    icmpv6 = 58
};

export struct ip_addr {
    ip_addr(const ipv4_address addr) : addr(addr) { }
    ip_addr(const ipv6_address addr) : addr(addr) { }

    list_entry le;
    variant<ipv4_address, ipv6_address> addr;
    bool slaac = false;
    bool dhcp = false;
    uint8_t prefix;
};

// FIXME - 128 on some Apple chips
static constexpr size_t CACHE_LINE = 64;

export struct alignas(CACHE_LINE) packet {
    void acquire() {
        refcount++;
    }

    void release() {
        if (--refcount == 0)
            delete this;
    }

    uint8_t data[MTU];
    uint8_t padding[MTU % CACHE_LINE ? (CACHE_LINE - (MTU % CACHE_LINE)) : 0];
    atomic<unsigned int> refcount = 1;
    size_t size;
};

export constexpr size_t PACKET_DATA_SIZE_PLUS_PADDING = offsetof(packet, refcount);

static_assert(offsetof(packet, data) % CACHE_LINE == 0);
static_assert(PACKET_DATA_SIZE_PLUS_PADDING % CACHE_LINE == 0);

struct socket_packet {
    socket_packet(packet& pack) : pack(pack) {
        pack.acquire();
    }

    ~socket_packet() {
        pack.release();
    }

    list_entry le;
    uint32_t scope_id;
    packet& pack;
};

enum dhcp_status {
    not_needed,
    need_discover,
    sent_discover,
    sent_request,
    done,
};

struct udp_header {
    uint16_t srcport;
    uint16_t dstport;
    uint16_t length;
    uint16_t checksum;
} __attribute__((packed));

static_assert(sizeof(udp_header) == 8);

constexpr auto DHCP_SERVER_PORT = byteswap((uint16_t)67);
constexpr auto DHCP_CLIENT_PORT = byteswap((uint16_t)68);

export class netif {
public:
    // FIXME - GCC 15 errors if no ctor or dtor here
    netif() { }
    virtual ~netif();

    void acquire() {
        refcount++;
    }

    void release() {
        if (--refcount == 0)
            delete this;
    }

    virtual nt_status send_packet(const mac_address& dst_mac, ethertype type,
                                  packet& pack) = 0;

    list_entry le;
    atomic<unsigned int> refcount = 1;
    mac_address mac;
    uint16_t mtu;

    spinlock addr_lock;
    list_entry addrs = { &addrs, &addrs };
    char name[16];
    ipv6_address ll_addr;
    uint32_t num;
    spinlock neighbour_lock;
    list_entry neighbour_cache = { &neighbour_cache, &neighbour_cache };
    spinlock arp_lock;
    list_entry arp_cache = { &arp_cache, &arp_cache };
    bool rs_needed = true, rs_visited = false;
    unsigned int rs_tries = 0, secs_since_last_rs = 0;
    spinlock prefix_lock;
    list_entry prefixes = { &prefixes, &prefixes };
    spinlock routers_lock;
    list_entry routers = { &routers, &routers };
    bool tx_udpv6_csum_offload = false;
    bool tx_tcpv6_csum_offload = false;
    bool dhcp_visited = false;
    bool loopback = false;
    atomic<uint64_t> rx_packets = 0, tx_packets = 0;
    atomic<uint64_t> rx_bytes = 0, tx_bytes = 0;

    spinlock dhcp_lock;
    enum dhcp_status dhcp_status = dhcp_status::need_discover;
    uint32_t dhcp_xid;
    ipv4_address dhcp_server_id;
    ipv4_address dhcp_offered_address;
    uint32_t dhcp_lease_left, dhcp_renew_time, dhcp_rebind_time;
};

export class socket : public rw_sync_object {
public:
    socket() : rw_sync_object(object_type::socket, false, true) { }
    virtual ~socket() { }

    void release() {
        if (--refcount == 0)
            delete this;
    }

    nt_status ioctl(uint32_t code, span<const uint8_t> inbuf,
                    span<uint8_t> outbuf, uintptr_t& written,
                    bool nonblock = false);
    virtual void last_handle_closed() = 0;

    virtual nt_status recvfrom(span<const span<uint8_t>> v, int flags,
                               sockaddr_storage* from, size_t* fromlen,
                               uint64_t& recv_bytes, span<HANDLE> handles,
                               size_t& received_handles) = 0;
    virtual nt_status sendto(span<const span<const uint8_t>> v, int flags,
                             sockaddr* to, size_t tolen, uint64_t& sent_bytes,
                             span<const HANDLE> handles) = 0;
    virtual nt_status bind(const sockaddr* addr, size_t addrlen) = 0;
    virtual nt_status connect(const sockaddr* addr, size_t addrlen) = 0;
    virtual nt_status connect_nonblock(const sockaddr* addr, size_t addrlen) = 0;
    virtual nt_status listen(int backlog) = 0;
    virtual nt_status accept(HANDLE& sock, void* addr, int& addrlen) = 0;
    virtual nt_status accept_nonblock(HANDLE& sock, void* addr, int& addrlen) = 0;

    nt_status read(span<const span<uint8_t>> v,
                   uint64_t& read_bytes) {
        size_t received_handles;

        return recvfrom(v, 0, nullptr, nullptr, read_bytes, span<HANDLE>(),
                        received_handles);
    }

    nt_status write(span<const span<const uint8_t>> v,
                    uint64_t& written_bytes) {
        return sendto(v, 0, nullptr, 0, written_bytes, span<HANDLE>());
    }

    virtual nt_status getsockname(span<uint8_t> addr, size_t& len) = 0;
    virtual nt_status getpeername(span<uint8_t> addr, size_t& len) = 0;

    nt_status read_nonblock(span<const span<uint8_t>> v,
                            uint64_t& read_bytes) {
        size_t received_handles;

        return recvfrom_nonblock(v, 0, nullptr, nullptr, read_bytes,
                                 span<HANDLE>(), received_handles);
    }

    nt_status write_nonblock(span<const span<const uint8_t>> v,
                             uint64_t& written_bytes) {
        return sendto_nonblock(v, 0, nullptr, 0, written_bytes, span<HANDLE>());
    }

    virtual nt_status recvfrom_nonblock(span<const span<uint8_t>> v, int flags,
                                        sockaddr_storage* from, size_t* fromlen,
                                        size_t& recv_bytes, span<HANDLE> handles,
                                        size_t& received_handles) = 0;
    virtual nt_status sendto_nonblock(span<const span<const uint8_t>> v, int flags,
                                      sockaddr* to, size_t tolen, uint64_t& sent_bytes,
                                      span<const HANDLE> handles) = 0;
    virtual nt_status shutdown(int how) = 0;

    virtual nt_status setsockopt_tcp(sockopt_tcp, span<const uint8_t>) {
        return nt_status::NOT_SUPPORTED;
    }

    virtual nt_status setsockopt_ipv6(sockopt_ipv6, span<const uint8_t>) {
        return nt_status::NOT_SUPPORTED;
    }

    virtual nt_status getsockopt_tcp(sockopt_tcp, span<uint8_t>, socklen_t&) {
        return nt_status::NOT_SUPPORTED;
    }

    virtual nt_status getsockopt_ipv6(sockopt_ipv6, span<uint8_t>, socklen_t&) {
        return nt_status::NOT_SUPPORTED;
    }

private:
    nt_status do_recvmsg(span<const uint8_t> inbuf, bool nonblock);
    nt_status do_sendmsg(span<const uint8_t> inbuf, bool nonblock);
    nt_status do_bind(span<const uint8_t> inbuf);
    nt_status do_connect(span<const uint8_t> inbuf, bool nonblock);
    nt_status do_listen(span<const uint8_t> inbuf);
    nt_status do_accept(span<const uint8_t> inbuf, span<uint8_t> outbuf,
                        uintptr_t& written_bytes, bool nonblock);
    nt_status do_getsockname(span<const uint8_t> inbuf, span<uint8_t> outbuf,
                             uintptr_t& written_bytes);
    nt_status do_getpeername(span<const uint8_t> inbuf, span<uint8_t> outbuf,
                             uintptr_t& written_bytes);
    nt_status do_getsockopt(span<const uint8_t> inbuf);
    nt_status getsockopt(int level, int optname, span<uint8_t> buf,
                         socklen_t& retlen);
    nt_status getsockopt_error(span<uint8_t> buf, socklen_t& retlen);
    nt_status do_setsockopt(span<const uint8_t> inbuf);
    nt_status setsockopt(int level, int optname, span<const uint8_t> buf);
    nt_status do_shutdown(span<const uint8_t> inbuf);
};

static_assert(has_ioctls<class socket>);
static_assert(is_sync_object<class socket>);
static_assert(has_last_handle_closed<class socket>);
static_assert(is_readable_object_without_offset<class socket>);
static_assert(is_writable_object_without_offset<class socket>);
static_assert(is_nonblocking_readable_vec_object<class socket>);
static_assert(is_nonblocking_writable_vec_object<class socket>);

class netctl : public chardev {
public:
    netctl() { }

    nt_status read(span<const span<uint8_t>>, uint64_t&) override {
        return nt_status::NOT_IMPLEMENTED;
    }

    nt_status write(span<const span<const uint8_t>>, uint64_t&) override {
        return nt_status::NOT_IMPLEMENTED;
    }

    nt_status ioctl(uint32_t code, span<const uint8_t> inbuf,
                    span<uint8_t> outbuf, uintptr_t& written) override;
};

struct ipv4_route {
    ipv4_route(netif& ni, const ipv4_address& prefix, uint8_t prefix_length,
               const ipv4_address& gateway, bool dhcp) : ni(ni), prefix(prefix), prefix_length(prefix_length), gateway(gateway), dhcp(dhcp) {
        ni.acquire();
    }

    ~ipv4_route() {
        ni.release();
    }

    list_entry le;
    netif& ni;
    ipv4_address prefix;
    uint8_t prefix_length;
    ipv4_address gateway;
    bool dhcp;
};

template<>
consteval bool format_spec_valid<mac_address>(string_view fmt) {
    return fmt.empty();
}

export span<char> do_format_arg(span<char> sp, string_view, const mac_address& m) {
    sp = do_format_arg(sp, ":02x", m.v[0]);
    sp = do_format_arg(sp, "", ":");
    sp = do_format_arg(sp, ":02x", m.v[1]);
    sp = do_format_arg(sp, "", ":");
    sp = do_format_arg(sp, ":02x", m.v[2]);
    sp = do_format_arg(sp, "", ":");
    sp = do_format_arg(sp, ":02x", m.v[3]);
    sp = do_format_arg(sp, "", ":");
    sp = do_format_arg(sp, ":02x", m.v[4]);
    sp = do_format_arg(sp, "", ":");
    sp = do_format_arg(sp, ":02x", m.v[5]);

    return sp;
}

template<>
consteval bool format_spec_valid<ethertype>(string_view fmt) {
    return fmt.empty();
}

export span<char> do_format_arg(span<char> sp, string_view, ethertype e) {
    switch (e) {
        using enum ethertype;

        case ipv4:
            return do_format_arg(sp, "", "ipv4");
        case arp:
            return do_format_arg(sp, "", "arp");
        case ipv6:
            return do_format_arg(sp, "", "ipv6");
        default:
            return do_format_arg(sp, ":04x", byteswap((uint16_t)e));
    }

    return sp;
}

template<>
consteval bool format_spec_valid<ip_proto>(string_view fmt) {
    return fmt.empty();
}

export span<char> do_format_arg(span<char> sp, string_view, ip_proto p) {
    switch (p) {
        using enum ip_proto;

        case icmpv4:
            return do_format_arg(sp, "", "icmpv4");
        case tcp:
            return do_format_arg(sp, "", "tcp");
        case udp:
            return do_format_arg(sp, "", "udp");
        case icmpv6:
            return do_format_arg(sp, "", "icmpv6");
        default:
            return do_format_arg(sp, ":02x", (uint8_t)p);
    }

    return sp;
}

template<>
consteval bool format_spec_valid<icmpv6_type>(string_view fmt) {
    return fmt.empty();
}

export span<char> do_format_arg(span<char> sp, string_view, icmpv6_type t) {
    switch (t) {
        using enum icmpv6_type;

        case dest_unreachable:
            return do_format_arg(sp, "", "dest_unreachable");
        case packet_too_big:
            return do_format_arg(sp, "", "packet_too_big");
        case time_exceeded:
            return do_format_arg(sp, "", "time_exceeded");
        case param_problem:
            return do_format_arg(sp, "", "param_problem");
        case echo_request:
            return do_format_arg(sp, "", "echo_request");
        case echo_reply:
            return do_format_arg(sp, "", "echo_reply");
        case router_solicitation:
            return do_format_arg(sp, "", "router_solicitation");
        case router_advertisement:
            return do_format_arg(sp, "", "router_advertisement");
        case neighbour_solicitation:
            return do_format_arg(sp, "", "neighbour_solicitation");
        case neighbour_advertisement:
            return do_format_arg(sp, "", "neighbour_advertisement");
        default:
            return do_format_arg(sp, ":02x", (uint8_t)t);
    }

    return sp;
}

template<>
consteval bool format_spec_valid<netif>(string_view fmt) {
    return fmt.empty();
}

export span<char> do_format_arg(span<char> sp, string_view, const netif& ni) {
    auto sv = string_view(ni.name, sizeof(ni.name));

    if (auto zero = sv.find('\0'); zero != string_view::npos)
        sv = string_view(sv.data(), zero);

    return do_format_arg(sp, "", sv);
}

export spinlock netif_lock;
export list_entry netif_list = { &netif_list, &netif_list };
export atomic<uint32_t> next_netif_num = 1;
export atomic<uint32_t> next_eth_num = 0;

export struct ipv4 {
    using header = ipv4_header;
    using address = ipv4_address;
    using sockaddr = sockaddr_in;
    using icmp_header = icmpv4_header;

    static constexpr int family = AF_INET;
    static constexpr auto ethertype = ethertype::ipv4;
};

export struct ipv6 {
    using header = ipv6_header;
    using address = ipv6_address;
    using sockaddr = sockaddr_in6;
    using icmp_header = icmpv6_header;

    static constexpr int family = AF_INET6;
    static constexpr auto ethertype = ethertype::ipv6;
};

template<typename T>
concept socket_family = is_same_v<T, ipv4> || is_same_v<T, ipv6>;

static_assert(socket_family<ipv4>);
static_assert(socket_family<ipv6>);

ipv6_address map_ipv4_address_to_ipv6(const ipv4_address& addr) {
    ipv6_address ret;

    static const auto MAPPED_ADDRESS = parse_ipv6_address("::ffff:0:0");

    ret = MAPPED_ADDRESS;
    memcpy(ret.v.data() + 12, addr.v.data(), sizeof(ipv4_address));

    return ret;
}

bool is_mapped_address(const ipv6_address& addr) {
    static const auto MAPPED_ADDRESS = parse_ipv6_address("::ffff:0:0");

    return !memcmp(addr.v.data(), MAPPED_ADDRESS.v.data(), 12);
}

ipv4_address extract_mapped_address(const ipv6_address& addr) {
    return *(ipv4_address*)(addr.v.data() + 12);
}

export void received_packet(netif& ni, packet& pack, const mac_address& src_mac,
                            ethertype type, bool skip_csumming);
export bool drop_packet(const mac_address& dst, const mac_address& us);
export bool init_net();
export ipv6_address find_ll_address(const mac_address& m);

spinlock ipv4_routes_lock;
list_entry ipv4_routes = { &ipv4_routes, &ipv4_routes };

void received_ipv4_tcp_packet(netif& ni, packet& pack,
                              const mac_address& src_mac, bool skip_checksum);
void received_ipv6_tcp_packet(netif& ni, packet& pack,
                              const mac_address& src_mac, bool skip_checksum);
void received_ipv4_udp_packet(netif& ni, packet& pack,
                              const mac_address& src_mac, bool skip_checksum);
void received_ipv6_udp_packet(netif& ni, packet& pack,
                              const mac_address& src_mac, bool skip_checksum);
bool is_our_ip(const ipv4_address& ip);
bool is_our_ip(const ipv6_address& ip);

template<socket_family FAMILY>
nt_status dispatch_packet(netif& ni, packet& pack, const mac_address& dst_mac);

uint16_t calc_ipv4_checksum(const ipv4_header& ip, span<const uint8_t> data,
                            uint8_t protocol);
uint16_t calc_ipv6_checksum(const ipv6_header& ip, span<const uint8_t> data,
                            uint8_t next_header);
uint16_t calc_ipv6_checksum_offload(const ipv6_header& ip, uint32_t length,
                                    uint8_t next_header);
nt_status ipv4_choose_interface(const ipv4_address& dst, netif*& ni,
                                ipv4_address& src, ipv4_address& gateway);
nt_status ipv6_choose_interface(const ipv6_address& dst, uint32_t scope_id,
                                netif*& ni, ipv6_address& src);
bool is_multicast_address(const ipv4_address& addr);
bool is_multicast_address(const ipv6_address& addr);
bool is_broadcast_address(netif& ni, const ipv4_address& dst);
void send_address_unreachable(netif& ni, const packet& pack,
                              const ipv4_address& addr);
void send_address_unreachable(netif& ni, const packet& pack,
                              const ipv6_address& addr);
mac_address find_mac_address(netif& ni, const ipv4_address& dst);
mac_address find_mac_address(netif& ni, const ipv6_address& dst);
void handle_icmpv4_error_udp(packet& pack);
void handle_icmpv6_error_udp(packet& pack);
nt_status create_new_ipv4_tcp_socket(object_ref& ref);
nt_status create_new_ipv6_tcp_socket(object_ref& ref);
nt_status create_new_ipv4_udp_socket(object_ref& ref);
nt_status create_new_ipv6_udp_socket(object_ref& ref);
nt_status create_new_unix_socket(object_ref& ref);
void maybe_add_to_neighbour_cache(netif& ni, const ipv6_address& dst,
                                  const mac_address& mac);
nt_status create_unix_socket_pair(int type, object_ref& ref1, object_ref& ref2);
export void add_ipv4_route(netif& ni, const ipv4_address& prefix,
                           uint8_t prefix_length, const ipv4_address& gateway,
                           bool dhcp = false);
void send_dhcp_discover(netif& ni);
ipv4_address calculate_broadcast_address(const ipv4_address& addr, uint8_t prefix);
ipv4_address zero_right_bits(const ipv4_address& addr, uint8_t n);
void add_dns_server(const ipv6_address& address, uint32_t ttl);
void handle_dhcp_expiry(netif& ni);
void do_dhcp_renewal(netif& ni, bool broadcast);
void received_dhcp_packet(netif& ni, packet& pack);
