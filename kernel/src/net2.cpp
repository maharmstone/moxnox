// SPDX-License-Identifier: GPL-2.0-only
module;

#include <array>
#include <span>
#include <mutex>
#include <bit>
#include <atomic>
#include <optional>
#include <utility>
#include <variant>
#include <string_view>
#include <algorithm>
#include <stddef.h>
#include <stdint.h>
#include <sys/uio.h>

module net;

import bsod;
import print;
import handle;
import mem;
import moxnox_ioctls;
import thread;
import config;
import scheduler;
import event;
import kernel_time_page;
import timer;
import format;
import vfs;
import rangelock;
import process;
import vec;

using namespace std;

static const size_t MAX_BUFFER_SIZE = 1048576;

static object_ref netctl_obj;

static spinlock ipv6_raw_sockets_lock;
static list_entry ipv6_raw_sockets = { &ipv6_raw_sockets, &ipv6_raw_sockets };
static size_t num_ipv6_raw_sockets = 0;

static spinlock ipv4_raw_sockets_lock;
static list_entry ipv4_raw_sockets = { &ipv4_raw_sockets, &ipv4_raw_sockets };
static size_t num_ipv4_raw_sockets = 0;

static atomic<uint64_t> lo_packets = 0, lo_bytes = 0;
static netif* lo_netif = nullptr;

static spinlock hostname_lock;

template<socket_family FAMILY>
class raw_socket : public socket {
public:
    raw_socket(int protocol) : protocol(protocol) { }
    ~raw_socket() override;
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

    nt_status connect(const sockaddr*, size_t) override {
        return nt_status::INVALID_PARAMETER;
    }

    nt_status listen(int) override {
        return nt_status::INVALID_PARAMETER;
    }

    nt_status accept(HANDLE&, void*, int&) override {
        return nt_status::INVALID_PARAMETER;
    }

    nt_status getsockname(span<uint8_t> buf, size_t& len) override;

    nt_status getpeername(span<uint8_t>, size_t&) override {
        return nt_status::CONNECTION_DISCONNECTED;
    }

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

    nt_status shutdown(int) override {
        return nt_status::NOT_SUPPORTED;
    }

    list_entry le;
    int protocol;
    bool added_to_list = false;
    spinlock buffer_lock;
    list_entry buffer = { &buffer, &buffer };
    size_t buffer_size = 0;
    spinlock bind_lock;
    bool bind_done = false;
    typename FAMILY::address bound_address;
    nt_status pending_error = nt_status::SUCCESS;
    bool dead = false;

private:
    template<bool NONBLOCK>
    nt_status do_recvfrom(span<const span<uint8_t>> v, int flags,
                          sockaddr_storage* from, size_t* fromlen,
                          uint64_t& recv_bytes);

    template<bool NONBLOCK>
    nt_status do_sendto(span<const span<const uint8_t>> v, int flags,
                        sockaddr* to, size_t tolen, uint64_t& sent_bytes);
};

enum class neighbour_cache_status {
    incomplete,
    reachable
};

template<socket_family FAMILY>
struct neighbour_cache_entry {
    neighbour_cache_entry(netif& ni) : ni(ni) {
        ni.acquire();
    }

    ~neighbour_cache_entry() {
        ni.release();
    }

    void acquire() {
        refcount++;
    }

    void release() {
        if (--refcount == 0)
            delete this;
    }

    list_entry le;
    netif& ni;
    typename FAMILY::address addr;
    atomic<unsigned int> refcount = 1;
    object_ref event;
    mac_address mac;
    neighbour_cache_status status;
    int64_t last_update_time;
};

struct ipv6_route {
    ipv6_route(netif& ni, const ipv6_address& prefix, uint8_t prefix_length) : ni(ni), prefix(prefix), prefix_length(prefix_length) {
        ni.acquire();
    }

    ~ipv6_route() {
        ni.release();
    }

    list_entry le;
    netif& ni;
    ipv6_address prefix;
    uint8_t prefix_length;
};

struct ipv6_prefix {
    list_entry le;
    ipv6_address prefix;
    uint8_t prefix_length;
    uint32_t preferred_lifetime;
    uint32_t valid_lifetime;
    bool tick_visited = false;
    bool on_link;
};

struct ipv6_router {
    list_entry le;
    ipv6_address addr;
    mac_address mac;
    uint32_t lifetime;
    bool tick_visited = false;
};

struct icmpv6_neighbour_solicitation : icmpv6_header {
    uint32_t reserved;
    ipv6_address target;
} __attribute((packed));

static_assert(sizeof(icmpv6_neighbour_solicitation) == 24);

struct icmpv6_neighbour_advertisement : icmpv6_header {
    uint32_t : 5;
    uint32_t _override : 1;
    uint32_t solicited : 1;
    uint32_t router : 1;
    uint32_t : 24;
    ipv6_address target;
} __attribute((packed));

static_assert(sizeof(icmpv6_neighbour_advertisement) == 24);

struct icmpv6_router_solicitation : icmpv6_header {
    uint32_t reserved;
} __attribute((packed));

static_assert(sizeof(icmpv6_router_solicitation) == 8);

struct icmpv6_router_advertisement : icmpv6_header {
    uint8_t cur_hop_limit;
    uint8_t : 1;
    uint8_t snac_router : 1;
    uint8_t nd_proxy : 1;
    uint8_t prf : 2;
    uint8_t home_agent : 1;
    uint8_t other_configuration : 1;
    uint8_t managed_address_configuration : 1;
    uint16_t router_lifetime;
    uint32_t reachable_time;
    uint32_t retrans_timer;
} __attribute((packed));

static_assert(sizeof(icmpv6_router_advertisement) == 16);

enum class ndp_option_type : uint8_t {
    source_lladdr = 1,
    target_lladdr = 2,
    prefix_info = 3,
    redirected_header = 4,
    mtu = 5,
    rdnss = 25,
};

struct ndp_option {
    ndp_option_type type;
    uint8_t length;
} __attribute((packed));

static_assert(sizeof(ndp_option) == 2);

struct ndp_option_lladdr : ndp_option {
    mac_address mac;
} __attribute((packed));

static_assert(sizeof(ndp_option_lladdr) == 8);

struct ndp_option_prefix_info : ndp_option  {
    uint8_t prefix_length;
    uint8_t : 4;
    uint8_t dhcpv6_pd_preferred : 1;
    uint8_t router_address : 1;
    uint8_t autonomous : 1;
    uint8_t onlink : 1;
    uint32_t valid_lifetime;
    uint32_t preferred_lifetime;
    uint32_t reserved;
    ipv6_address prefix;
} __attribute__((packed));

static_assert(sizeof(ndp_option_prefix_info) == 32);

struct ndp_option_rdnss : ndp_option {
    uint16_t reserved;
    uint32_t lifetime;
    ipv6_address servers[1];
} __attribute__((packed));

static_assert(offsetof(ndp_option_rdnss, servers) == 8);

struct dns_server {
    list_entry le;
    uint32_t ttl;
    ipv6_address address;
};

class loopback_netif : public netif {
public:
    nt_status send_packet(const mac_address& dst_mac, ethertype type,
                          packet& pack) override;
};

static spinlock ipv6_routes_lock;
static list_entry ipv6_routes = { &ipv6_routes, &ipv6_routes };
static spinlock dns_servers_lock;
static list_entry dns_servers = { &dns_servers, &dns_servers };

static void handle_ipv4_packet(netif& ni, packet& pack, const mac_address& src_mac,
                               bool skip_checksum);
static void handle_ipv6_packet(netif& ni, packet& pack, const mac_address& src_mac,
                               bool skip_checksum);

uint16_t calc_ipv4_checksum(const ipv4_header& ip, span<const uint8_t> data,
                            uint8_t protocol) {
    struct {
        ipv4_address src;
        ipv4_address dst;
        uint8_t zero;
        uint8_t protocol;
        uint16_t length;
    } __attribute__((packed)) pseudo;

    uint32_t sum = 0;

    pseudo.src = ip.src;
    pseudo.dst = ip.dst;
    pseudo.length = byteswap((uint16_t)data.size());
    pseudo.protocol = protocol;
    pseudo.zero = 0;

    for (auto s : span((uint16_t*)&pseudo, sizeof(pseudo) / sizeof(uint16_t))) {
        sum += byteswap(s);
    }

    for (auto s : span((uint16_t*)data.data(), data.size() / sizeof(uint16_t))) {
        sum += byteswap(s);
    }

    if (data.size() & 1)
        sum += (uint16_t)(data[data.size() - 1] << 8);

    while (sum & 0xffff0000) {
        sum = (sum >> 16) + (sum & 0xffff);
    }

    sum = byteswap((uint16_t)(~(uint16_t)sum));

    return (uint16_t)sum;
}

uint16_t calc_ipv6_checksum(const ipv6_header& ip, span<const uint8_t> data,
                            uint8_t next_header) {
    struct {
        ipv6_address src;
        ipv6_address dst;
        uint32_t length;
        uint8_t zero[3];
        uint8_t next_header;
    } __attribute__((packed)) pseudo;

    uint32_t sum = 0;

    pseudo.src = ip.src;
    pseudo.dst = ip.dst;
    pseudo.length = byteswap((uint32_t)data.size());
    pseudo.zero[0] = 0;
    pseudo.zero[1] = 0;
    pseudo.zero[2] = 0;
    pseudo.next_header = next_header;

    for (auto s : span((uint16_t*)&pseudo, sizeof(pseudo) / sizeof(uint16_t))) {
        sum += byteswap(s);
    }

    for (auto s : span((uint16_t*)data.data(), data.size() / sizeof(uint16_t))) {
        sum += byteswap(s);
    }

    if (data.size() & 1)
        sum += (uint16_t)(data[data.size() - 1] << 8);

    while (sum & 0xffff0000) {
        sum = (sum >> 16) + (sum & 0xffff);
    }

    return byteswap((uint16_t)(~(uint16_t)sum));
}

uint16_t calc_ipv6_checksum_offload(const ipv6_header& ip, uint32_t length,
                                    uint8_t next_header) {
    struct {
        ipv6_address src;
        ipv6_address dst;
        uint32_t length;
        uint8_t zero[3];
        uint8_t next_header;
    } __attribute__((packed)) pseudo;

    uint32_t sum = 0;

    pseudo.src = ip.src;
    pseudo.dst = ip.dst;
    pseudo.length = byteswap(length);
    pseudo.zero[0] = 0;
    pseudo.zero[1] = 0;
    pseudo.zero[2] = 0;
    pseudo.next_header = next_header;

    for (auto s : span((uint16_t*)&pseudo, sizeof(pseudo) / sizeof(uint16_t))) {
        sum += byteswap(s);
    }

    while (sum & 0xffff0000) {
        sum = (sum >> 16) + (sum & 0xffff);
    }

    return byteswap((uint16_t)sum);
}

static void send_neighbour_advertisement(netif& ni, const ipv6_address& target,
                                         const ipv6_address& dst_ip,
                                         const mac_address& dst_mac) {
    auto pack = new packet;
    if (!pack) {
        print("new failed\n");
        return;
    }

    struct na_pkt {
        ipv6_header ip;
        icmpv6_neighbour_advertisement na;
        ndp_option_lladdr opt;
    } __attribute__((packed));

    pack->size = sizeof(na_pkt);
    auto& pkt = *(na_pkt*)pack->data;

    memset(&pkt, 0, sizeof(pkt));

    pkt.ip.version = 6;
    pkt.ip.payload_length = byteswap((uint16_t)(sizeof(icmpv6_neighbour_advertisement) + sizeof(ndp_option_lladdr)));
    pkt.ip.next_header = (uint8_t)ip_proto::icmpv6;
    pkt.ip.hop_limit = 255;
    pkt.ip.src = ni.ll_addr;
    pkt.ip.dst = dst_ip;

    pkt.na.type = icmpv6_type::neighbour_advertisement;
    pkt.na.solicited = 1;
    pkt.na._override = 1;
    pkt.na.target = target;

    pkt.opt.type = ndp_option_type::target_lladdr;
    pkt.opt.length = 1; // 8 bytes
    pkt.opt.mac = ni.mac;

    pkt.na.checksum = calc_ipv6_checksum(pkt.ip, span((uint8_t*)&pkt.na, sizeof(pkt.na) + sizeof(pkt.opt)),
                                         IPPROTO_ICMPV6);

    auto status = ni.send_packet(dst_mac, ethertype::ipv6, *pack);
    if (!success(status))
        print("send_packet returned {}\n", status);

    pack->release();
}

static bool valid_ndp_packet(const packet& pack) {
    const auto& ip = *(ipv6_header*)pack.data;

    if (ip.hop_limit != 255)
        return false;

    const auto& icmp = *(icmpv6_header*)(pack.data + sizeof(ipv6_header));

    if (calc_ipv6_checksum(ip, span((uint8_t*)&icmp, pack.size - sizeof(ipv6_header)), IPPROTO_ICMPV6) != 0)
        return false;

    return true;
}

static void handle_neighbour_solicitation(netif& ni, packet& pack,
                                          const mac_address& src_mac) {
    if (pack.size < sizeof(ipv6_header) + sizeof(icmpv6_neighbour_solicitation)) {
        print("handle_neighbour_solicitation: buffer length was {}, expected at least {}\n",
              pack.size, sizeof(ipv6_header) + sizeof(icmpv6_neighbour_solicitation));
        return;
    }

    if (!valid_ndp_packet(pack))
        return;

    auto& ip = *(ipv6_header*)pack.data;
    auto& ns = *(icmpv6_neighbour_solicitation*)(pack.data + sizeof(ipv6_header));

#if 0
    print("ns: target = {}\n", ns.target);
#endif

    // FIXME - options (sender's MAC means we can avoid sending NS back to them)

    bool found = false;
    {
        lock_guard lock(ni.addr_lock);

        for (auto le = ni.addrs.next; le != &ni.addrs; le = le->next) {
            auto& a = containing_record<ip_addr>(le);

            if (auto* addr = get_if<ipv6_address>(&a.addr)) {
                if (*addr == ns.target) {
                    found = true;
                    break;
                }
            }
        }
    }

    if (!found)
        return;

    // FIXME - if source was :: (DAD), send to 33:33:00:00:00:01 and ff02::1

    send_neighbour_advertisement(ni, ns.target, ip.src, src_mac);
}

static void add_neighbour_cache_entry(netif& ni, const mac_address& mac,
                                      const ipv6_address& addr,
                                      bool do_override) {
    {
        lock_guard lock(ni.neighbour_lock);

        for (auto le = ni.neighbour_cache.next; le != &ni.neighbour_cache; le = le->next) {
            auto& c = containing_record<neighbour_cache_entry<ipv6>>(le);

            if (c.addr != addr)
                continue;

            if (!do_override && c.status == neighbour_cache_status::reachable)
                return;

            c.mac = mac;

            if (c.status == neighbour_cache_status::incomplete) {
                c.status = neighbour_cache_status::reachable;
                c.last_update_time = kernel_time_page.system_time;
                set_event(static_cast<event&>(c.event.get()));
            }

            return;
        }
    }

    // add new entry

    auto nce = new neighbour_cache_entry<ipv6>(ni);
    if (!nce) {
        print("new failed\n");
        return;
    }

    nce->addr = addr;
    nce->mac = mac;

    nce->event = create_event(false);
    if (!nce->event) {
        print("create_event failed\n");
        nce->release();
        return;
    }

    set_event(static_cast<event&>(nce->event.get()));

    nce->status = neighbour_cache_status::reachable;

    bool beaten_to_it = false;

    {
        lock_guard lock(ni.neighbour_lock);

        for (auto le = ni.neighbour_cache.next; le != &ni.neighbour_cache; le = le->next) {
            auto& c = containing_record<neighbour_cache_entry<ipv6>>(le);

            if (c.addr == addr) {
                beaten_to_it = true;

                c.mac = mac;

                if (c.status == neighbour_cache_status::incomplete) {
                    c.status = neighbour_cache_status::reachable;
                    set_event(static_cast<event&>(c.event.get()));
                }

                c.last_update_time = kernel_time_page.system_time;

                break;
            }
        }

        if (!beaten_to_it) {
            insert_tail_list(ni.neighbour_cache, nce->le);
            nce->last_update_time = kernel_time_page.system_time;
        }
    }

    if (beaten_to_it)
        nce->release();
}

static void handle_neighbour_advertisement(netif& ni, packet& pack) {
    if (pack.size < sizeof(ipv6_header) + sizeof(icmpv6_neighbour_advertisement)) {
        print("handle_neighbour_advertisement: buffer length was {}, expected at least {}\n",
              pack.size, sizeof(ipv6_header) + sizeof(icmpv6_neighbour_advertisement));
        return;
    }

    if (!valid_ndp_packet(pack))
        return;

    auto& na = *(icmpv6_neighbour_advertisement*)(pack.data + sizeof(ipv6_header));

#if 0
    print("na: override = {}, solicited = {}, router = {}, target = {}\n",
          na._override, na.solicited, na.router, na.target);
#endif

    auto opts = span(pack.data + sizeof(ipv6_header) + sizeof(icmpv6_neighbour_advertisement),
                     pack.size - sizeof(ipv6_header) - sizeof(icmpv6_neighbour_advertisement));

    mac_address mac = NULL_MAC_ADDRESS;

    while (opts.size() >= 8) {
        auto& opt = *(ndp_option*)opts.data();

        if (opts.size() < opt.length * 8)
            break;

        if (opt.type == ndp_option_type::target_lladdr) {
            auto& opt2 = *(ndp_option_lladdr*)&opt;

            mac = opt2.mac;

            break;
        }

        opts = span(opts.data() + (opt.length * 8), opts.size() - (opt.length * 8));
    }

    if (mac == NULL_MAC_ADDRESS) {
        print("neighbour advertisement did not provide a MAC address\n");
        return;
    }

    // FIXME - if !na.solicited, update cache status to stale

    add_neighbour_cache_entry(ni, mac, na.target, na._override);
}

bool is_multicast_address(const ipv4_address& addr) {
    return (addr.v[0] & 0xf0) == 0xe0;
}

bool is_multicast_address(const ipv6_address& addr) {
    static const auto multicast_range = parse_ipv6_address("ff00::");

    return !memcmp(multicast_range.v.data(), addr.v.data(), 1);
}

static void handle_v6_echo_request(netif& ni, packet& pack,
                                   const mac_address& src_mac, bool skip_checksum) {
    if (pack.size < sizeof(ipv6_header) + sizeof(icmpv6_echo)) {
        print("handle_v6_echo_request: buffer length was {}, expected at least {}\n",
              pack.size, sizeof(ipv6_header) + sizeof(icmpv6_echo));
        return;
    }

    const auto& orig_ip = *(ipv6_header*)pack.data;
    const auto& icmp = *(icmpv6_header*)(pack.data + sizeof(ipv6_header));

    if (!skip_checksum) {
        if (calc_ipv6_checksum(orig_ip, span((uint8_t*)&icmp, pack.size - sizeof(ipv6_header)), IPPROTO_ICMPV6) != 0)
            return;
    }

    auto pack2 = new packet;
    if (!pack2) {
        print("new failed\n");
        return;
    }

    // take copy

    pack2->size = pack.size;
    memcpy(pack2->data, pack.data, pack.size);

    // switch IP addresses

    auto& ip = *(ipv6_header*)pack2->data;
    swap(ip.src, ip.dst);

    if (is_multicast_address(ip.src))
        ip.src = ni.ll_addr;

    ip.hop_limit = DEFAULT_HOP_LIMIT;

    // change type to reply

    auto& header = *(icmpv6_header*)(pack2->data + sizeof(ipv6_header));
    header.type = icmpv6_type::echo_reply;
    header.checksum = 0;

    header.checksum = calc_ipv6_checksum(ip, span((uint8_t*)&header, pack2->size - sizeof(ipv6_header)), IPPROTO_ICMPV6);

    if (ip.src == ip.dst) // pinging ourselves
        handle_ipv6_packet(ni, *pack2, ni.mac, true);
    else {
        auto status = ni.send_packet(src_mac, ethertype::ipv6, *pack2);
        if (!success(status))
            print("send_packet returned {}\n", status);
    }

    pack2->release();
}

void add_ipv4_route(netif& ni, const ipv4_address& prefix,
                    uint8_t prefix_length, const ipv4_address& gateway,
                    bool dhcp) {
    {
        lock_guard lock(ipv4_routes_lock);

        for (auto le = ipv4_routes.next; le != &ipv4_routes; le = le->next) {
            auto& r = containing_record<ipv4_route>(le);

            if (r.prefix == prefix && r.prefix_length == prefix_length &&
                r.gateway == gateway && &r.ni == &ni) {
                return;
            }
        }
    }

    auto r = new ipv4_route(ni, prefix, prefix_length, gateway, dhcp);
    if (!r) {
        print("new failed\n");
        return;
    }

    bool beaten_to_it = false;

    {
        lock_guard lock(ipv4_routes_lock);

        for (auto le = ipv4_routes.next; le != &ipv4_routes; le = le->next) {
            auto& r2 = containing_record<ipv4_route>(le);

            if (r2.prefix == prefix && r2.prefix_length == prefix_length &&
                r2.gateway == gateway && &r2.ni == &ni) {
                beaten_to_it = true;
                break;
            }
        }

        if (!beaten_to_it)
            insert_tail_list(ipv4_routes, r->le);
    }

    if (beaten_to_it)
        delete r;
}

static void add_ipv6_route(netif& ni, const ipv6_address& prefix,
                           uint8_t prefix_length) {
    {
        lock_guard lock(ipv6_routes_lock);

        for (auto le = ipv6_routes.next; le != &ipv6_routes; le = le->next) {
            auto& r = containing_record<ipv6_route>(le);

            if (r.prefix == prefix && r.prefix_length == prefix_length &&
                &r.ni == &ni) {
                return;
            }
        }
    }

    auto r = new ipv6_route(ni, prefix, prefix_length);
    if (!r) {
        print("new failed\n");
        return;
    }

    bool beaten_to_it = false;

    {
        lock_guard lock(ipv6_routes_lock);

        for (auto le = ipv6_routes.next; le != &ipv6_routes; le = le->next) {
            auto& r2 = containing_record<ipv6_route>(le);

            if (r2.prefix == prefix && r2.prefix_length == prefix_length &&
                &r2.ni == &ni) {
                beaten_to_it = true;
                break;
            }
        }

        if (!beaten_to_it)
            insert_tail_list(ipv6_routes, r->le);
    }

    if (beaten_to_it)
        delete r;
}

static void remove_ipv6_route(netif& ni, const ipv6_address& prefix,
                              uint8_t prefix_length) {
    ipv6_route* found = nullptr;

    {
        lock_guard lock(ipv6_routes_lock);

        for (auto le = ipv6_routes.next; le != &ipv6_routes; le = le->next) {
            auto& r = containing_record<ipv6_route>(le);

            if (r.prefix == prefix && r.prefix_length == prefix_length &&
                &r.ni == &ni) {
                found = &r;
                remove_entry_list(r.le);
                break;
            }
        }
    }

    if (found)
        delete found;
}

static void add_ipv6_prefix(netif& ni, const ipv6_address& prefix,
                            uint8_t prefix_length, uint32_t valid_lifetime,
                            uint32_t preferred_lifetime, bool on_link,
                            bool autonomous) {
    ipv6_prefix* found = nullptr;

    {
        lock_guard lock(ni.prefix_lock);

        for (auto le = ni.prefixes.next; le != &ni.prefixes; le = le->next) {
            auto& p = containing_record<ipv6_prefix>(le);

            if (p.prefix == prefix && p.prefix_length == prefix_length) {
                if (valid_lifetime == 0) {
                    found = &p;
                    remove_entry_list(p.le);
                    break;
                }

                p.valid_lifetime = valid_lifetime;
                p.preferred_lifetime = preferred_lifetime;
                p.on_link = on_link;
                return;
            }
        }
    }

    if (found) {
        delete found;
        remove_ipv6_route(ni, prefix, prefix_length);

        if (prefix_length == 64) {
            // delete SLAAC addresses

            while (true) {
                ip_addr* dead_addr = nullptr;

                {
                    lock_guard lock(ni.addr_lock);

                    for (auto le = ni.addrs.next; le != &ni.addrs; le = le->next) {
                        auto& a = containing_record<ip_addr>(le);

                        auto* addr = get_if<ipv6_address>(&a.addr);

                        if (!addr)
                            continue;

                        if (!a.slaac)
                            continue;

                        if (!memcmp(addr->v.data(), prefix.v.data(), 8)) {
                            remove_entry_list(a.le);
                            dead_addr = &a;
                            break;
                        }
                    }
                }

                if (!dead_addr)
                    break;

                print("{}: removing IPv6 address {}\n", ni, *get_if<ipv6_address>(&dead_addr->addr));
                delete dead_addr;
            }
        }

        return;
    }

    if (valid_lifetime == 0)
        return;

    auto p = new ipv6_prefix;
    if (!p) {
        print("new failed\n");
        return;
    }

    p->prefix = prefix;
    p->prefix_length = prefix_length;
    p->preferred_lifetime = preferred_lifetime;
    p->valid_lifetime = valid_lifetime;
    p->on_link = on_link;

    bool beaten_to_it = false;

    {
        lock_guard lock(ni.prefix_lock);

        for (auto le = ni.prefixes.next; le != &ni.prefixes; le = le->next) {
            auto& p2 = containing_record<ipv6_prefix>(le);

            if (p2.prefix == prefix && p2.prefix_length == prefix_length) {
                p2.valid_lifetime = valid_lifetime;
                p2.preferred_lifetime = preferred_lifetime;
                p2.on_link = on_link;
                beaten_to_it = true;
                break;
            }
        }

        if (!beaten_to_it)
            insert_tail_list(ni.prefixes, p->le);
    }

    if (prefix_length == 64) {
        if (autonomous) {
            ipv6_address slaac;

            memcpy(slaac.v.data(), prefix.v.data(), 8);
            memcpy(&slaac.v[8], &ni.ll_addr.v[8], 8);

            bool already_exists = false;

            {
                lock_guard lock(ni.addr_lock);

                for (auto le = ni.addrs.next; le != &ni.addrs; le = le->next) {
                    auto& a = containing_record<ip_addr>(le);

                    if (auto* addr = get_if<ipv6_address>(&a.addr)) {
                        if (*addr == slaac) {
                            already_exists = true;
                            break;
                        }
                    }
                }
            }

            if (!already_exists) {
                auto a = new ip_addr(slaac);
                if (!a)
                    print("new failed\n");
                else {
                    a->slaac = true;

                    {
                        lock_guard lock(ni.addr_lock);

                        for (auto le = ni.addrs.next; le != &ni.addrs; le = le->next) {
                            auto& a2 = containing_record<ip_addr>(le);

                            if (auto* addr = get_if<ipv6_address>(&a2.addr)) {
                                if (*addr == slaac) {
                                    already_exists = true;
                                    break;
                                }
                            }
                        }

                        if (!already_exists)
                            insert_tail_list(ni.addrs, a->le);
                    }

                    if (already_exists)
                        delete a;
                    else
                        print("{}: added new IPv6 address {}\n", ni, slaac);
                }
            }
        } else {
            // delete SLAAC addresses

            while (true) {
                ip_addr* dead_addr = nullptr;

                {
                    lock_guard lock(ni.addr_lock);

                    for (auto le = ni.addrs.next; le != &ni.addrs; le = le->next) {
                        auto& a = containing_record<ip_addr>(le);

                        auto* addr = get_if<ipv6_address>(&a.addr);

                        if (!addr)
                            continue;

                        if (!a.slaac)
                            continue;

                        if (!memcmp(addr->v.data(), prefix.v.data(), 8)) {
                            remove_entry_list(a.le);
                            dead_addr = &a;
                            break;
                        }
                    }
                }

                if (!dead_addr)
                    break;

                print("{}: removing IPv6 address {}\n", ni, *get_if<ipv6_address>(&dead_addr->addr));
                delete dead_addr;
            }
        }
    }

    if (beaten_to_it) {
        delete p;
        return;
    }

    add_ipv6_route(ni, prefix, prefix_length);
}

ipv4_address zero_right_bits(const ipv4_address& addr, uint8_t n) {
    ipv4_address ret = addr;
    unsigned int i = 3;

    while (true) {
        if (n >= 8) {
            ret.v[i] = 0;
            n -= 8;
            i--;

            if (n == 0)
                break;
        } else {
            ret.v[i] = (uint8_t)(ret.v[i] & ~((1 << n) - 1));
            break;
        }
    }

    return ret;
}

static ipv6_address zero_right_bits(const ipv6_address& addr, uint8_t n) {
    ipv6_address ret = addr;
    unsigned int i = 15;

    while (true) {
        if (n >= 8) {
            ret.v[i] = 0;
            n -= 8;
            i--;

            if (n == 0)
                break;
        } else {
            ret.v[i] = (uint8_t)(ret.v[i] & ~((1 << n) - 1));
            break;
        }
    }

    return ret;
}

static void regen_time_page_dns_servers() {
    // holding dns_servers_lock

    size_t i = 0;
    auto& seq = kernel_time_page.dns_servers_seq_lock;

    seq.store(seq.load(memory_order_relaxed) + 1, memory_order_relaxed);
    atomic_thread_fence(memory_order_release);

    for (auto le = dns_servers.next; le != &dns_servers; le = le->next) {
        auto& d = containing_record<dns_server>(le);

        kernel_time_page.dns_servers[i] = d.address;
        i++;

        if (i == kernel_time_page.dns_servers.size())
            break;
    }

    while (i < kernel_time_page.dns_servers.size()) {
        kernel_time_page.dns_servers[i] = parse_ipv6_address("::");
        i++;
    }

    atomic_thread_fence(memory_order_release);
    seq.store(seq.load(memory_order_relaxed) + 1, memory_order_relaxed);
}

void add_dns_server(const ipv6_address& address, uint32_t ttl) {
    {
        lock_guard lock(dns_servers_lock);

        for (auto le = dns_servers.next; le != &dns_servers; le = le->next) {
            auto& d = containing_record<dns_server>(le);

            if (d.address == address) {
                d.ttl = ttl;
                return;
            }
        }
    }

    if (ttl == 0)
        return;

    auto d2 = (dns_server*)malloc(sizeof(dns_server));
    if (!d2) {
        print("malloc failed\n");
        return;
    }

    d2->address = address;
    d2->ttl = ttl;

    bool already_done = false;

    {
        lock_guard lock(dns_servers_lock);

        for (auto le = dns_servers.next; le != &dns_servers; le = le->next) {
            auto& d = containing_record<dns_server>(le);

            if (d.address == address) {
                d.ttl = ttl;
                already_done = true;
                break;
            }
        }

        if (!already_done) {
            insert_tail_list(dns_servers, d2->le);
            regen_time_page_dns_servers();
        }
    }

    if (already_done)
        free(d2);
}

static void handle_router_advertisement(netif& ni, packet& pack) {
    if (pack.size < sizeof(ipv6_header) + sizeof(icmpv6_router_advertisement)) {
        print("handle_router_advertisement: buffer length was {}, expected at least {}\n",
              pack.size, sizeof(ipv6_header) + sizeof(icmpv6_router_advertisement));
        return;
    }

    if (!valid_ndp_packet(pack))
        return;

    auto& ip = *(ipv6_header*)pack.data;
    auto& ra = *(icmpv6_router_advertisement*)(pack.data + sizeof(ipv6_header));

#if 0
    print("RA: src = {}, cur_hop_limit = {}, snac_router = {}, nd_proxy = {}, prf = {}, home_agent = {}, other_configuration = {}, managed_address_configuration = {}, router_lifetime = {}, reachable_time = {}, retrans_timer = {}\n",
          ip.src, ra.cur_hop_limit, +ra.snac_router, +ra.nd_proxy, +ra.prf,
          +ra.home_agent, +ra.other_configuration,
          +ra.managed_address_configuration, byteswap(ra.router_lifetime),
          byteswap(ra.reachable_time), byteswap(ra.retrans_timer));
#endif

    // FIXME - reachable_time (neighbour cache state machine)
    // FIXME - retrans_timer (how long to wait between neighbour solicitations)

    optional<mac_address> src_lladdr;

    auto opts = span(pack.data + sizeof(ipv6_header) + sizeof(icmpv6_router_advertisement),
                     pack.size - sizeof(ipv6_header) - sizeof(icmpv6_router_advertisement));

    while (opts.size() >= 8) {
        auto& opt = *(ndp_option*)opts.data();

        if (opts.size() < opt.length * 8) {
            print("handle_router_advertisement: router advertisement option truncated\n");
            break;
        }

        switch (opt.type) {
            case ndp_option_type::source_lladdr: {
                auto& src = *(ndp_option_lladdr*)&opt;

#if 0
                print("source link-local address: {}\n", src.mac);
#endif

                src_lladdr = src.mac;

                break;
            }

            case ndp_option_type::prefix_info: {
                if (opt.length * 8 < sizeof(ndp_option_prefix_info)) {
                    print("handle_router_advertisement: prefix info was {} bytes, expected {}\n",
                          opt.length * 8, sizeof(ndp_option_prefix_info));
                    break;
                }

                auto& pref = *(ndp_option_prefix_info*)&opt;

#if 0
                print("prefix: onlink = {}, autonomous = {}, router_address = {}, dhcpv6_pd_preferred = {}, valid_lifetime = {}, preferred_lifetime = {}, prefix = {}/{}\n",
                      +pref.onlink, +pref.autonomous, +pref.router_address,
                      +pref.dhcpv6_pd_preferred, byteswap(pref.valid_lifetime),
                      byteswap(pref.preferred_lifetime), pref.prefix,
                      pref.prefix_length);
#endif

                add_ipv6_prefix(ni, zero_right_bits(pref.prefix, 128 - pref.prefix_length),
                                pref.prefix_length, byteswap(pref.valid_lifetime),
                                byteswap(pref.preferred_lifetime), pref.onlink,
                                pref.autonomous);

                break;
            }

            // FIXME - MTU

            case ndp_option_type::rdnss: {
                if (opt.length * 8 < offsetof(ndp_option_rdnss, servers)) {
                    print("handle_router_advertisement: RDNSS info was {} bytes, expected at least {}\n",
                          opt.length * 8, offsetof(ndp_option_rdnss, servers));
                    break;
                }

                auto& rdnss = *(ndp_option_rdnss*)&opt;

                size_t num_servers = ((opt.length * 8) - offsetof(ndp_option_rdnss, servers)) / sizeof(ipv6_address);

                for (size_t i = 0; i < num_servers; i++) {
                    add_dns_server(rdnss.servers[i], byteswap(rdnss.lifetime));
                }

                break;
            }

            default:
                break;
        }

        opts = span(opts.data() + (opt.length * 8), opts.size() - (opt.length * 8));
    }

    if (src_lladdr.has_value())
        add_neighbour_cache_entry(ni, *src_lladdr, ip.src, true);

    if (ra.router_lifetime == 0) {
        ipv6_router* r = nullptr;
        bool no_routers_left = false;

        {
            lock_guard lock(ni.routers_lock);

            for (auto le = ni.routers.next; le != &ni.routers; le = le->next) {
                auto& r2 = containing_record<ipv6_router>(le);

                if (r2.addr == ip.src) {
                    remove_entry_list(r2.le);
                    r = &r2;
                    break;
                }
            }

            no_routers_left = is_list_empty(ni.routers);
        }

        if (r)
            delete r;

        if (no_routers_left)
            remove_ipv6_route(ni, parse_ipv6_address("::"), 0); // remove default route
    } else {
        bool router_found = false;

        {
            lock_guard lock(ni.routers_lock);

            for (auto le = ni.routers.next; le != &ni.routers; le = le->next) {
                auto& r = containing_record<ipv6_router>(le);

                if (r.addr == ip.src) {
                    r.lifetime = ra.router_lifetime;
                    router_found = true;
                    break;
                }
            }
        }

        if (!router_found && src_lladdr.has_value()) {
            auto r = new ipv6_router;

            if (!r)
                print("new failed");
            else {
                r->addr = ip.src;
                r->mac = *src_lladdr;
                r->lifetime = ra.router_lifetime;

                {
                    lock_guard lock(ni.routers_lock);

                    for (auto le = ni.routers.next; le != &ni.routers; le = le->next) {
                        auto& r = containing_record<ipv6_router>(le);

                        if (r.addr == ip.src) {
                            r.lifetime = ra.router_lifetime;
                            router_found = true;
                            break;
                        }
                    }

                    if (!router_found)
                        insert_tail_list(ni.routers, r->le);
                }

                if (!router_found)
                    add_ipv6_route(ni, parse_ipv6_address("::"), 0); // add default route
                else
                    delete r;
            }
        }
    }

    ni.rs_needed = false;
}

static void handle_icmpv6_error(packet& pack) {
    if (pack.size < sizeof(ipv6_header) + sizeof(icmpv6_error)) {
        print("handle_icmpv6_error: buffer length was {}, expected at least {}\n",
              pack.size, sizeof(ipv6_header) + sizeof(icmpv6_error));
        return;
    }

    const auto& orig_ip = *(ipv6_header*)pack.data;
    const auto& icmp = *(icmpv6_error*)(pack.data + sizeof(ipv6_header));

    if (calc_ipv6_checksum(orig_ip, span((uint8_t*)&icmp, pack.size - sizeof(ipv6_header)), IPPROTO_ICMPV6) != 0)
        return;

    if (icmp.header.next_header == IPPROTO_UDP)
        handle_icmpv6_error_udp(pack);
}

static void handle_icmpv6_packet(netif& ni, packet& pack,
                                 const mac_address& src_mac,
                                 bool skip_checksum) {
    if (pack.size < sizeof(ipv6_header) + sizeof(icmpv6_header)) {
        print("handle_icmpv6_packet: buffer length was {}, expected at least {}\n",
              pack.size, sizeof(ipv6_header) + sizeof(icmpv6_header));
        return;
    }

    auto& header = *(icmpv6_header*)(pack.data + sizeof(ipv6_header));

#if 0
    print("icmpv6: type = {}, code = {:02x}, checksum = {:04x}\n",
          header.type, header.code, byteswap(header.checksum));
#endif

    switch (header.type) {
        case icmpv6_type::dest_unreachable:
        case icmpv6_type::packet_too_big:
        case icmpv6_type::time_exceeded:
        case icmpv6_type::param_problem:
            handle_icmpv6_error(pack);
            break;

        case icmpv6_type::echo_request:
            handle_v6_echo_request(ni, pack, src_mac, skip_checksum);
            break;

        case icmpv6_type::router_advertisement:
            handle_router_advertisement(ni, pack);
            break;

        case icmpv6_type::neighbour_solicitation:
            handle_neighbour_solicitation(ni, pack, src_mac);
            break;

        case icmpv6_type::neighbour_advertisement:
            handle_neighbour_advertisement(ni, pack);
            break;

        default:
            break;
    }
}

static void send_to_raw_ipv6_sockets(uint32_t netif_num, packet& pack) {
    auto& ip = *(ipv6_header*)pack.data;
    size_t num_sockets;

    {
        lock_guard lock(ipv6_raw_sockets_lock);
        num_sockets = num_ipv6_raw_sockets;
    }

    if (num_sockets == 0)
        return;

    auto arr = (raw_socket<ipv6>**)malloc(sizeof(raw_socket<ipv6>*) * num_sockets);
    if (!arr) {
        print("malloc failed\n");
        return;
    }

    size_t num = 0;

    {
        lock_guard lock(ipv6_raw_sockets_lock);

        for (auto le = ipv6_raw_sockets.next; le != &ipv6_raw_sockets; le = le->next) {
            auto& s = containing_record<raw_socket<ipv6>>(le);

            arr[num] = &s;
            s.acquire();

            num++;
            if (num == num_sockets)
                break;
        }
    }

    if (num == 0) {
        free(arr);
        return;
    }

    for (size_t i = 0; i < num; i++) {
        auto& s = *arr[i];

        if (s.protocol != ip.next_header) {
            s.release();
            continue;
        }

        auto p = new socket_packet(pack);
        if (!p) {
            print("new failed\n");
            s.release();
            continue;
        }

        p->scope_id = netif_num;

        bool done = false;

        {
            sync_lock sl;
            lock_guard lock(sl);
            lock_guard lock2(s.buffer_lock);

            if (s.buffer_size + pack.size <= MAX_BUFFER_SIZE) {
                insert_tail_list(s.buffer, p->le);
                s.buffer_size += pack.size;
                done = true;
            }

            if (!s.signalled) {
                wake_up_waiters(s.waiters, WAIT_TYPE_READING);
                s.signalled = true;
            }
        }

        if (!done) {
            delete p;
            s.release();
            continue;
        }

        s.release();
    }

    free(arr);
}

static void send_to_ipv6_sockets(netif& ni, packet& pack,
                                 const mac_address& src_mac, bool skip_checksum) {
    auto& ip = *(ipv6_header*)pack.data;

    if (ip.next_header == IPPROTO_UDP)
        received_ipv6_udp_packet(ni, pack, src_mac, skip_checksum);
    else if (ip.next_header == IPPROTO_TCP)
        received_ipv6_tcp_packet(ni, pack, src_mac, skip_checksum);

    send_to_raw_ipv6_sockets(ni.num, pack);
}

static void send_to_raw_ipv4_sockets(uint32_t netif_num, packet& pack) {
    auto& ip = *(ipv4_header*)pack.data;
    size_t num_sockets;

    {
        lock_guard lock(ipv4_raw_sockets_lock);
        num_sockets = num_ipv4_raw_sockets;
    }

    if (num_sockets == 0)
        return;

    auto arr = (raw_socket<ipv4>**)malloc(sizeof(raw_socket<ipv4>*) * num_sockets);
    if (!arr) {
        print("malloc failed\n");
        return;
    }

    size_t num = 0;

    {
        lock_guard lock(ipv4_raw_sockets_lock);

        for (auto le = ipv4_raw_sockets.next; le != &ipv4_raw_sockets; le = le->next) {
            auto& s = containing_record<raw_socket<ipv4>>(le);

            arr[num] = &s;
            s.acquire();

            num++;
            if (num == num_sockets)
                break;
        }
    }

    if (num == 0) {
        free(arr);
        return;
    }

    for (size_t i = 0; i < num; i++) {
        auto& s = *arr[i];

        if (s.protocol != ip.protocol) {
            s.release();
            continue;
        }

        auto p = new socket_packet(pack);
        if (!p) {
            print("new failed\n");
            s.release();
            continue;
        }

        p->scope_id = netif_num;

        bool done = false;

        {
            sync_lock sl;
            lock_guard lock(sl);
            lock_guard lock2(s.buffer_lock);

            if (s.buffer_size + pack.size <= MAX_BUFFER_SIZE) {
                insert_tail_list(s.buffer, p->le);
                s.buffer_size += pack.size;
                done = true;
            }

            if (!s.signalled) {
                wake_up_waiters(s.waiters, WAIT_TYPE_READING);
                s.signalled = true;
            }
        }

        if (!done) {
            delete p;
            s.release();
            continue;
        }

        s.release();
    }

    free(arr);
}

static void send_to_ipv4_sockets(netif& ni, packet& pack,
                                 const mac_address& src_mac, bool skip_checksum) {
    auto& ip = *(ipv4_header*)pack.data;

    if (ip.protocol == IPPROTO_UDP)
        received_ipv4_udp_packet(ni, pack, src_mac, skip_checksum);
    else if (ip.protocol == IPPROTO_TCP)
        received_ipv4_tcp_packet(ni, pack, src_mac, skip_checksum);

    send_to_raw_ipv4_sockets(ni.num, pack);
}

static bool should_accept_multicast(const ipv4_address& dst) {
    static const auto all_nodes = parse_ipv4_address("224.0.0.1");

    return dst == all_nodes;
}

static bool should_accept_multicast(netif& ni, const ipv6_address& dst) {
    static const auto all_nodes = parse_ipv6_address("ff02::1");
    static const auto solicited_multicast = parse_ipv6_address("ff02::1:ff00:0");

    if (dst == all_nodes)
        return true;

    if (!memcmp(dst.v.data(), solicited_multicast.v.data(), 13)) {
        lock_guard lock(ni.addr_lock);

        for (auto le = ni.addrs.next; le != &ni.addrs; le = le->next) {
            auto& a = containing_record<ip_addr>(le);

            auto* ipaddr = get_if<ipv6_address>(&a.addr);

            if (!ipaddr)
                continue;

            if (ipaddr->v[13] == dst.v[13] && ipaddr->v[14] == dst.v[14] &&
                ipaddr->v[15] == dst.v[15]) {
                return true;
            }
        }
    }

    return false;
}

static void handle_ipv6_packet(netif& ni, packet& pack, const mac_address& src_mac,
                               bool skip_checksum) {
    if (pack.size < sizeof(ipv6_header)) {
        print("handle_ipv6_packet: buffer length was {}, expected at least {}\n",
              pack.size, sizeof(ipv6_header));
        return;
    }

    const auto& ip = *(ipv6_header*)pack.data;
    const auto& dst = ip.dst;

#if 0
    print("ipv6: version = {}, traffic_class1 = {}, traffic_class2 = {}, flow_label = {}, payload_length = {}, next_header = {}, hop_limit = {}, src = {}, dst = {}\n",
          +ip.version, +ip.traffic_class1, +ip.traffic_class2, +ip.flow_label,
          byteswap(ip.payload_length), ip.next_header, ip.hop_limit,
          ip.src, ip.dst);
#endif

    if (is_multicast_address(ip.dst)) {
        if (!should_accept_multicast(ni, ip.dst))
            return;
    } else {
        bool found = false;

        // check if it's for us

        {
            lock_guard lock(ni.addr_lock);

            for (auto le = ni.addrs.next; le != &ni.addrs; le = le->next) {
                auto& a = containing_record<ip_addr>(le);

                if (auto* addr = get_if<ipv6_address>(&a.addr)) {
                    if (*addr == dst) {
                        found = true;
                        break;
                    }
                }
            }
        }

        if (!found)
            return;
    }

    switch ((ip_proto)ip.next_header) {
        case ip_proto::icmpv6:
            handle_icmpv6_packet(ni, pack, src_mac, skip_checksum);
            break;

        default:
            break;
    }

    send_to_ipv6_sockets(ni, pack, src_mac, skip_checksum);

    {
        lock_guard lock(ni.neighbour_lock);

        for (auto le = ni.neighbour_cache.next; le != &ni.neighbour_cache; le = le->next) {
            auto& c = containing_record<neighbour_cache_entry<ipv6>>(le);

            if (c.addr == ip.src) {
                if (c.status == neighbour_cache_status::reachable)
                    c.last_update_time = kernel_time_page.system_time;

                break;
            }
        }
    }
}

static void handle_v4_echo_request(netif& ni, packet& pack, const mac_address& src_mac,
                                   const ipv4_address& our_ip, bool skip_checksum) {
    const auto& orig_ip = *(ipv4_header*)pack.data;
    size_t ip_header_size = orig_ip.ihl * sizeof(uint32_t);

    if (pack.size < ip_header_size + sizeof(icmpv4_echo)) {
        print("handle_v4_echo_request: buffer length was {}, expected at least {}\n",
              pack.size, ip_header_size + sizeof(icmpv4_echo));
        return;
    }

    if (!skip_checksum) {
        if (calc_ipv4_checksum(span(pack.data + ip_header_size, pack.size - ip_header_size)) != 0)
            return;
    }

    auto pack2 = new packet;
    if (!pack2) {
        print("new failed\n");
        return;
    }

    // take copy

    pack2->size = pack.size;
    memcpy(pack2->data, pack.data, pack.size);

    // switch IP addresses

    auto& ip = *(ipv4_header*)pack2->data;
    ip.dst = ip.src;
    ip.src = our_ip;

    ip.ttl = DEFAULT_HOP_LIMIT;

    ip.header_checksum = 0;
    ip.header_checksum = calc_ipv4_checksum(span(pack2->data, ip_header_size));

    // change type to reply

    auto& header = *(icmpv4_header*)(pack2->data + ip_header_size);
    header.type = icmpv4_type::echo_reply;

    header.checksum = 0;
    header.checksum = calc_ipv4_checksum(span(pack2->data + ip_header_size,
                                              pack2->size - ip_header_size));

    if (ip.src == ip.dst || ni.loopback) // pinging ourselves
        handle_ipv4_packet(ni, *pack2, ni.mac, true);
    else {
        auto status = ni.send_packet(src_mac, ethertype::ipv4, *pack2);
        if (!success(status))
            print("send_packet returned {}\n", status);
    }

    pack2->release();
}

static void handle_icmpv4_error(packet& pack, bool skip_checksum) {
    const auto& orig_ip = *(ipv4_header*)pack.data;
    size_t ip_header_size = orig_ip.ihl * sizeof(uint32_t);

    if (pack.size < sizeof(ipv4_header) + sizeof(icmpv4_error)) {
        print("handle_icmpv4_error: buffer length was {}, expected at least {}\n",
              pack.size, sizeof(ipv4_header) + sizeof(icmpv4_error));
        return;
    }

    if (!skip_checksum) {
        if (calc_ipv4_checksum(span(pack.data + ip_header_size, pack.size - ip_header_size)) != 0)
            return;
    }

    const auto& icmp = *(icmpv4_error*)(pack.data + ip_header_size);

    if (icmp.header.protocol == IPPROTO_UDP)
        handle_icmpv4_error_udp(pack);
}

static void handle_icmpv4_packet(netif& ni, packet& pack, const mac_address& src_mac,
                                 const ipv4_address& our_ip, bool skip_checksum) {
    const auto& ip = *(ipv4_header*)pack.data;
    size_t ip_header_size = ip.ihl * sizeof(uint32_t);

    if (pack.size < ip_header_size + sizeof(icmpv4_header)) {
        print("handle_icmpv4_packet: buffer length was {}, expected at least {}\n",
              pack.size, ip_header_size + sizeof(icmpv4_header));
        return;
    }

    auto& header = *(icmpv4_header*)(pack.data + ip_header_size);

#if 0
    print("icmpv4: type = {}, code = {:02x}, checksum = {:04x}\n",
          (uint8_t)header.type, header.code, byteswap(header.checksum));
#endif

    switch (header.type) {
        case icmpv4_type::dest_unreachable:
        case icmpv4_type::time_exceeded:
        case icmpv4_type::param_problem:
            handle_icmpv4_error(pack, skip_checksum);
            break;

        case icmpv4_type::echo_request:
            handle_v4_echo_request(ni, pack, src_mac, our_ip, skip_checksum);
            break;

        default:
            break;
    }
}

static uint8_t calc_matching_prefix(const ipv4_address& a1, const ipv4_address& a2) {
    uint32_t v1 = (uint32_t)(a1.v[0] << 24) | (uint32_t)(a1.v[1] << 16) | (uint32_t)(a1.v[2] << 8) | (uint32_t)a1.v[3];
    uint32_t v2 = (uint32_t)(a2.v[0] << 24) | (uint32_t)(a2.v[1] << 16) | (uint32_t)(a2.v[2] << 8) | (uint32_t)a2.v[3];

    return (uint8_t)countl_one(~(v1 ^ v2));
}

ipv4_address calculate_broadcast_address(const ipv4_address& addr, uint8_t prefix) {
    uint32_t v = (uint32_t)(addr.v[0] << 24) | (uint32_t)(addr.v[1] << 16) | (uint32_t)(addr.v[2] << 8) | (uint32_t)addr.v[3];
    uint32_t mask;
    ipv4_address ret;

    if (prefix == 0)
        mask = 0;
    else
        mask = 0xffffffff << (32 - prefix);

    v = (v & mask) | ~mask;

    ret.v[0] = (uint8_t)(v >> 24);
    ret.v[1] = (v >> 16) & 0xff;
    ret.v[2] = (v >> 8) & 0xff;
    ret.v[3] = v & 0xff;

    return ret;
}

static void handle_ipv4_packet(netif& ni, packet& pack, const mac_address& src_mac,
                               bool skip_checksum) {
    if (pack.size < sizeof(ipv4_header)) {
        print("handle_ipv4_packet: buffer length was {}, expected at least {}\n",
              pack.size, sizeof(ipv4_header));
        return;
    }

    const auto& ip = *(ipv4_header*)pack.data;
    size_t ip_header_size = ip.ihl * sizeof(uint32_t);

    if (ip_header_size < sizeof(ipv4_header)) {
        print("handle_ipv4_packet: ip_header_size was {}, expected at least {}\n",
              ip_header_size, sizeof(ipv4_header));
        return;
    }

    if (pack.size < ip_header_size) {
        print("handle_ipv4_packet: buffer length was {}, expected at least {}\n",
              pack.size, ip_header_size);
        return;
    }

    auto size = byteswap((uint16_t)ip.total_length);

    if (size < sizeof(ipv4_header) || size > pack.size)
        return;

#if 0
    print("ipv4: version = {}, ihl = {}, dscp = {}, ecn = {}, total_length = {}, identification = {}, fragment_offset1 = {}, MF = {}, DF = {}, fragment_offset2 = {}, ttl = {}, protocol = {}, header_checksum = {:x}, src = {}, dst = {}\n",
          ip.version, ip.ihl, ip.dscp, ip.ecn, byteswap((uint16_t)ip.total_length),
          ip.identification, ip.fragment_offset1, ip.MF, ip.DF, ip.fragment_offset2,
          ip.ttl, ip.protocol, byteswap((uint16_t)ip.header_checksum), ip.src, ip.dst);
#endif

    if (ip.src == parse_ipv4_address("255.255.255.255"))
        return;

    if (is_multicast_address(ip.dst) && !should_accept_multicast(ip.dst))
        return;

    optional<ipv4_address> our_ip;
    bool no_ipv4_addresses = true;

    if (ni.loopback)
        our_ip = ip.dst;
    else {
        uint8_t matching_prefix;

        lock_guard lock(ni.addr_lock);

        for (auto le = ni.addrs.next; le != &ni.addrs; le = le->next) {
            auto& a = containing_record<ip_addr>(le);

            auto* addr = get_if<ipv4_address>(&a.addr);

            if (!addr)
                continue;

            no_ipv4_addresses = false;

            if (ip.dst == parse_ipv4_address("255.255.255.255") || is_multicast_address(ip.dst)) {
                if (!our_ip.has_value()) {
                    our_ip = *addr;
                    matching_prefix = calc_matching_prefix(*addr, ip.src);
                } else {
                    uint8_t pref = calc_matching_prefix(*addr, ip.src);

                    if (pref > matching_prefix) {
                        our_ip = *addr;
                        matching_prefix = pref;
                    }
                }
            } else {
                if (ip.dst == *addr) {
                    our_ip = *addr;
                    break;
                }

                if (ip.dst == calculate_broadcast_address(*addr, a.prefix)) {
                    our_ip = *addr;
                    break;
                }
            }
        }
    }

    if (!our_ip.has_value()) {
        if (no_ipv4_addresses && (ip.dst == parse_ipv4_address("255.255.255.255") || is_multicast_address(ip.dst)))
            our_ip = parse_ipv4_address("0.0.0.0");
        else if (no_ipv4_addresses && ip.protocol == IPPROTO_UDP) // let unicast DHCP packets through before configuration
            our_ip = parse_ipv4_address("0.0.0.0");
        else
            return;
    }

    if (!skip_checksum) {
        if (calc_ipv4_checksum(span(pack.data, ip_header_size)) != 0)
            return;
    }

    switch ((ip_proto)ip.protocol) {
        case ip_proto::icmpv4:
            handle_icmpv4_packet(ni, pack, src_mac, *our_ip, skip_checksum);
            break;

        default:
            break;
    }

    send_to_ipv4_sockets(ni, pack, src_mac, skip_checksum);
}

bool drop_packet(const mac_address& dst, const mac_address& us) {
    if (dst == us)
        return false;

    // IPv6 multicast
    if (dst.v[0] == 0x33 && dst.v[1] == 0x33)
        return false;

    // IPv4 multicast
    if (dst.v[0] == 0x01 && dst.v[1] == 0x00 && dst.v[2] == 0x5e)
        return false;

    // broadcast
    if (dst.v[0] == 0xff && dst.v[1] == 0xff && dst.v[2] == 0xff &&
        dst.v[3] == 0xff && dst.v[4] == 0xff && dst.v[5] == 0xff) {
        return false;
    }

    return true;
}

static void reap_neighbour_cache() {
    int64_t now = kernel_time_page.system_time;

    // FIXME - implement proper RFC4861 state machine? INCOMPLETE, REACHABLE, STALE, DELAY, PROBE

    static const unsigned int REAP_TIME = 300; // 5 minutes

    {
        lock_guard lock(netif_lock);

        for (auto le = netif_list.next; le != &netif_list; le = le->next) {
            list_entry reap_list = { &reap_list, &reap_list };

            auto& ni = containing_record<netif>(le);

            {
                lock_guard lock2(ni.neighbour_lock);

                for (auto le2 = ni.neighbour_cache.next; le2 != &ni.neighbour_cache; le2 = le2->next) {
                    auto& c = containing_record<neighbour_cache_entry<ipv6>>(le2);

                    if (now >= c.last_update_time + (REAP_TIME * SYSTEM_TIME_RESOLUTION)) {
                        auto le_prev = le2->prev;

                        remove_entry_list(c.le);
                        insert_tail_list(reap_list, c.le);

                        le2 = le_prev;
                    }
                }
            }

            while (!is_list_empty(reap_list)) {
                auto& c = containing_record<neighbour_cache_entry<ipv6>>(reap_list.next);

                set_event(static_cast<event&>(c.event.get()));

                remove_entry_list(c.le);
                c.release();
            }
        }
    }
}

static void reap_arp_cache() {
    int64_t now = kernel_time_page.system_time;

    static const unsigned int REAP_TIME = 300; // 5 minutes

    {
        lock_guard lock(netif_lock);

        for (auto le = netif_list.next; le != &netif_list; le = le->next) {
            list_entry reap_list = { &reap_list, &reap_list };

            auto& ni = containing_record<netif>(le);

            {
                lock_guard lock2(ni.arp_lock);

                for (auto le2 = ni.arp_cache.next; le2 != &ni.arp_cache; le2 = le2->next) {
                    auto& c = containing_record<neighbour_cache_entry<ipv4>>(le2);

                    if (now >= c.last_update_time + (REAP_TIME * SYSTEM_TIME_RESOLUTION)) {
                        auto le_prev = le2->prev;

                        remove_entry_list(c.le);
                        insert_tail_list(reap_list, c.le);

                        le2 = le_prev;
                    }
                }
            }

            while (!is_list_empty(reap_list)) {
                auto& c = containing_record<neighbour_cache_entry<ipv4>>(reap_list.next);

                set_event(static_cast<event&>(c.event.get()));

                remove_entry_list(c.le);
                c.release();
            }
        }
    }
}

static void send_router_solicitation(netif& ni) {
    static const unsigned int MAX_TRIES = 3;
    static const mac_address dst_mac = { 0x33, 0x33, 0x00, 0x00, 0x00, 0x02 };

    if (ni.rs_tries == MAX_TRIES) {
        print("no response to router solicitations on {} after {} tries, giving up\n",
              ni, ni.rs_tries);
        ni.rs_needed = false;
        return;
    }

    auto pack = new packet;
    if (!pack) {
        print("new failed\n");
        return;
    }

    struct rs_pkt {
        ipv6_header ip;
        icmpv6_router_solicitation rs;
        ndp_option_lladdr opt;
    } __attribute__((packed));

    pack->size = sizeof(rs_pkt);
    auto& pkt = *(rs_pkt*)pack->data;

    memset(&pkt, 0, sizeof(pkt));

    pkt.ip.version = 6;
    pkt.ip.payload_length = byteswap((uint16_t)(sizeof(icmpv6_router_solicitation) + sizeof(ndp_option_lladdr)));
    pkt.ip.next_header = (uint8_t)ip_proto::icmpv6;
    pkt.ip.hop_limit = 255;
    pkt.ip.src = ni.ll_addr;
    pkt.ip.dst = parse_ipv6_address("ff02::2");

    pkt.rs.type = icmpv6_type::router_solicitation;

    pkt.opt.type = ndp_option_type::source_lladdr;
    pkt.opt.length = 1; // 8 bytes
    pkt.opt.mac = ni.mac;

    pkt.rs.checksum = calc_ipv6_checksum(pkt.ip, span((uint8_t*)&pkt.rs, sizeof(pkt.rs) + sizeof(pkt.opt)), IPPROTO_ICMPV6);

    auto status = ni.send_packet(dst_mac, ethertype::ipv6, *pack);
    if (!success(status))
        print("send_packet returned {}\n", status);

    pack->release();

    ni.rs_tries++;
    ni.secs_since_last_rs = 0;
}

static void send_router_solicitations() {
    static const unsigned int RS_DELAY = 4; // seconds

    while (true) {
        netif* ni = nullptr;

        {
            lock_guard lock(netif_lock);

            for (auto le = netif_list.next; le != &netif_list; le = le->next) {
                auto& ni2 = containing_record<netif>(le);

                if (!ni2.rs_needed || ni2.rs_visited)
                    continue;

                ni2.rs_visited = true;
                ni2.secs_since_last_rs++;

                if (ni2.rs_tries == 0 || ni2.secs_since_last_rs >= RS_DELAY) {
                    ni = &ni2;
                    ni2.acquire();
                    break;
                }
            }
        }

        if (!ni)
            break;

        send_router_solicitation(*ni);
    }

    {
        lock_guard lock(netif_lock);

        for (auto le = netif_list.next; le != &netif_list; le = le->next) {
            auto& ni = containing_record<netif>(le);

            ni.rs_visited = false;
        }
    }
}

static void send_dhcp_messages() {
    while (true) {
        netif* ni = nullptr;
        dhcp_status status;
        bool need_renewal = false, need_rebind = false;

        {
            lock_guard lock(netif_lock);

            for (auto le = netif_list.next; le != &netif_list; le = le->next) {
                auto& ni2 = containing_record<netif>(le);

                if (ni2.dhcp_visited || ni2.dhcp_status == dhcp_status::not_needed)
                    continue;

                ni2.dhcp_visited = true;

                {
                    lock_guard lock(ni2.dhcp_lock);

                    if (ni2.dhcp_status == dhcp_status::need_discover) {
                        status = ni2.dhcp_status;
                        ni = &ni2;
                        ni2.acquire();
                        break;
                    } else if (ni2.dhcp_status == dhcp_status::done) {
                        ni2.dhcp_lease_left--;

                        if (ni2.dhcp_renew_time > 0) {
                            ni2.dhcp_renew_time--;

                            if (ni2.dhcp_renew_time == 0) {
                                need_renewal = true;
                                ni2.dhcp_renew_time = max(ni2.dhcp_lease_left / 2, (uint32_t)60);
                            }
                        }

                        if (ni2.dhcp_rebind_time > 0) {
                            ni2.dhcp_rebind_time--;

                            if (ni2.dhcp_rebind_time == 0) {
                                need_rebind = true;
                                ni2.dhcp_renew_time = 0;
                                ni2.dhcp_rebind_time = max(ni2.dhcp_lease_left / 2, (uint32_t)60);
                            }
                        }

                        if (ni2.dhcp_lease_left == 0 || need_renewal || need_rebind) {
                            status = ni2.dhcp_status;
                            ni = &ni2;
                            ni2.acquire();
                            break;
                        }
                    }
                }
            }
        }

        if (!ni)
            break;

        if (status == dhcp_status::need_discover)
            send_dhcp_discover(*ni);
        else if (need_rebind)
            do_dhcp_renewal(*ni, true);
        else if (need_renewal)
            do_dhcp_renewal(*ni, false);
        else if (status == dhcp_status::done)
            handle_dhcp_expiry(*ni);

        ni->release();
    }

    {
        lock_guard lock(netif_lock);

        for (auto le = netif_list.next; le != &netif_list; le = le->next) {
            auto& ni = containing_record<netif>(le);

            ni.dhcp_visited = false;
        }
    }
}

static void netif_routers_tick(netif& ni) {
    while (true) {
        ipv6_router* dead_router = nullptr;
        bool no_more_routers;

        {
            lock_guard lock(ni.routers_lock);

            for (auto le = ni.routers.next; le != &ni.routers; le = le->next) {
                auto& r = containing_record<ipv6_router>(le);

                if (r.tick_visited)
                    continue;

                r.lifetime--;
                r.tick_visited = true;

                if (r.lifetime == 0) {
                    remove_entry_list(r.le);
                    dead_router = &r;
                    no_more_routers = is_list_empty(ni.routers);
                    break;
                }
            }
        }

        if (!dead_router)
            break;

        delete dead_router;

        if (no_more_routers)
            remove_ipv6_route(ni, parse_ipv6_address("::"), 64);
    }

    {
        lock_guard lock(ni.routers_lock);

        for (auto le = ni.routers.next; le != &ni.routers; le = le->next) {
            auto& r = containing_record<ipv6_router>(le);

            r.tick_visited = false;
        }
    }
}

static void netif_prefixes_tick(netif& ni) {
    netif_routers_tick(ni);

    // FIXME - also preferred_lifetime

    while (true) {
        ipv6_prefix* dead_prefix = nullptr;

        {
            lock_guard lock(ni.prefix_lock);

            for (auto le = ni.prefixes.next; le != &ni.prefixes; le = le->next) {
                auto& p = containing_record<ipv6_prefix>(le);

                if (p.tick_visited)
                    continue;

                p.valid_lifetime--;
                p.tick_visited = true;

                if (p.valid_lifetime == 0) {
                    remove_entry_list(p.le);
                    dead_prefix = &p;
                    break;
                }
            }
        }

        if (!dead_prefix)
            break;

        remove_ipv6_route(ni, dead_prefix->prefix, dead_prefix->prefix_length);

        // FIXME - should we be sending neighbour advertisements to say these IPs are going away?

        if (dead_prefix->prefix_length == 64) {
            // delete SLAAC addresses

            while (true) {
                ip_addr* dead_addr = nullptr;

                {
                    lock_guard lock(ni.addr_lock);

                    for (auto le = ni.addrs.next; le != &ni.addrs; le = le->next) {
                        auto& a = containing_record<ip_addr>(le);

                        auto* addr = get_if<ipv6_address>(&a.addr);
                        if (!addr)
                            continue;

                        if (!a.slaac)
                            continue;

                        if (!memcmp(addr->v.data(), dead_prefix->prefix.v.data(), 8)) {
                            remove_entry_list(a.le);
                            dead_addr = &a;
                            break;
                        }
                    }
                }

                if (!dead_addr)
                    break;

                print("{}: removing IPv6 address {}\n", ni, *get_if<ipv6_address>(&dead_addr->addr));
                delete dead_addr;
            }
        }

        delete dead_prefix;
    }

    {
        lock_guard lock(ni.prefix_lock);

        for (auto le = ni.prefixes.next; le != &ni.prefixes; le = le->next) {
            auto& p = containing_record<ipv6_prefix>(le);

            p.tick_visited = false;
        }
    }
}

static void router_lifetime_tick() {
    lock_guard lock(netif_lock);

    for (auto le = netif_list.next; le != &netif_list; le = le->next) {
        auto& ni = containing_record<netif>(le);

        netif_routers_tick(ni);
        netif_prefixes_tick(ni);
    }
}

static void reap_dns_servers() {
    list_entry free_list = { &free_list, &free_list };

    {
        bool changed = false;
        lock_guard lock(dns_servers_lock);

        for (auto le = dns_servers.next; le != &dns_servers; le = le->next) {
            auto& d = containing_record<dns_server>(le);

            if (d.ttl == 0) {
                le = le->prev;
                remove_entry_list(d.le);
                insert_tail_list(free_list, d.le);
                changed = true;
            } else
                d.ttl--;
        }

        if (changed)
            regen_time_page_dns_servers();
    }

    while (!is_list_empty(free_list)) {
        auto& d = containing_record<dns_server>(free_list.next);

        remove_entry_list(d.le);
        free(&d);
    }
}

static void network_housekeeping() {
    // once a second

    send_router_solicitations();
    send_dhcp_messages();
    router_lifetime_tick();
    reap_neighbour_cache();
    reap_arp_cache();
    reap_dns_servers();
}

static void network_thread() {
    auto tim_ref = create_timer(true);
    if (!tim_ref) {
        print("create_timer failed, exiting network thread\n");
        return;
    }

    auto& tim = static_cast<timer&>(tim_ref.get());

    int64_t due_time_ticks = SYSTEM_TIME_RESOLUTION; // 1 second
    auto due_time_ms = (int32_t)((due_time_ticks * 1000) / SYSTEM_TIME_RESOLUTION);

    auto status = tim.set(-due_time_ticks, nullptr, nullptr, false,
                          due_time_ms, nullptr);
    if (!success(status)) {
        print("timer::set returned {}, exiting network thread\n", status);
        return;
    }

    while (true) {
        status = wait_for_object(tim, false);
        if (!success(status)) {
            print("wait_for_object returned {}, exiting network thread\n", status);
            break;
        }

        network_housekeeping();
    }
}

static void update_hostname(string_view name) {
    lock_guard lock(hostname_lock);

    auto& seq = kernel_time_page.hostname_seq_lock;

    seq.store(seq.load(memory_order_relaxed) + 1, memory_order_relaxed);
    atomic_thread_fence(memory_order_release);

    size_t len = min(name.size(), sizeof(kernel_time_page.hostname) - 1);

    memcpy(kernel_time_page.hostname, name.data(), len);

    memset(kernel_time_page.hostname + len, 0,
           sizeof(kernel_time_page.hostname) - len);

    atomic_thread_fence(memory_order_release);
    seq.store(seq.load(memory_order_relaxed) + 1, memory_order_relaxed);
}

bool init_net() {
    static const string_view LOOPBACK_NAME = "lo";

    auto pptr = new netctl;
    if (!pptr) {
        print("new failed\n");
        return false;
    }

    netctl_obj = object_ref{*pptr};

    auto status = name_object(netctl_obj.get(), "netctl");
    if (!success(status)) {
        print("name_object returned {}\n", status);
        return false;
    }

    auto lo = new loopback_netif;
    if (!lo) {
        print("new failed\n");
        return false;
    }

    auto lo_addr = new ip_addr(parse_ipv6_address("::1"));
    if (!lo_addr) {
        print("new failed\n");
        delete lo;
        return false;
    }

    auto lo_addr2 = new ip_addr(parse_ipv4_address("127.0.0.1"));
    if (!lo_addr2) {
        print("new failed\n");
        delete lo_addr;
        delete lo;
        return false;
    }

    lo->mac = NULL_MAC_ADDRESS;
    lo->loopback = true;
    lo->num = next_netif_num.fetch_add(1);

    memcpy(lo->name, LOOPBACK_NAME.data(), LOOPBACK_NAME.size());
    memset(lo->name + LOOPBACK_NAME.size(), 0,
           sizeof(lo->name) - LOOPBACK_NAME.size());

    memset(lo->ll_addr.v.data(), 0, lo->ll_addr.v.size());
    lo->rs_needed = false;
    lo->mtu = MTU;

    {
        lock_guard lg(lo->addr_lock);
        insert_tail_list(lo->addrs, lo_addr->le);
        insert_tail_list(lo->addrs, lo_addr2->le);
    }

    {
        lock_guard lg(netif_lock);
        insert_tail_list(netif_list, lo->le);
    }

    add_ipv4_route(*lo, parse_ipv4_address("127.0.0.0"), 8, parse_ipv4_address("0.0.0.0"));

    lo->dhcp_status = dhcp_status::not_needed;

    lo_netif = lo;

    update_hostname("localhost");

    add_kernel_thread(network_thread);

    return true;
}

static nt_status netctl_socket(span<const uint8_t> buf) {
    if (buf.size() < sizeof(MOXNOX_NETCTL_SOCKET))
        return nt_status::BUFFER_TOO_SMALL;

    auto& params = *(MOXNOX_NETCTL_SOCKET*)buf.data();

    if (!params.h)
        return nt_status::INVALID_PARAMETER;

    auto t = get_current_thread();
    if (!t->proc)
        return nt_status::INTERNAL_ERROR;

    switch (params.af) {
        case AF_INET: {
            object_ref ref;

            switch (params.type) {
                case SOCK_RAW: {
                    if (params.protocol == 0)
                        return nt_status::INVALID_PARAMETER;

                    auto s = new raw_socket<ipv4>(params.protocol);
                    if (!s) {
                        print("new failed\n");
                        return nt_status::INSUFFICIENT_RESOURCES;
                    }

                    ref = object_ref{*s};

                    break;
                }

                case SOCK_DGRAM: {
                    if (params.protocol != 0 && params.protocol != IPPROTO_UDP)
                        return nt_status::INVALID_PARAMETER;

                    auto status = create_new_ipv4_udp_socket(ref);
                    if (!success(status))
                        return status;

                    break;
                }

                case SOCK_STREAM: {
                    if (params.protocol != 0 && params.protocol != IPPROTO_TCP)
                        return nt_status::INVALID_PARAMETER;

                    auto status = create_new_ipv4_tcp_socket(ref);
                    if (!success(status))
                        return status;

                    break;
                }

                default:
                    return nt_status::INVALID_PARAMETER;
            }

            HANDLE h;

            auto f = new file(ref.get(), 0, 0);
            if (!f) {
                print("new failed\n");
                return nt_status::INSUFFICIENT_RESOURCES;
            }

            object_ref ref2{*f};

            auto status = t->proc->add_handle(ref2.get(), FILE_READ_DATA | FILE_WRITE_DATA,
                                              true, h);
            if (!success(status))
                return status;

            status = t->proc->write_vm(params.h, &h, sizeof(h), nullptr);
            if (!success(status)) {
                // FIXME - close handle
                return status;
            }

            if (params.type == SOCK_RAW) {
                auto& s = static_cast<raw_socket<ipv4>&>(ref.get());

                lock_guard lg(ipv4_raw_sockets_lock);
                insert_tail_list(ipv4_raw_sockets, s.le);
                num_ipv4_raw_sockets++;
                s.added_to_list = true;
            }

            break;
        }

        case AF_INET6: {
            object_ref ref;

            switch (params.type) {
                case SOCK_RAW: {
                    if (params.protocol == 0)
                        return nt_status::INVALID_PARAMETER;

                    auto s = new raw_socket<ipv6>(params.protocol);
                    if (!s) {
                        print("new failed\n");
                        return nt_status::INSUFFICIENT_RESOURCES;
                    }

                    ref = object_ref{*s};

                    break;
                }

                case SOCK_DGRAM: {
                    if (params.protocol != 0 && params.protocol != IPPROTO_UDP)
                        return nt_status::INVALID_PARAMETER;

                    auto status = create_new_ipv6_udp_socket(ref);
                    if (!success(status))
                        return status;

                    break;
                }

                case SOCK_STREAM: {
                    if (params.protocol != 0 && params.protocol != IPPROTO_TCP)
                        return nt_status::INVALID_PARAMETER;

                    auto status = create_new_ipv6_tcp_socket(ref);
                    if (!success(status))
                        return status;

                    break;
                }

                default:
                    return nt_status::INVALID_PARAMETER;
            }

            HANDLE h;

            auto f = new file(ref.get(), 0, 0);
            if (!f) {
                print("new failed\n");
                return nt_status::INSUFFICIENT_RESOURCES;
            }

            object_ref ref2{*f};

            auto status = t->proc->add_handle(ref2.get(), FILE_READ_DATA | FILE_WRITE_DATA,
                                              true, h);
            if (!success(status))
                return status;

            status = t->proc->write_vm(params.h, &h, sizeof(h), nullptr);
            if (!success(status)) {
                // FIXME - close handle
                return status;
            }

            if (params.type == SOCK_RAW) {
                auto& s = static_cast<raw_socket<ipv6>&>(ref.get());

                lock_guard lg(ipv6_raw_sockets_lock);
                insert_tail_list(ipv6_raw_sockets, s.le);
                num_ipv6_raw_sockets++;
                s.added_to_list = true;
            }

            break;
        }

        case AF_UNIX: {
            object_ref ref;

            switch (params.type) {
                case SOCK_STREAM: {
                    auto status = create_new_unix_socket(ref);
                    if (!success(status))
                        return status;

                    break;
                }

                default:
                    return nt_status::INVALID_PARAMETER;
            }

            HANDLE h;

            auto f = new file(ref.get(), 0, 0);
            if (!f) {
                print("new failed\n");
                return nt_status::INSUFFICIENT_RESOURCES;
            }

            object_ref ref2{*f};

            auto status = t->proc->add_handle(ref2.get(), FILE_READ_DATA | FILE_WRITE_DATA,
                                              true, h);
            if (!success(status))
                return status;

            status = t->proc->write_vm(params.h, &h, sizeof(h), nullptr);
            if (!success(status)) {
                // FIXME - close handle
                return status;
            }

            break;
        }

        default:
            return nt_status::INVALID_PARAMETER;
    }

    return nt_status::SUCCESS;
}

static nt_status netctl_get_netif_count(span<uint8_t> buf, uintptr_t& written) {
    if (buf.size() < sizeof(MOXNOX_NETCTL_GET_NETIF_COUNT))
        return nt_status::BUFFER_TOO_SMALL;

    auto& out = *(MOXNOX_NETCTL_GET_NETIF_COUNT*)buf.data();
    uint32_t num = 0;

    {
        lock_guard lock(netif_lock);

        for (auto le = netif_list.next; le != &netif_list; le = le->next) {
            num++;
        }
    }

    out.count = num;
    written = sizeof(MOXNOX_NETCTL_GET_NETIF_COUNT);

    return nt_status::SUCCESS;
}

static nt_status netctl_get_netif_ids(span<uint8_t> buf, uintptr_t& written) {
    if (buf.size() < offsetof(MOXNOX_NETCTL_GET_NETIF_IDS, id))
        return nt_status::BUFFER_TOO_SMALL;

    size_t left = (buf.size() - offsetof(MOXNOX_NETCTL_GET_NETIF_IDS, id)) / sizeof(uint32_t);

    auto& out = *(MOXNOX_NETCTL_GET_NETIF_IDS*)buf.data();
    uint32_t num = 0;

    {
        lock_guard lock(netif_lock);

        for (auto le = netif_list.next; le != &netif_list; le = le->next) {
            auto& ni = containing_record<netif>(le);

            if (left == 0)
                return nt_status::BUFFER_TOO_SMALL;

            out.id[num] = ni.num;
            num++;
            left--;
        }
    }

    out.count = num;
    written = offsetof(MOXNOX_NETCTL_GET_NETIF_IDS, id) + (num * sizeof(uint32_t));

    return nt_status::SUCCESS;
}

static nt_status netctl_get_netif_info(span<const uint8_t> inbuf,
                                       span<uint8_t> outbuf, uintptr_t& written) {
    if (inbuf.size() < sizeof(uint32_t))
        return nt_status::INVALID_PARAMETER;

    auto id = *(uint32_t*)inbuf.data();

    if (outbuf.size() < sizeof(MOXNOX_NETCTL_GET_NETIF_INFO))
        return nt_status::BUFFER_TOO_SMALL;

    auto& out = *(MOXNOX_NETCTL_GET_NETIF_INFO*)outbuf.data();
    netif* ni = nullptr;

    {
        lock_guard lock(netif_lock);

        for (auto le = netif_list.next; le != &netif_list; le = le->next) {
            auto& ni2 = containing_record<netif>(le);

            if (ni2.num == id) {
                ni2.acquire();
                ni = &ni2;
                break;
            }
        }
    }

    if (!ni)
        return nt_status::NOT_FOUND;

    memcpy(out.name, ni->name, sizeof(ni->name));
    out.ll_addr = ni->ll_addr;

    if (ni->loopback) {
        out.rx_packets = out.tx_packets = lo_packets.load(memory_order::relaxed);
        out.rx_bytes = out.tx_bytes = lo_bytes.load(memory_order::relaxed);
    } else {
        out.rx_packets = ni->rx_packets.load(memory_order::relaxed);
        out.rx_bytes = ni->rx_bytes.load(memory_order::relaxed);
        out.tx_packets = ni->tx_packets.load(memory_order::relaxed);
        out.tx_bytes = ni->tx_bytes.load(memory_order::relaxed);
    }

    ni->release();

    written = sizeof(MOXNOX_NETCTL_GET_NETIF_INFO);
    return nt_status::SUCCESS;
}

static nt_status netctl_get_netif_by_name(span<const uint8_t> inbuf,
                                          span<uint8_t> outbuf,
                                          uintptr_t& written) {
    if (inbuf.size() < sizeof(MOXNOX_NETCTL_GET_NETIF_BY_NAME))
        return nt_status::BUFFER_TOO_SMALL;

    if (outbuf.size() < sizeof(uint32_t))
        return nt_status::BUFFER_TOO_SMALL;

    auto p = *(MOXNOX_NETCTL_GET_NETIF_BY_NAME*)inbuf.data();

    auto& out = *(uint32_t*)outbuf.data();

    {
        lock_guard lock(netif_lock);

        for (auto le = netif_list.next; le != &netif_list; le = le->next) {
            auto& ni = containing_record<netif>(le);

            if (!memcmp(ni.name, p.name, sizeof(p.name))) {
                out = ni.num;
                written = sizeof(uint32_t);
                return nt_status::SUCCESS;
            }
        }
    }

    return nt_status::NOT_FOUND;
}

static nt_status netctl_get_netif_ip_count(span<const uint8_t> inbuf,
                                           span<uint8_t> outbuf,
                                           uintptr_t& written) {
    if (inbuf.size() < sizeof(uint32_t))
        return nt_status::INVALID_PARAMETER;

    auto id = *(uint32_t*)inbuf.data();

    if (outbuf.size() < sizeof(uint32_t))
        return nt_status::BUFFER_TOO_SMALL;

    auto& out = *(uint32_t*)outbuf.data();
    netif* ni = nullptr;

    {
        lock_guard lock(netif_lock);

        for (auto le = netif_list.next; le != &netif_list; le = le->next) {
            auto& ni2 = containing_record<netif>(le);

            if (ni2.num == id) {
                ni2.acquire();
                ni = &ni2;
                break;
            }
        }
    }

    if (!ni)
        return nt_status::NOT_FOUND;

    uint32_t count = 0;

    {
        lock_guard lock(ni->addr_lock);

        for (auto le = ni->addrs.next; le != &ni->addrs; le = le->next) {
            count++;
        }
    }

    ni->release();

    out = count;
    written = sizeof(uint32_t);

    return nt_status::SUCCESS;
}

static nt_status netctl_get_netif_ips(span<const uint8_t> inbuf,
                                      span<uint8_t> outbuf,
                                      uintptr_t& written) {
    if (inbuf.size() < sizeof(uint32_t))
        return nt_status::INVALID_PARAMETER;

    auto id = *(uint32_t*)inbuf.data();

    if (outbuf.size() < offsetof(MOXNOX_NETCTL_GET_NETIF_IPS, ip))
        return nt_status::BUFFER_TOO_SMALL;

    size_t left = (outbuf.size() - offsetof(MOXNOX_NETCTL_GET_NETIF_IPS, ip)) / sizeof(ipv6_address);

    auto& out = *(MOXNOX_NETCTL_GET_NETIF_IPS*)outbuf.data();
    netif* ni = nullptr;

    {
        lock_guard lock(netif_lock);

        for (auto le = netif_list.next; le != &netif_list; le = le->next) {
            auto& ni2 = containing_record<netif>(le);

            if (ni2.num == id) {
                ni2.acquire();
                ni = &ni2;
                break;
            }
        }
    }

    if (!ni)
        return nt_status::NOT_FOUND;

    uint32_t num = 0;
    nt_status ret = nt_status::SUCCESS;

    {
        lock_guard lock(ni->addr_lock);

        for (auto le = ni->addrs.next; le != &ni->addrs; le = le->next) {
            auto& a = containing_record<ip_addr>(le);

            auto* addrv4 = get_if<ipv4_address>(&a.addr);
            auto* addrv6 = get_if<ipv6_address>(&a.addr);

            if (!addrv4 && !addrv6)
                continue;

            if (left == 0) {
                ret = nt_status::BUFFER_TOO_SMALL;
                break;
            }

            if (addrv4) {
                out.ip[num] = parse_ipv6_address("::ffff:0:0");

                out.ip[num].v[12] = addrv4->v[0];
                out.ip[num].v[13] = addrv4->v[1];
                out.ip[num].v[14] = addrv4->v[2];
                out.ip[num].v[15] = addrv4->v[3];
            } else if (addrv6)
                out.ip[num] = *addrv6;

            num++;
            left--;
        }
    }

    ni->release();

    if (!success(ret))
        return ret;

    out.count = num;
    written = offsetof(MOXNOX_NETCTL_GET_NETIF_IPS, ip) + (num * sizeof(ipv6_address));

    return ret;
}

static nt_status netctl_socketpair(span<const uint8_t> inbuf,
                                   span<uint8_t> outbuf, uintptr_t& written) {
    auto& proc = *get_current_thread()->proc;
    nt_status status;
    object_ref ref1, ref2;
    handle_t h1, h2;

    if (inbuf.size() < sizeof(MOXNOX_SOCKETPAIR_IN))
        return nt_status::INVALID_PARAMETER;

    if (outbuf.size() < sizeof(MOXNOX_SOCKETPAIR_OUT))
        return nt_status::INVALID_PARAMETER;

    auto& params_in = *(MOXNOX_SOCKETPAIR_IN*)inbuf.data();
    auto& params_out = *(MOXNOX_SOCKETPAIR_OUT*)outbuf.data();

    if (params_in.domain != AF_UNIX)
        return nt_status::INVALID_PARAMETER;

    if (params_in.protocol != 0)
        return nt_status::INVALID_PARAMETER;

    status = create_unix_socket_pair(params_in.type, ref1, ref2);
    if (!success(status))
        return status;

    status = proc.add_handle(ref1.get(), FILE_READ_DATA | FILE_WRITE_DATA,
                             true, h1);
    if (!success(status))
        return status;

    status = proc.add_handle(ref2.get(), FILE_READ_DATA | FILE_WRITE_DATA,
                             true, h2);
    if (!success(status)) {
        // FIXME - close handle h1
        return status;
    }

    params_out.h1 = h1;
    params_out.h2 = h2;

    written = sizeof(MOXNOX_SOCKETPAIR_OUT);

    return nt_status::SUCCESS;
}

nt_status netctl::ioctl(uint32_t code, span<const uint8_t> inbuf,
                        span<uint8_t> outbuf, uintptr_t& written) {
    switch (code) {
        case IOCTL_MOXNOX_NETCTL_SOCKET:
            return netctl_socket(inbuf);

        case IOCTL_MOXNOX_NETCTL_GET_NETIF_COUNT:
            return netctl_get_netif_count(outbuf, written);

        case IOCTL_MOXNOX_NETCTL_GET_NETIF_IDS:
            return netctl_get_netif_ids(outbuf, written);

        case IOCTL_MOXNOX_NETCTL_GET_NETIF_INFO:
            return netctl_get_netif_info(inbuf, outbuf, written);

        case IOCTL_MOXNOX_NETCTL_GET_NETIF_BY_NAME:
            return netctl_get_netif_by_name(inbuf, outbuf, written);

        case IOCTL_MOXNOX_NETCTL_GET_NETIF_IP_COUNT:
            return netctl_get_netif_ip_count(inbuf, outbuf, written);

        case IOCTL_MOXNOX_NETCTL_GET_NETIF_IPS:
            return netctl_get_netif_ips(inbuf, outbuf, written);

        case IOCTL_MOXNOX_NETCTL_SOCKETPAIR:
            return netctl_socketpair(inbuf, outbuf, written);

        default:
            return nt_status::NOT_IMPLEMENTED;
    }
}

template<socket_family FAMILY>
raw_socket<FAMILY>::~raw_socket() {
    if constexpr (is_same_v<FAMILY, ipv4>) {
        lock_guard lg(ipv4_raw_sockets_lock);

        if (added_to_list) {
            remove_entry_list(le);
            num_ipv4_raw_sockets--;
        }
    } else if constexpr (is_same_v<FAMILY, ipv6>) {
        lock_guard lg(ipv6_raw_sockets_lock);

        if (added_to_list) {
            remove_entry_list(le);
            num_ipv6_raw_sockets--;
        }
    }

    while (true) {
        socket_packet* pp = nullptr;

        {
            lock_guard lg(buffer_lock);

            if (is_list_empty(buffer))
                break;

            auto& p = containing_record<socket_packet>(buffer.next);

            remove_entry_list(p.le);
            pp = &p;
        }

        if (!pp)
            break;

        delete pp;
    }
}

nt_status socket::do_recvmsg(span<const uint8_t> inbuf, bool nonblock) {
    nt_status status;
    uint32_t read_bytes;

    auto& t = *get_current_thread();
    if (!t.proc)
        return nt_status::INTERNAL_ERROR;

    if (inbuf.size() < sizeof(MOXNOX_SOCKET_RECVMSG))
        return nt_status::BUFFER_TOO_SMALL;

    auto& params = *(MOXNOX_SOCKET_RECVMSG*)inbuf.data();

    size_t recv_bytes;
    size_t fromlen = 0, orig_fromlen = 0;
    size_t num_handles = 0;

    if (!params.recv_bytes)
        return nt_status::INVALID_PARAMETER;

    if (params.vec_count > PAGE_SIZE / sizeof(iovec))
        return nt_status::INVALID_PARAMETER;

    if (params.fromlen) {
        status = t.proc->read_vm(params.fromlen, &fromlen, sizeof(fromlen),
                                 &read_bytes);
        if (!success(status))
            return status;

        if (read_bytes < sizeof(fromlen))
            return nt_status::NO_MEMORY;

        orig_fromlen = fromlen;
    }

    if (params.num_handles) {
        status = t.proc->read_vm(params.num_handles, &num_handles, sizeof(num_handles),
                                 &read_bytes);
        if (!success(status))
            return status;

        if (read_bytes < sizeof(num_handles))
            return nt_status::NO_MEMORY;

        if (num_handles > 256)
            return nt_status::INVALID_PARAMETER;
    }

    iovec* usvec = nullptr;

    if (params.vec_count > 0) {
        usvec = (iovec*)malloc(params.vec_count * sizeof(iovec));
        if (!usvec) {
            print("out of memory\n");
            return nt_status::INSUFFICIENT_RESOURCES;
        }

        status = t.proc->read_vm(params.vec, usvec, (uint32_t)(params.vec_count * sizeof(iovec)),
                                &read_bytes);
        if (!success(status)) {
            free(usvec);
            return status;
        }

        if (read_bytes < (uint32_t)(params.vec_count * sizeof(iovec))) {
            free(usvec);
            return nt_status::NO_MEMORY;
        }
    }

    optional<rangelock_req> rlr;
    span<uint8_t>* spans = nullptr;
    size_t num_spans = 0;
    sockaddr_storage from;
    span<HANDLE> handles;
    HANDLE* hbuf = nullptr;
    size_t received_handles = 0;

    for (size_t i = 0; i < params.vec_count; i++) {
        auto& v = usvec[i];

        if (v.iov_len > 0 && (!is_userspace_address((uintptr_t)v.iov_base) || !is_userspace_address((uintptr_t)v.iov_base + v.iov_len - 1))) {
            free(usvec);
            return nt_status::ACCESS_VIOLATION;
        }
    }

    if (usvec) {
        status = pin_iovecs_for_writing(*t.proc, span(usvec, params.vec_count),
                                        rlr, spans, num_spans);
        if (!success(status)) {
            free(usvec);
            return status;
        }

        free(usvec);
    }

    if (num_handles > 0) {
        hbuf = (HANDLE*)malloc(sizeof(HANDLE) * num_handles);
        if (!hbuf) {
            print("malloc failed\n");
            return nt_status::INSUFFICIENT_RESOURCES;
        }

        handles = span(hbuf, num_handles);
    }

    if (nonblock) {
        status = this->recvfrom_nonblock(span(spans, num_spans),
                                         params.flags, &from,
                                         fromlen != 0 ? &fromlen : nullptr,
                                         recv_bytes, handles, received_handles);
    } else {
        status = this->recvfrom(span(spans, num_spans), params.flags,
                                &from, fromlen != 0 ? &fromlen : nullptr,
                                recv_bytes, handles, received_handles);
    }

    rlr.reset();

    auto spans_size = sizeof(span<uint8_t>) * num_spans;
    if (spans_size > PAGE_SIZE)
        vfree(spans, spans_size);
    else if (spans_size > 0)
        free(spans);

    if (!success(status) || status == nt_status::USER_APC) {
        if (hbuf)
            free(hbuf);

        return status;
    }

    if (received_handles > 0) {
        status = t.proc->write_vm(params.handles, hbuf,
                                  (uint32_t)(min(num_handles, received_handles) * sizeof(HANDLE)),
                                  nullptr);
        if (!success(status)) {
            free(hbuf);
            return status;
        }
    }

    if (hbuf)
        free(hbuf);

    if (params.num_handles) {
        status = t.proc->write_vm(params.num_handles, &received_handles,
                                  sizeof(received_handles), nullptr);
        if (!success(status))
            return status;
    }

    if (params.from && fromlen != 0) {
        status = t.proc->write_vm(params.from, &from,
                                  (uint32_t)min(orig_fromlen, fromlen), nullptr);
        if (!success(status))
            return status;
    }

    if (params.fromlen) {
        status = t.proc->write_vm(params.fromlen, &fromlen, (uint32_t)sizeof(size_t),
                                  nullptr);
        if (!success(status))
            return status;
    }

    status = t.proc->write_vm(params.recv_bytes, &recv_bytes, sizeof(size_t),
                              nullptr);
    if (!success(status))
        return status;

    return nt_status::SUCCESS;
}

nt_status socket::do_sendmsg(span<const uint8_t> inbuf, bool nonblock) {
    nt_status status;
    uint32_t read_bytes;

    auto& t = *get_current_thread();
    if (!t.proc)
        return nt_status::INTERNAL_ERROR;

    if (inbuf.size() < offsetof(MOXNOX_SOCKET_SENDMSG, handles))
        return nt_status::BUFFER_TOO_SMALL;

    auto& params = *(MOXNOX_SOCKET_SENDMSG*)inbuf.data();

    if (params.num_handles > 256)
        return nt_status::INVALID_PARAMETER;

    if (inbuf.size() < offsetof(MOXNOX_SOCKET_SENDMSG, handles) + (sizeof(HANDLE) * params.num_handles))
        return nt_status::INVALID_PARAMETER;

    sockaddr_storage to;
    size_t sent_bytes;

    if ((params.tolen == 0 && params.to) || (params.tolen != 0 && !params.to))
        return nt_status::INVALID_PARAMETER;

    if (!params.sent_bytes)
        return nt_status::INVALID_PARAMETER;

    if (params.vec_count > PAGE_SIZE / sizeof(iovec))
        return nt_status::INVALID_PARAMETER;

    if (params.to) {
        if ((size_t)params.tolen > sizeof(sockaddr_storage))
            return nt_status::INVALID_PARAMETER;

        status = t.proc->read_vm(params.to, &to, (uint32_t)params.tolen,
                                 &read_bytes);
        if (!success(status))
            return status;

        if (read_bytes < (uint32_t)params.tolen)
            return nt_status::NO_MEMORY;
    }

    iovec* usvec = nullptr;

    if (params.vec_count > 0) {
        usvec = (iovec*)malloc(params.vec_count * sizeof(iovec));
        if (!usvec) {
            print("out of memory\n");
            return nt_status::INSUFFICIENT_RESOURCES;
        }

        status = t.proc->read_vm(params.vec, usvec, (uint32_t)(params.vec_count * sizeof(iovec)),
                                &read_bytes);
        if (!success(status)) {
            free(usvec);
            return status;
        }

        if (read_bytes < (uint32_t)(params.vec_count * sizeof(iovec))) {
            free(usvec);
            return nt_status::NO_MEMORY;
        }
    }

    optional<rangelock_req> rlr;
    span<const uint8_t>* spans = nullptr;
    size_t num_spans = 0;

    for (size_t i = 0; i < params.vec_count; i++) {
        auto& v = usvec[i];

        if (v.iov_len > 0 && (!is_userspace_address((uintptr_t)v.iov_base) || !is_userspace_address((uintptr_t)v.iov_base + v.iov_len - 1))) {
            free(usvec);
            return nt_status::ACCESS_VIOLATION;
        }
    }

    if (usvec) {
        status = pin_iovecs_for_reading(*t.proc, span(usvec, params.vec_count), rlr,
                                        spans, num_spans);
        if (!success(status)) {
            free(usvec);
            return status;
        }

        free(usvec);
    }

    if (nonblock) {
        status = this->sendto_nonblock(span(spans, num_spans), params.flags,
                                       params.to ? (sockaddr*)&to : nullptr,
                                       (int)params.tolen, sent_bytes,
                                       span(params.handles, params.num_handles));
    } else {
        status = this->sendto(span(spans, num_spans), params.flags,
                              params.to ? (sockaddr*)&to : nullptr, (int)params.tolen,
                              sent_bytes, span(params.handles, params.num_handles));
    }

    rlr.reset();

    auto spans_size = sizeof(span<const uint8_t>) * num_spans;
    if (spans_size > PAGE_SIZE)
        vfree(spans, spans_size);
    else if (spans_size > 0)
        free(spans);

    if (!success(status) || status == nt_status::USER_APC)
        return status;

    status = t.proc->write_vm(params.sent_bytes, &sent_bytes, sizeof(size_t),
                              nullptr);
    if (!success(status))
        return status;

    return nt_status::SUCCESS;
}

nt_status socket::do_bind(span<const uint8_t> inbuf) {
    if (inbuf.size() < sizeof(sockaddr))
        return nt_status::BUFFER_TOO_SMALL;

    return this->bind((sockaddr*)inbuf.data(), inbuf.size());
}

nt_status socket::do_connect(span<const uint8_t> inbuf) {
    if (inbuf.size() < sizeof(sockaddr))
        return nt_status::BUFFER_TOO_SMALL;

    return this->connect((sockaddr*)inbuf.data(), inbuf.size());
}

nt_status socket::do_listen(span<const uint8_t> inbuf) {
    if (inbuf.size() < sizeof(int))
        return nt_status::BUFFER_TOO_SMALL;

    auto backlog = *(int*)inbuf.data();

    return this->listen(backlog);
}

nt_status socket::do_accept(span<const uint8_t> inbuf, span<uint8_t> outbuf,
                            uintptr_t& written_bytes) {
    if (inbuf.size() < sizeof(MOXNOX_SOCKET_ACCEPT_IN))
        return nt_status::BUFFER_TOO_SMALL;

    if (outbuf.size() < sizeof(MOXNOX_SOCKET_ACCEPT_OUT))
        return nt_status::BUFFER_TOO_SMALL;

    auto& t = *get_current_thread();
    if (!t.proc)
        return nt_status::INTERNAL_ERROR;

    auto& params_in = *(MOXNOX_SOCKET_ACCEPT_IN*)inbuf.data();
    auto& params_out = *(MOXNOX_SOCKET_ACCEPT_OUT*)outbuf.data();

    HANDLE sock;
    int addrlen = params_in.addrlen;

    auto status = this->accept(sock, params_in.addr, addrlen);

    if (!success(status))
        return status;

    params_out.sock = sock;
    params_out.addrlen = addrlen;

    written_bytes = sizeof(MOXNOX_SOCKET_ACCEPT_OUT);

    return nt_status::SUCCESS;
}

nt_status socket::do_getsockname(span<const uint8_t> inbuf, span<uint8_t> outbuf,
                                 uintptr_t& written_bytes) {
    auto& t = *get_current_thread();
    size_t len;

    // FIXME - should be socklen_t rather than int

    if (inbuf.size() < sizeof(MOXNOX_SOCKET_GETSOCKNAME))
        return nt_status::INVALID_PARAMETER;

    if (outbuf.size() < sizeof(int))
        return nt_status::INVALID_PARAMETER;

    auto& p = *(MOXNOX_SOCKET_GETSOCKNAME*)inbuf.data();

    if (!p.addr || p.addrlen == 0 || p.addrlen > PAGE_SIZE)
        return nt_status::INVALID_PARAMETER;

    auto buf = (uint8_t*)malloc(p.addrlen);
    if (!buf) {
        print("malloc failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    auto status = this->getsockname(span(buf, p.addrlen), len);

    if (!success(status)) {
        free(buf);
        return status;
    }

    if (!t.proc->copy_to(p.addr, buf, len)) {
        free(buf);
        return nt_status::ACCESS_VIOLATION;
    }

    free(buf);

    *(int*)outbuf.data() = (int)len;
    written_bytes = sizeof(int);

    return status;
}

nt_status socket::do_getpeername(span<const uint8_t> inbuf, span<uint8_t> outbuf,
                                 uintptr_t& written_bytes) {
    auto& t = *get_current_thread();
    size_t len;

    // FIXME - should be socklen_t rather than int

    if (inbuf.size() < sizeof(MOXNOX_SOCKET_GETSOCKNAME))
        return nt_status::INVALID_PARAMETER;

    if (outbuf.size() < sizeof(int))
        return nt_status::INVALID_PARAMETER;

    auto& p = *(MOXNOX_SOCKET_GETSOCKNAME*)inbuf.data();

    if (!p.addr || p.addrlen == 0 || p.addrlen > PAGE_SIZE)
        return nt_status::INVALID_PARAMETER;

    auto buf = (uint8_t*)malloc(p.addrlen);
    if (!buf) {
        print("malloc failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    auto status = this->getpeername(span(buf, p.addrlen), len);

    if (!success(status)) {
        free(buf);
        return status;
    }

    if (!t.proc->copy_to(p.addr, buf, len)) {
        free(buf);
        return nt_status::ACCESS_VIOLATION;
    }

    free(buf);

    *(int*)outbuf.data() = (int)len;
    written_bytes = sizeof(int);

    return status;
}

nt_status socket::getsockopt_error(span<uint8_t> buf, socklen_t& retlen) {
    // FIXME - do this properly

    if (buf.size() < sizeof(int))
        return nt_status::BUFFER_TOO_SMALL;

    // FIXME - resets error when called
    // FIXME - error should be an errno value (return nt_status, and get ws2_32 translating?)

    *(int*)buf.data() = 0;

    retlen = sizeof(int);

    return nt_status::SUCCESS;
}

nt_status socket::getsockopt(int level, int optname, span<uint8_t> buf,
                             socklen_t& retlen) {
    if (level == SOL_SOCKET) {
        auto opt = (sockopt_socket)optname;

        switch (opt) {
            case sockopt_socket::ERROR:
                return getsockopt_error(buf, retlen);

            default:
                print("FIXME - getsockopt(SOL_SOCKET, {})\n", opt);
                return nt_status::NOT_SUPPORTED;
        }
    } else if (level == IPPROTO_TCP)
        return getsockopt_tcp((sockopt_tcp)optname, buf, retlen);
    else if (level == IPPROTO_IPV6)
        return getsockopt_ipv6((sockopt_ipv6)optname, buf, retlen);
    else
        print("FIXME - getsockopt({}, {})\n", level, optname);

    return nt_status::NOT_SUPPORTED;
}

nt_status socket::do_getsockopt(span<const uint8_t> inbuf) {
    auto& t = *get_current_thread();
    nt_status status;
    socklen_t len, retlen;

    if (inbuf.size() < sizeof(MOXNOX_SOCKET_GETSOCKOPT))
        return nt_status::BUFFER_TOO_SMALL;

    auto& params = *(MOXNOX_SOCKET_GETSOCKOPT*)inbuf.data();

    if (!params.optval || !params.optlen)
        return nt_status::INVALID_PARAMETER;

    if (!t.proc->copy_from(&len, params.optlen, sizeof(len)))
        return nt_status::ACCESS_VIOLATION;

    if (len <= 0 || (size_t)len > PAGE_SIZE)
        return nt_status::INVALID_PARAMETER;

    auto buf = (uint8_t*)malloc((size_t)len);
    if (!buf) {
        print("malloc failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    status = this->getsockopt(params.level, params.optname, span(buf, (size_t)len),
                              retlen);
    if (!success(status)) {
        free(buf);
        return status;
    }

    if (!t.proc->copy_to(params.optval, buf, (size_t)retlen)) {
        free(buf);
        return nt_status::ACCESS_VIOLATION;
    }

    free(buf);

    if (!t.proc->copy_to(params.optlen, &retlen, sizeof(retlen)))
        return nt_status::ACCESS_VIOLATION;

    return status;
}

nt_status socket::setsockopt(int level, int optname, span<const uint8_t> buf) {
    if (level == SOL_SOCKET) {
        auto opt = (sockopt_socket)optname;

        switch (opt) {
            using enum sockopt_socket;

            case KEEPALIVE:
                if (buf.size() < sizeof(int))
                    return nt_status::INVALID_PARAMETER;

                // FIXME - actually doing keep-alives

                return nt_status::SUCCESS;

            case REUSEADDR:
                if (buf.size() < sizeof(int))
                    return nt_status::INVALID_PARAMETER;

                // FIXME - actually reusing addresses

                return nt_status::SUCCESS;

            default:
                print("FIXME - setsockopt(SOL_SOCKET, {}, {})\n", opt, buf.size());
                return nt_status::NOT_SUPPORTED;
        }

    } else if (level == IPPROTO_TCP)
        return setsockopt_tcp((sockopt_tcp)optname, buf);
    else if (level == IPPROTO_IPV6)
        return setsockopt_ipv6((sockopt_ipv6)optname, buf);
    else
        print("FIXME - setsockopt({}, {}, {})\n", level, optname, buf.size());

    return nt_status::NOT_SUPPORTED;
}

nt_status socket::do_setsockopt(span<const uint8_t> inbuf) {
    auto& t = *get_current_thread();
    nt_status status;

    if (inbuf.size() < sizeof(MOXNOX_SOCKET_SETSOCKOPT))
        return nt_status::BUFFER_TOO_SMALL;

    auto& params = *(MOXNOX_SOCKET_SETSOCKOPT*)inbuf.data();

    if (!params.optval || !params.optlen)
        return nt_status::INVALID_PARAMETER;

    if (params.optlen <= 0 || (size_t)params.optlen > PAGE_SIZE)
        return nt_status::INVALID_PARAMETER;

    auto len = (size_t)params.optlen;

    auto buf = (uint8_t*)malloc(len);
    if (!buf) {
        print("malloc failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    if (!t.proc->copy_from(buf, params.optval, len)) {
        free(buf);
        return nt_status::ACCESS_VIOLATION;
    }

    status = this->setsockopt(params.level, params.optname, span(buf, len));

    free(buf);

    return status;
}

nt_status socket::do_shutdown(span<const uint8_t> inbuf) {
    if (inbuf.size() < sizeof(int))
        return nt_status::BUFFER_TOO_SMALL;

    auto how = *(int*)inbuf.data();

    return this->shutdown(how);
}

nt_status socket::ioctl(uint32_t code, span<const uint8_t> inbuf,
                        span<uint8_t> outbuf, uintptr_t& written_bytes,
                        bool nonblock) {
    switch (code) {
        case IOCTL_MOXNOX_SOCKET_RECVMSG:
            return do_recvmsg(inbuf, nonblock);

        case IOCTL_MOXNOX_SOCKET_SENDMSG:
            return do_sendmsg(inbuf, nonblock);

        case IOCTL_MOXNOX_SOCKET_BIND:
            return do_bind(inbuf);

        case IOCTL_MOXNOX_SOCKET_CONNECT:
            return do_connect(inbuf);

        case IOCTL_MOXNOX_SOCKET_LISTEN:
            return do_listen(inbuf);

        case IOCTL_MOXNOX_SOCKET_ACCEPT:
            return do_accept(inbuf, outbuf, written_bytes);

        case IOCTL_MOXNOX_SOCKET_GETSOCKNAME:
            return do_getsockname(inbuf, outbuf, written_bytes);

        case IOCTL_MOXNOX_SOCKET_GETPEERNAME:
            return do_getpeername(inbuf, outbuf, written_bytes);

        case IOCTL_MOXNOX_SOCKET_GETSOCKOPT:
            return do_getsockopt(inbuf);

        case IOCTL_MOXNOX_SOCKET_SETSOCKOPT:
            return do_setsockopt(inbuf);

        case IOCTL_MOXNOX_SOCKET_SHUTDOWN:
            return do_shutdown(inbuf);

        default:
            return nt_status::INVALID_PARAMETER;
    }
}

template<socket_family FAMILY>
template<bool NONBLOCK>
nt_status raw_socket<FAMILY>::do_recvfrom(span<const span<uint8_t>> v, int flags,
                                          sockaddr_storage* from, size_t* fromlen,
                                          uint64_t& recv_bytes) {
    // FIXME - MSG_TRUNC
    if (flags != 0)
        return nt_status::INVALID_PARAMETER;

    auto vec_size = get_vec_size(v);
    span<uint8_t> top = vec_size == 0 ? span<uint8_t>() : v.front();

    acquire();

    while (true) {
        nt_status status;

        if constexpr (NONBLOCK) {
            status = wait_for_object(*this, true, 0);

            if (status == nt_status::TIMEOUT) {
                release();
                return nt_status::CANT_WAIT;
            }
        } else
            status = wait_for_object(*this, true);

        if (!success(status) || status == nt_status::USER_APC || status == nt_status::ALERTED) {
            release();
            return status;
        }

        if (dead) {
            release();
            return nt_status::CONNECTION_DISCONNECTED;
        }

        socket_packet* sp = nullptr;
        nt_status error = nt_status::SUCCESS;

        {
            sync_lock sl;
            lock_guard lock(sl);
            lock_guard lock2(buffer_lock);

            if (!success(pending_error)) {
                error = pending_error;
                pending_error = nt_status::SUCCESS;

                if (is_list_empty(buffer))
                    signalled = false;
            }

            if (!is_list_empty(buffer)) {
                sp = &containing_record<socket_packet>(buffer.next);
                remove_entry_list(sp->le);

                if (is_list_empty(buffer))
                    signalled = false;
            }
        }

        if (!success(error)) {
            release();
            return error;
        }

        if (!sp)
            continue;

        size_t header_len;

        // header gets stripped for IPv6 but not IPv4
        if constexpr (is_same_v<FAMILY, ipv6>)
            header_len = sizeof(typename FAMILY::header);
        else
            header_len = 0;

        auto to_copy = (uint32_t)min(sp->pack.size - header_len, vec_size);

        memcpy_vec(v, top, sp->pack.data + header_len, to_copy);

        recv_bytes = to_copy;

        if (from && fromlen && *fromlen > 0) {
            if constexpr (is_same_v<FAMILY, ipv4>) {
                auto& addr = *(sockaddr_in*)from;
                auto& ip = *(ipv4_header*)sp->pack.data;

                memset(&addr, 0, sizeof(addr));

                addr.sin_family = AF_INET;
                addr.sin_port = 0;
                memcpy(&addr.sin_addr, ip.src.v.data(), sizeof(ipv4_address));

                *fromlen = sizeof(addr);
            } else if constexpr (is_same_v<FAMILY, ipv6>) {
                auto& addr = *(sockaddr_in6*)from;
                auto& ip = *(ipv6_header*)sp->pack.data;

                memset(&addr, 0, sizeof(addr));

                addr.sin6_family = AF_INET6;
                addr.sin6_port = 0;
                addr.sin6_flowinfo = 0; // FIXME? Allowed by RFC3493, but not necessarily what Linux does
                memcpy(&addr.sin6_addr, ip.src.v.data(), sizeof(ipv6_address));
                addr.sin6_scope_id = sp->scope_id;

                *fromlen = sizeof(addr);
            }
        }

        delete sp;

        break;
    }

    release();

    return nt_status::SUCCESS;
}

static bool is_link_local(const ipv6_address& addr) {
    // link-local unicast
    if (addr.v[0] == 0xfe && (addr.v[1] & 0xc0) == 0x80)
        return true;

    // interface-local multicast or link-local multicast
    if (addr.v[0] == 0xff && (addr.v[1] == 0x01 || addr.v[1] == 0x02))
        return true;

    return false;
}

static bool ipv4_prefix_match(const ipv4_address& addr, const ipv4_address& pref,
                              uint8_t len) {
    auto addr2 = zero_right_bits(addr, 32 - len);

    return addr2 == pref;
}

static bool ipv6_prefix_match(const ipv6_address& addr, const ipv6_address& pref,
                              uint8_t len) {
    auto addr2 = zero_right_bits(addr, 128 - len);

    return addr2 == pref;
}

static netif* lookup_route(const ipv6_address& dst) {
    lock_guard lock(ipv6_routes_lock);

    ipv6_route* best_match = nullptr;

    for (auto le = ipv6_routes.next; le != &ipv6_routes; le = le->next) {
        auto& r = containing_record<ipv6_route>(le);

        if (!ipv6_prefix_match(dst, r.prefix, r.prefix_length))
            continue;

        if (!best_match || (r.prefix_length > best_match->prefix_length))
            best_match = &r;
    }

    if (!best_match)
        return nullptr;

    best_match->ni.acquire();

    return &best_match->ni;
}

static netif* lookup_route(const ipv4_address& dst, ipv4_address& gateway) {
    lock_guard lock(ipv4_routes_lock);

    ipv4_route* best_match = nullptr;

    for (auto le = ipv4_routes.next; le != &ipv4_routes; le = le->next) {
        auto& r = containing_record<ipv4_route>(le);

        if (!ipv4_prefix_match(dst, r.prefix, r.prefix_length))
            continue;

        if (!best_match || (r.prefix_length > best_match->prefix_length))
            best_match = &r;
    }

    if (!best_match)
        return nullptr;

    best_match->ni.acquire();

    gateway = best_match->gateway;

    return &best_match->ni;
}

static uint8_t ipv6_match_len(const ipv6_address& addr1, const ipv6_address& addr2) {
    uint8_t ret = 0;

    for (uint8_t i = 0; i < 16; i++) {
        if (addr1.v[i] == addr2.v[i])
            ret += 8;
        else {
            ret += (uint8_t)countl_zero((uint8_t)(addr1.v[i] ^ addr2.v[i]));
            break;
        }
    }

    return ret;
}

nt_status ipv4_choose_interface(const ipv4_address& dst, netif*& ni,
                                ipv4_address& src, ipv4_address& gateway) {
    // bind overrides everything
    if (src != parse_ipv4_address("0.0.0.0")) {
        ni = nullptr;

        if (src.v[0] == 127) { // 127.0.0.0/8 loopback
            lo_netif->acquire();
            ni = lo_netif;
        } else {
            lock_guard lg(netif_lock);

            for (auto le = netif_list.next; le != &netif_list; le = le->next) {
                auto& ni2 = containing_record<netif>(le);

                {
                    lock_guard lock(ni2.addr_lock);

                    for (auto le2 = ni2.addrs.next; le2 != &ni2.addrs; le2 = le2->next) {
                        auto& a = containing_record<ip_addr>(le2);

                        auto* addr = get_if<ipv4_address>(&a.addr);

                        if (!addr)
                            continue;

                        if (*addr == src) {
                            ni2.acquire();

                            ni = &ni2;
                            break;
                        }
                    }
                }

                if (ni)
                    break;
            }
        }

        if (ni) {
            bool found_gateway = false;

            if (dst == parse_ipv4_address("255.255.255.255")) {
                gateway = parse_ipv4_address("0.0.0.0");
                return nt_status::SUCCESS;
            }

            {
                lock_guard lock(ipv4_routes_lock);

                ipv4_route* best_match = nullptr;

                for (auto le = ipv4_routes.next; le != &ipv4_routes; le = le->next) {
                    auto& r = containing_record<ipv4_route>(le);

                    if (&r.ni != ni)
                        continue;

                    if (!ipv4_prefix_match(dst, r.prefix, r.prefix_length))
                        continue;

                    if (!best_match || (r.prefix_length > best_match->prefix_length))
                        best_match = &r;
                }

                if (best_match) {
                    gateway = best_match->gateway;
                    found_gateway = true;
                }
            }

            if (!found_gateway) {
                ni->release();
                return nt_status::NETWORK_UNREACHABLE;
            }

            return nt_status::SUCCESS;
        }

        return nt_status::NETWORK_UNREACHABLE;
    }

    // prefer same address

    {
        lock_guard lg(netif_lock);

        for (auto le = netif_list.next; le != &netif_list; le = le->next) {
            auto& ni2 = containing_record<netif>(le);

            {
                lock_guard lock(ni2.addr_lock);

                for (auto le2 = ni2.addrs.next; le2 != &ni2.addrs; le2 = le2->next) {
                    auto& a = containing_record<ip_addr>(le2);

                    auto* addr = get_if<ipv4_address>(&a.addr);

                    if (!addr)
                        continue;

                    if (*addr == dst) {
                        ni2.acquire();

                        ni = &ni2;
                        src = dst;
                        gateway = parse_ipv4_address("0.0.0.0");

                        return nt_status::SUCCESS;
                    }
                }
            }
        }
    }

    ni = lookup_route(dst, gateway);
    if (!ni)
        return nt_status::HOST_UNREACHABLE;

    if (dst == parse_ipv4_address("255.255.255.255"))
        gateway = parse_ipv4_address("0.0.0.0");

    auto& n = *ni;

    {
        lock_guard lock(n.addr_lock);

        ip_addr* best = nullptr;

        for (auto le = n.addrs.next; le != &n.addrs; le = le->next) {
            auto& a = containing_record<ip_addr>(le);

            auto* addr = get_if<ipv4_address>(&a.addr);

            if (!addr)
                continue;

            best = &a;
            break;
        }

        if (!best) {
            ni->release();
            return nt_status::NETWORK_UNREACHABLE;
        }

        src = *get_if<ipv4_address>(&best->addr);
        return nt_status::SUCCESS;
    }
}

nt_status ipv6_choose_interface(const ipv6_address& dst, uint32_t scope_id,
                                netif*& ni, ipv6_address& src) {
    // see RFC6724

    // bind overrides everything
    if (src != parse_ipv6_address("::")) {
        lock_guard lg(netif_lock);

        for (auto le = netif_list.next; le != &netif_list; le = le->next) {
            auto& ni2 = containing_record<netif>(le);

            {
                lock_guard lock(ni2.addr_lock);

                for (auto le2 = ni2.addrs.next; le2 != &ni2.addrs; le2 = le2->next) {
                    auto& a = containing_record<ip_addr>(le2);

                    auto* addr = get_if<ipv6_address>(&a.addr);

                    if (!addr)
                        continue;

                    if (*addr == src) {
                        ni2.acquire();

                        ni = &ni2;
                        return nt_status::SUCCESS;
                    }
                }
            }
        }

        return nt_status::NETWORK_UNREACHABLE;
    }

    // rule 1: prefer same address

    {
        lock_guard lg(netif_lock);

        for (auto le = netif_list.next; le != &netif_list; le = le->next) {
            auto& ni2 = containing_record<netif>(le);

            {
                lock_guard lock(ni2.addr_lock);

                for (auto le2 = ni2.addrs.next; le2 != &ni2.addrs; le2 = le2->next) {
                    auto& a = containing_record<ip_addr>(le2);

                    auto* addr = get_if<ipv6_address>(&a.addr);

                    if (!addr)
                        continue;

                    if (*addr == dst) {
                        ni2.acquire();

                        ni = &ni2;
                        src = dst;
                        return nt_status::SUCCESS;
                    }
                }
            }
        }
    }

    // rule 2: if link-local, choose link-local

    if (is_link_local(dst)) {
        if (scope_id == 0)
            return nt_status::NETWORK_UNREACHABLE;

        ni = nullptr;

        {
            lock_guard lg(netif_lock);

            for (auto le = netif_list.next; le != &netif_list; le = le->next) {
                auto& ni2 = containing_record<netif>(le);

                if (ni2.num == scope_id) {
                    ni = &ni2;
                    ni2.acquire();
                    break;
                }
            }
        }

        if (!ni)
            return nt_status::NETWORK_UNREACHABLE;

        src = ni->ll_addr;
        return nt_status::SUCCESS;
    }

    ni = lookup_route(dst);
    if (!ni)
        return nt_status::HOST_UNREACHABLE;

    // FIXME - consult routing tables

    // FIXME - rule 3: avoid deprecated address
    // rule 4: prefer home addresses (Mobile IPv6, not relevant)
    // FIXME - rule 5: prefer outgoing interface
    // FIXME - rule 6: prefer matching label
    // FIXME - rule 7: prefer temporary addresses

    // rule 8: prefer longest matching prefix

    auto& n = *ni;

    {
        lock_guard lock(n.addr_lock);

        ip_addr* best = nullptr;
        uint8_t best_match_len = 0;

        for (auto le = n.addrs.next; le != &n.addrs; le = le->next) {
            auto& a = containing_record<ip_addr>(le);

            auto* addr = get_if<ipv6_address>(&a.addr);

            if (!addr)
                continue;

            auto match_len = ipv6_match_len(dst, *addr);

            if (!best || match_len > best_match_len) {
                best = &a;
                best_match_len = match_len;
            }
        }

        if (!best) {
            ni->release();
            return nt_status::NETWORK_UNREACHABLE;
        }

        src = *get_if<ipv6_address>(&best->addr);
        return nt_status::SUCCESS;
    }
}

static void send_neighbour_solicitation(netif& ni, const ipv6_address& dst) {
    struct ns_pkt {
        ipv6_header ip;
        icmpv6_neighbour_solicitation ns;
        ndp_option_lladdr opt;
    } __attribute__((packed));

    auto pack = new packet;
    if (!pack) {
        print("new failed\n");
        return;
    }

    auto& pkt = *(ns_pkt*)pack->data;
    pack->size = sizeof(pkt);

    memset(&pkt, 0, sizeof(pkt));

    mac_address dst_mac;

    dst_mac.v[0] = 0x33;
    dst_mac.v[1] = 0x33;
    dst_mac.v[2] = 0xff;
    dst_mac.v[3] = dst.v[13];
    dst_mac.v[4] = dst.v[14];
    dst_mac.v[5] = dst.v[15];

    pkt.ip.version = 6;
    pkt.ip.payload_length = byteswap((uint16_t)(sizeof(icmpv6_neighbour_solicitation) + sizeof(ndp_option_lladdr)));
    pkt.ip.next_header = (uint8_t)ip_proto::icmpv6;
    pkt.ip.hop_limit = 255;
    pkt.ip.src = ni.ll_addr;

    pkt.ip.dst.v[0] = 0xff;
    pkt.ip.dst.v[1] = 0x2;
    pkt.ip.dst.v[2] = 0x0;
    pkt.ip.dst.v[3] = 0x0;
    pkt.ip.dst.v[4] = 0x0;
    pkt.ip.dst.v[5] = 0x0;
    pkt.ip.dst.v[6] = 0x0;
    pkt.ip.dst.v[7] = 0x0;
    pkt.ip.dst.v[8] = 0x0;
    pkt.ip.dst.v[9] = 0x0;
    pkt.ip.dst.v[10] = 0x0;
    pkt.ip.dst.v[11] = 0x1;
    pkt.ip.dst.v[12] = 0xff;
    pkt.ip.dst.v[13] = dst.v[13];
    pkt.ip.dst.v[14] = dst.v[14];
    pkt.ip.dst.v[15] = dst.v[15];

    pkt.ns.type = icmpv6_type::neighbour_solicitation;
    pkt.ns.target = dst;

    pkt.opt.type = ndp_option_type::source_lladdr;
    pkt.opt.length = 1; // 8 bytes
    pkt.opt.mac = ni.mac;

    pkt.ns.checksum = calc_ipv6_checksum(pkt.ip, span((uint8_t*)&pkt.ns, sizeof(pkt.ns) + sizeof(pkt.opt)), IPPROTO_ICMPV6);

    auto status = ni.send_packet(dst_mac, ethertype::ipv6, *pack);
    if (!success(status))
        print("send_packet returned {}\n", status);

    pack->release();
}

static mac_address search_neighbour_cache(netif& ni, const ipv6_address& dst) {
    neighbour_cache_entry<ipv6>* nce = nullptr;

    {
        lock_guard lock(ni.neighbour_lock);

        for (auto le = ni.neighbour_cache.next; le != &ni.neighbour_cache; le = le->next) {
            auto& c = containing_record<neighbour_cache_entry<ipv6>>(le);

            if (c.addr == dst) {
                if (c.status == neighbour_cache_status::reachable)
                    return c.mac;

                c.acquire();
                nce = &c;
                break;
            }
        }
    }

    if (nce) { // found, but incomplete
        wait_for_object(nce->event.get(), false);

        if (nce->status == neighbour_cache_status::reachable) {
            auto ret = nce->mac;

            nce->release();
            return ret;
        } else
            nce->release();
    }

    nce = new neighbour_cache_entry<ipv6>(ni);
    if (!nce) {
        print("new failed\n");
        return NULL_MAC_ADDRESS;
    }

    neighbour_cache_entry<ipv6>* nce2 = nullptr;

    nce->addr = dst;

    nce->event = create_event(false);
    if (!nce->event) {
        print("create_event failed\n");
        nce->release();
        return NULL_MAC_ADDRESS;
    }

    nce->status = neighbour_cache_status::incomplete;
    nce->last_update_time = kernel_time_page.system_time;

    {
        lock_guard lock(ni.neighbour_lock);

        for (auto le = ni.neighbour_cache.next; le != &ni.neighbour_cache; le = le->next) {
            auto& c = containing_record<neighbour_cache_entry<ipv6>>(le);

            if (c.addr == dst) {
                c.acquire();
                nce2 = &c;
                break;
            }
        }

        if (!nce2) {
            insert_tail_list(ni.neighbour_cache, nce->le);
            nce->acquire();
        }
    }

    if (nce2) { // race and we lost
        nce->release();

        wait_for_object(nce2->event.get(), false);

        if (nce2->status == neighbour_cache_status::reachable) {
            auto ret = nce2->mac;

            nce2->release();
            return ret;
        } else {
            nce2->release();
            return NULL_MAC_ADDRESS;
        }
    }

    for (unsigned int i = 0; i < 3; i++) {
        uint64_t timeout = SYSTEM_TIME_RESOLUTION; // 1 second

        send_neighbour_solicitation(ni, dst);

        auto status = wait_for_object(nce->event.get(), false, timeout);

        if (status == nt_status::TIMEOUT)
            continue;
        else if (!success(status)) {
            set_event(static_cast<event&>(nce->event.get()));

            {
                lock_guard lock(ni.neighbour_lock);

                if (nce->status != neighbour_cache_status::reachable) {
                    remove_entry_list(nce->le);
                    nce->release();
                }
            }

            nce->release();

            print("wait_for_object returned {}\n", status);
            return NULL_MAC_ADDRESS;
        }

        if (nce->status == neighbour_cache_status::reachable) {
            auto ret = nce->mac;

            nce->release();
            return ret;
        }
    }

    set_event(static_cast<event&>(nce->event.get()));

    auto ret = NULL_MAC_ADDRESS;

    {
        lock_guard lock(ni.neighbour_lock);

        if (nce->status == neighbour_cache_status::reachable)
            ret = nce->mac;
        else {
            remove_entry_list(nce->le);
            nce->release();
        }
    }

    nce->release();

    return ret;
}

mac_address find_mac_address(netif& ni, const ipv6_address& dst) {
    if (dst.v[0] == 0xff) { // multicast
        mac_address ret;

        ret.v[0] = 0x33;
        ret.v[1] = 0x33;
        ret.v[2] = dst.v[12];
        ret.v[3] = dst.v[13];
        ret.v[4] = dst.v[14];
        ret.v[5] = dst.v[15];

        return ret;
    }

    if (is_link_local(dst))
        return search_neighbour_cache(ni, dst);

    bool on_link = false;

    {
        lock_guard lock(ni.prefix_lock);

        for (auto le = ni.prefixes.next; le != &ni.prefixes; le = le->next) {
            auto& p = containing_record<ipv6_prefix>(le);

            if (ipv6_prefix_match(dst, p.prefix, p.prefix_length)) {
                on_link = p.on_link;
                break;
            }
        }
    }

    if (on_link)
        return search_neighbour_cache(ni, dst);
    else {
        lock_guard lock(ni.routers_lock);

        if (is_list_empty(ni.routers))
            return NULL_MAC_ADDRESS;

        auto& r = containing_record<ipv6_router>(ni.routers.next);

        return r.mac;
    }
}

static void send_arp_request(netif& ni, const ipv4_address& dst) {
    optional<ipv4_address> src_ip;

    {
        lock_guard lock(ni.addr_lock);

        for (auto le = ni.addrs.next; le != &ni.addrs; le = le->next) {
            auto& a = containing_record<ip_addr>(le);

            auto* addr = get_if<ipv4_address>(&a.addr);

            if (addr) {
                src_ip = *addr;
                break;
            }
        }
    }

    if (!src_ip.has_value())
        return;

    auto pack = new packet;
    if (!pack) {
        print("new failed\n");
        return;
    }

    auto& arp = *(arp_header*)pack->data;
    pack->size = sizeof(arp);

    memset(&arp, 0, sizeof(arp));

    arp.htype = byteswap((uint16_t)1); // Ethernet
    arp.ptype = (uint16_t)ethertype::ipv4;
    arp.hlen = sizeof(mac_address);
    arp.plen = sizeof(ipv4_address);
    arp.opcode = (uint16_t)arp_opcode::request;
    *(mac_address*)arp.sender_mac_address = ni.mac;
    arp.sender_ip_address = *src_ip;
    arp.target_ip_address = dst;

    auto dst_mac = BROADCAST_MAC_ADDRESS;

    auto status = ni.send_packet(dst_mac, ethertype::arp, *pack);
    if (!success(status))
        print("send_packet returned {}\n", status);

    pack->release();
}

mac_address search_arp_cache(netif& ni, const ipv4_address& dst) {
    neighbour_cache_entry<ipv4>* nce = nullptr;

    {
        lock_guard lock(ni.arp_lock);

        for (auto le = ni.arp_cache.next; le != &ni.arp_cache; le = le->next) {
            auto& c = containing_record<neighbour_cache_entry<ipv4>>(le);

            if (c.addr == dst) {
                if (c.status == neighbour_cache_status::reachable)
                    return c.mac;

                c.acquire();
                nce = &c;
                break;
            }
        }
    }

    if (nce) { // found, but incomplete
        wait_for_object(nce->event.get(), false);

        if (nce->status == neighbour_cache_status::reachable) {
            auto ret = nce->mac;

            nce->release();
            return ret;
        } else
            nce->release();
    }

    nce = new neighbour_cache_entry<ipv4>(ni);
    if (!nce) {
        print("new failed\n");
        return NULL_MAC_ADDRESS;
    }

    neighbour_cache_entry<ipv4>* nce2 = nullptr;

    nce->addr = dst;

    nce->event = create_event(false);
    if (!nce->event) {
        print("create_event failed\n");
        nce->release();
        return NULL_MAC_ADDRESS;
    }

    nce->status = neighbour_cache_status::incomplete;
    nce->last_update_time = kernel_time_page.system_time;

    {
        lock_guard lock(ni.arp_lock);

        for (auto le = ni.arp_cache.next; le != &ni.arp_cache; le = le->next) {
            auto& c = containing_record<neighbour_cache_entry<ipv4>>(le);

            if (c.addr == dst) {
                c.acquire();
                nce2 = &c;
                break;
            }
        }

        if (!nce2) {
            insert_tail_list(ni.arp_cache, nce->le);
            nce->acquire();
        }
    }

    if (nce2) { // race and we lost
        nce->release();

        wait_for_object(nce2->event.get(), false);

        if (nce2->status == neighbour_cache_status::reachable) {
            auto ret = nce2->mac;

            nce2->release();
            return ret;
        } else {
            nce2->release();
            return NULL_MAC_ADDRESS;
        }
    }

    for (unsigned int i = 0; i < 3; i++) {
        uint64_t timeout = SYSTEM_TIME_RESOLUTION; // 1 second

        send_arp_request(ni, dst);

        auto status = wait_for_object(nce->event.get(), false, timeout);

        if (status == nt_status::TIMEOUT)
            continue;
        else if (!success(status)) {
            set_event(static_cast<event&>(nce->event.get()));

            {
                lock_guard lock(ni.arp_lock);

                if (nce->status != neighbour_cache_status::reachable) {
                    remove_entry_list(nce->le);
                    nce->release();
                }
            }

            nce->release();

            print("wait_for_object returned {}\n", status);
            return NULL_MAC_ADDRESS;
        }

        if (nce->status == neighbour_cache_status::reachable) {
            auto ret = nce->mac;

            nce->release();
            return ret;
        }
    }

    set_event(static_cast<event&>(nce->event.get()));

    auto ret = NULL_MAC_ADDRESS;

    {
        lock_guard lock(ni.arp_lock);

        if (nce->status == neighbour_cache_status::reachable)
            ret = nce->mac;
        else {
            remove_entry_list(nce->le);
            nce->release();
        }
    }

    nce->release();

    return ret;
}

bool is_broadcast_address(netif& ni, const ipv4_address& dst) {
    if (dst == parse_ipv4_address("255.255.255.255"))
        return true;

    {
        lock_guard lock(ni.addr_lock);

        for (auto le = ni.addrs.next; le != &ni.addrs; le = le->next) {
            auto& a = containing_record<ip_addr>(le);

            auto* addr = get_if<ipv4_address>(&a.addr);

            if (!addr)
                continue;

            if (dst == calculate_broadcast_address(*addr, a.prefix))
                return true;
        }
    }

    return false;
}

mac_address find_mac_address(netif& ni, const ipv4_address& dst) {
    if (is_broadcast_address(ni, dst))
        return BROADCAST_MAC_ADDRESS;

    if (is_multicast_address(dst)) {
        mac_address ret;

        ret.v[0] = 0x01;
        ret.v[1] = 0x00;
        ret.v[2] = 0x5e;
        ret.v[3] = dst.v[1] & 0x7f;
        ret.v[4] = dst.v[2];
        ret.v[5] = dst.v[3];

        return ret;
    }

    return search_arp_cache(ni, dst);
}

void maybe_add_to_neighbour_cache(netif& ni, const ipv6_address& dst,
                                  const mac_address& mac) {
    bool on_link = false;

    {
        lock_guard lock(ni.prefix_lock);

        for (auto le = ni.prefixes.next; le != &ni.prefixes; le = le->next) {
            auto& p = containing_record<ipv6_prefix>(le);

            if (ipv6_prefix_match(dst, p.prefix, p.prefix_length)) {
                on_link = p.on_link;
                break;
            }
        }
    }

    if (!on_link)
        return;

    {
        lock_guard lock(ni.neighbour_lock);

        for (auto le = ni.neighbour_cache.next; le != &ni.neighbour_cache; le = le->next) {
            auto& c = containing_record<neighbour_cache_entry<ipv6>>(le);

            if (c.addr == dst) {
                if (c.status != neighbour_cache_status::reachable) {
                    c.mac = mac;
                    set_event(static_cast<event&>(c.event.get()));
                }
                return;
            }
        }
    }

    auto nce = new neighbour_cache_entry<ipv6>(ni);
    if (!nce) {
        print("new failed\n");
        return;
    }

    nce->addr = dst;
    nce->mac = mac;

    nce->event = create_event(false);
    if (!nce->event) {
        print("create_event failed\n");
        nce->release();
        return;
    }

    set_event(static_cast<event&>(nce->event.get()));

    nce->status = neighbour_cache_status::reachable;

    bool found = false;

    {
        lock_guard lock(ni.neighbour_lock);

        for (auto le = ni.neighbour_cache.next; le != &ni.neighbour_cache; le = le->next) {
            auto& c = containing_record<neighbour_cache_entry<ipv6>>(le);

            if (c.addr == dst) {
                if (c.status != neighbour_cache_status::reachable) {
                    c.mac = mac;
                    set_event(static_cast<event&>(c.event.get()));
                }

                found = true;
                break;
            }
        }

        if (!found) {
            insert_tail_list(ni.neighbour_cache, nce->le);
            nce->acquire();
        }
    }

    nce->release();
}

void send_address_unreachable(netif& ni, const packet& pack,
                              const ipv4_address& addr) {
    static const size_t MIN_MTU = 576;

    auto pack2 = new packet;
    if (!pack2) {
        print("new failed\n");
        return;
    }

    size_t payload_size = min(pack.size, MIN_MTU - sizeof(ipv4_header) - sizeof(icmpv4_error));

    pack2->size = sizeof(ipv4_header) + offsetof(icmpv4_error, header) + payload_size;

    auto& ip = *(ipv4_header*)pack2->data;
    ip.ihl = sizeof(ipv4_header) / sizeof(uint32_t);
    ip.version = 4;
    ip.ecn = 0;
    ip.dscp = 0;
    ip.total_length = byteswap((uint16_t)(sizeof(ipv4_header) + offsetof(icmpv4_error, header) + payload_size));
    ip.identification = 0;
    ip.fragment_offset1 = 0;
    ip.MF = 0;
    ip.DF = 1;
    ip.R = 0;
    ip.fragment_offset2 = 0;
    ip.ttl = DEFAULT_HOP_LIMIT;
    ip.protocol = IPPROTO_ICMP;
    ip.header_checksum = 0;
    ip.src = addr;
    ip.dst = addr;

    auto& err = *(icmpv4_error*)(pack2->data + sizeof(ipv4_header));

    err.type = icmpv4_type::dest_unreachable;
    err.code = (uint8_t)icmpv4_dest_unreachable_code::dest_host_unreachable;
    err.checksum = 0;
    err.param = 0;

    memcpy(&err.header, pack.data, payload_size);

    err.checksum = calc_ipv4_checksum(span((uint8_t*)&err, offsetof(icmpv4_error, header) + payload_size));

    handle_ipv4_packet(ni, *pack2, ni.mac, true);

    pack2->release();
}

void send_address_unreachable(netif& ni, const packet& pack,
                              const ipv6_address& addr) {
    static const size_t MIN_MTU = 1280;

    auto pack2 = new packet;
    if (!pack2) {
        print("new failed\n");
        return;
    }

    size_t payload_size = min(pack.size, MIN_MTU - sizeof(ipv6_header) - sizeof(icmpv6_error));

    pack2->size = sizeof(ipv6_header) + offsetof(icmpv6_error, header) + payload_size;

    auto& ip = *(ipv6_header*)pack2->data;
    ip.traffic_class1 = 0;
    ip.version = 6;
    ip.traffic_class2 = 0;
    ip.flow_label_high = 0;
    ip.flow_label_medium = 0;
    ip.flow_label_low = 0;
    ip.payload_length = byteswap((uint16_t)(offsetof(icmpv6_error, header) + payload_size));
    ip.next_header = IPPROTO_ICMPV6;
    ip.hop_limit = DEFAULT_HOP_LIMIT;
    ip.src = addr;
    ip.dst = addr;

    auto& err = *(icmpv6_error*)(pack2->data + sizeof(ipv6_header));

    err.type = icmpv6_type::dest_unreachable;
    err.code = (uint8_t)icmpv6_dest_unreachable_code::address_unreachable;
    err.checksum = 0;
    err.reserved = 0;

    memcpy(&err.header, pack.data, payload_size);

    err.checksum = calc_ipv6_checksum(ip, span((uint8_t*)&err, offsetof(icmpv6_error, header) + payload_size), IPPROTO_ICMPV6);

    handle_ipv6_packet(ni, *pack2, ni.mac, true);

    pack2->release();
}

template<socket_family FAMILY>
nt_status dispatch_packet(netif& ni, packet& pack, const mac_address& dst_mac) {
    nt_status status;

    auto& ip = *(typename FAMILY::header*)pack.data;

    if constexpr (is_same_v<FAMILY, ipv4>) {
        if (ip.src == ip.dst || ni.loopback) {
            lo_packets.fetch_add(1, memory_order::relaxed);
            lo_bytes.fetch_add(pack.size, memory_order::relaxed);

            handle_ipv4_packet(ni, pack, ni.mac, true);
        } else {
            if (is_broadcast_address(ni, ip.dst) || (is_multicast_address(ip.dst) && should_accept_multicast(ip.dst))) {
                lo_packets.fetch_add(1, memory_order::relaxed);
                lo_bytes.fetch_add(pack.size, memory_order::relaxed);

                handle_ipv4_packet(ni, pack, ni.mac, true); // send copy to ourselves
            }

            status = ni.send_packet(dst_mac, ethertype::ipv4, pack);
            if (!success(status)) {
                print("netif::send_packet returned {}\n", status);
                return status;
            }
        }
    } else if constexpr (is_same_v<FAMILY, ipv6>) {
        if (ip.src == ip.dst || ni.loopback) {
            lo_packets.fetch_add(1, memory_order::relaxed);
            lo_bytes.fetch_add(pack.size, memory_order::relaxed);

            handle_ipv6_packet(ni, pack, ni.mac, true);
        } else {
            if (is_multicast_address(ip.dst) && should_accept_multicast(ni, ip.dst)) {
                lo_packets.fetch_add(1, memory_order::relaxed);
                lo_bytes.fetch_add(pack.size, memory_order::relaxed);

                handle_ipv6_packet(ni, pack, ni.mac, true); // send copy to ourselves
            }

            status = ni.send_packet(dst_mac, ethertype::ipv6, pack);
            if (!success(status)) {
                print("netif::send_packet returned {}\n", status);
                return status;
            }
        }
    }

    return nt_status::SUCCESS;
}

template<socket_family FAMILY>
template<bool NONBLOCK>
nt_status raw_socket<FAMILY>::do_sendto(span<const span<const uint8_t>> v,
                                        int flags, sockaddr* to, size_t tolen,
                                        uint64_t& sent_bytes) {
    nt_status status;
    typename FAMILY::address src;
    auto vec_size = get_vec_size(v);
    span<const uint8_t> top = vec_size == 0 ? span<const uint8_t>() : v.front();

    if (!to || tolen < sizeof(typename FAMILY::sockaddr))
        return nt_status::INVALID_PARAMETER;

    if (flags != 0)
        return nt_status::INVALID_PARAMETER;

    auto& toin = *(typename FAMILY::sockaddr*)to;

    {
        lock_guard lock(bind_lock);

        if (bind_done)
            src = bound_address;
        else {
            if constexpr (is_same_v<FAMILY, ipv4>)
                src = parse_ipv4_address("0.0.0.0");
            else if constexpr (is_same_v<FAMILY, ipv6>)
                src = parse_ipv6_address("::");
        }
    }

    typename FAMILY::address dst, gateway;
    netif* pni;

    if constexpr (is_same_v<FAMILY, ipv4>)
        dst = *(typename FAMILY::address*)&toin.sin_addr;
    else if constexpr (is_same_v<FAMILY, ipv6>)
        dst = *(typename FAMILY::address*)&toin.sin6_addr;

    if constexpr (is_same_v<FAMILY, ipv4>) {
        status = ipv4_choose_interface(dst, pni, src, gateway);
        if (!success(status))
            return status;
    } else if constexpr (is_same_v<FAMILY, ipv6>) {
        status = ipv6_choose_interface(dst, toin.sin6_scope_id, pni, src);
        if (!success(status))
            return status;
    }

    auto& ni = *pni;

    if (vec_size + sizeof(typename FAMILY::header) > ni.mtu) {
        ni.release();
        return nt_status::INVALID_BUFFER_SIZE;
    }

    mac_address mac;

    if (src == dst)
        mac = ni.mac;
    else {
        if (ni.loopback)
            mac = NULL_MAC_ADDRESS;
        else {
            if constexpr (is_same_v<FAMILY, ipv4>)
                mac = find_mac_address(ni, gateway == parse_ipv4_address("0.0.0.0") ? dst : gateway);
            else
                mac = find_mac_address(ni, dst);
        }
    }

    auto pack = new packet;
    if (!pack) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    pack->size = sizeof(typename FAMILY::header) + vec_size;

    auto& ip = *(typename FAMILY::header*)pack->data;

    if constexpr (is_same_v<FAMILY, ipv4>) {
        ip.ihl = sizeof(ipv4_header) / sizeof(uint32_t);
        ip.version = 4;
        ip.ecn = 0;
        ip.dscp = 0;
        ip.total_length = byteswap((uint16_t)(sizeof(ipv4_header) + vec_size));
        ip.identification = 0;
        ip.fragment_offset1 = 0;
        ip.MF = 0;
        ip.DF = 1;
        ip.R = 0;
        ip.fragment_offset2 = 0;
        ip.ttl = DEFAULT_HOP_LIMIT;
        ip.protocol = (uint8_t)protocol;
        ip.header_checksum = 0;
        ip.src = src;
        ip.dst = dst;

        ip.header_checksum = calc_ipv4_checksum(span(pack->data, sizeof(ipv4_header)));
    } else if constexpr (is_same_v<FAMILY, ipv6>) {
        ip.traffic_class1 = 0;
        ip.version = 6;
        ip.traffic_class2 = 0;
        ip.flow_label_high = 0;
        ip.flow_label_medium = 0;
        ip.flow_label_low = 0;
        ip.payload_length = byteswap((uint16_t)vec_size);
        ip.next_header = (uint8_t)protocol;
        ip.hop_limit = DEFAULT_HOP_LIMIT;
        ip.src = src;
        ip.dst = dst;
    }

    memcpy_vec(pack->data + sizeof(typename FAMILY::header), v, top, vec_size);

    if constexpr (is_same_v<FAMILY, ipv6>) {
        // FIXME - should only be if IPV6_CHECKSUM sockopt set
        if (protocol == IPPROTO_ICMPV6 && vec_size >= sizeof(typename FAMILY::icmp_header)) {
            auto& icmp = *(typename FAMILY::icmp_header*)(pack->data + sizeof(typename FAMILY::header));

            icmp.checksum = 0;
            icmp.checksum = calc_ipv6_checksum(ip, span((uint8_t*)&icmp, vec_size), IPPROTO_ICMPV6);
        }
    }

    if (mac == NULL_MAC_ADDRESS && !ni.loopback) {
        send_address_unreachable(ni, *pack, src);

        pack->release();
        ni.release();
        return nt_status::HOST_UNREACHABLE;
    }

    status = dispatch_packet<FAMILY>(ni, *pack, mac);
    if (!success(status)) {
        pack->release();
        ni.release();
        return status;
    }

    pack->release();
    ni.release();

    sent_bytes = vec_size;

    return status;
}

netif::~netif() {
    // not routes or neighbour cache, as they hold reference to netif

    while (true) {
        ip_addr* a = nullptr;

        {
            lock_guard lock(addr_lock);

            if (!is_list_empty(addrs)) {
                a = &containing_record<ip_addr>(addrs.next);
                remove_entry_list(a->le);
            }
        }

        if (!a)
            break;

        delete a;
    }

    while (true) {
        ipv6_prefix* pref = nullptr;

        {
            lock_guard lock(prefix_lock);

            if (!is_list_empty(prefixes)) {
                pref = &containing_record<ipv6_prefix>(prefixes.next);
                remove_entry_list(pref->le);
            }
        }

        if (!pref)
            break;

        delete pref;
    }

    while (true) {
        ipv6_router* r = nullptr;

        {
            lock_guard lock(routers_lock);

            if (!is_list_empty(routers)) {
                r = &containing_record<ipv6_router>(routers.next);
                remove_entry_list(r->le);
            }
        }

        if (!r)
            break;

        delete r;
    }
}

nt_status loopback_netif::send_packet(const mac_address&, ethertype, packet&) {
    // can happen by doing e.g. "ping ff02::1%lo"

    // silently drop

    return nt_status::SUCCESS;
}

bool is_our_ip(const ipv4_address& ip) {
    if (ip.v[0] == 127) // 127.0.0.0/8 loopback
        return true;

    lock_guard lock(netif_lock);

    for (auto le = netif_list.next; le != &netif_list; le = le->next) {
        auto& ni = containing_record<netif>(le);

        {
            lock_guard lock2(ni.addr_lock);

            for (auto le2 = ni.addrs.next; le2 != &ni.addrs; le2 = le2->next) {
                auto& a = containing_record<ip_addr>(le2);

                auto* addr = get_if<ipv4_address>(&a.addr);

                if (!addr)
                    continue;

                if (*addr == ip)
                    return true;
            }
        }
    }

    return false;
}

bool is_our_ip(const ipv6_address& ip) {
    lock_guard lock(netif_lock);

    for (auto le = netif_list.next; le != &netif_list; le = le->next) {
        auto& ni = containing_record<netif>(le);

        {
            lock_guard lock2(ni.addr_lock);

            for (auto le2 = ni.addrs.next; le2 != &ni.addrs; le2 = le2->next) {
                auto& a = containing_record<ip_addr>(le2);

                auto* addr = get_if<ipv6_address>(&a.addr);

                if (!addr)
                    continue;

                if (*addr == ip)
                    return true;
            }
        }
    }

    return false;
}

template<socket_family FAMILY>
nt_status raw_socket<FAMILY>::bind(const sockaddr* addr, size_t addrlen) {
    if (addr->sa_family != FAMILY::family)
        return nt_status::INVALID_PARAMETER;

    if (addrlen < sizeof(typename FAMILY::sockaddr))
        return nt_status::BUFFER_TOO_SMALL;

    auto& b = *(typename FAMILY::sockaddr*)addr;

    typename FAMILY::address ip;

    if constexpr (is_same_v<FAMILY, ipv4>)
        ip = *(typename FAMILY::address*)&b.sin_addr;
    else if constexpr (is_same_v<FAMILY, ipv6>)
        ip = *(typename FAMILY::address*)&b.sin6_addr;

    if constexpr (is_same_v<FAMILY, ipv4>) {
        if (ip != parse_ipv4_address("0.0.0.0")) {
            if (!is_our_ip(ip))
                return nt_status::INVALID_ADDRESS_COMPONENT;
        }
    } else if constexpr (is_same_v<FAMILY, ipv6>) {
        if (ip != parse_ipv6_address("::")) {
            if (!is_our_ip(ip))
                return nt_status::INVALID_ADDRESS_COMPONENT;
        }
    }

    {
        lock_guard lock(bind_lock);

        if (bind_done)
            return nt_status::INVALID_PARAMETER;

        bound_address = ip;

        bind_done = true;
    }

    return nt_status::SUCCESS;
}

template<socket_family FAMILY>
nt_status raw_socket<FAMILY>::getsockname(span<uint8_t> buf, size_t& len) {
    if (buf.size() < sizeof(typename FAMILY::sockaddr))
        return nt_status::BUFFER_TOO_SMALL;

    auto& addr = *(typename FAMILY::sockaddr*)buf.data();

    memset(&addr, 0, sizeof(addr));

    if constexpr (is_same_v<FAMILY, ipv4>)
        addr.sin_family = FAMILY::family;
    else if constexpr (is_same_v<FAMILY, ipv6>)
        addr.sin6_family = FAMILY::family;

    {
        lock_guard lock(bind_lock);

        if (bind_done) {
            if constexpr (is_same_v<FAMILY, ipv4>)
                *(typename FAMILY::address*)&addr.sin_addr = bound_address;
            else if constexpr (is_same_v<FAMILY, ipv6>)
                *(typename FAMILY::address*)&addr.sin6_addr = bound_address;
        }
    }

    len = sizeof(typename FAMILY::sockaddr);

    return nt_status::SUCCESS;
}

template<socket_family FAMILY>
void raw_socket<FAMILY>::last_handle_closed() {
    dead = true;

    {
        sync_lock sl;
        lock_guard lock(sl);

        if (!signalled) {
            wake_up_waiters(waiters, WAIT_TYPE_READING);
            signalled = true;
        }
    }
}

ipv6_address find_ll_address(const mac_address& m) {
    ipv6_address ret;

    ret.v[0] = 0xfe;
    ret.v[1] = 0x80;
    ret.v[2] = 0x00;
    ret.v[3] = 0x00;
    ret.v[4] = 0x00;
    ret.v[5] = 0x00;
    ret.v[6] = 0x00;
    ret.v[7] = 0x00;
    ret.v[8] = m.v[0] ^ 0x02;
    ret.v[9] = m.v[1];
    ret.v[10] = m.v[2];
    ret.v[11] = 0xff;
    ret.v[12] = 0xfe;
    ret.v[13] = m.v[3];
    ret.v[14] = m.v[4];
    ret.v[15] = m.v[5];

    return ret;
}

static void send_arp_reply(netif& ni, const ipv4_address& src_ip,
                           const ipv4_address& dst_ip,
                           const mac_address& dst_mac) {
    auto pack = new packet;
    if (!pack) {
        print("new failed\n");
        return;
    }

    pack->size = sizeof(arp_header);
    auto& arp = *(arp_header*)pack->data;

    memset(&arp, 0, sizeof(arp));

    arp.htype = byteswap((uint16_t)1); // Ethernet
    arp.ptype = (uint16_t)ethertype::ipv4;
    arp.hlen = sizeof(mac_address);
    arp.plen = sizeof(ipv4_address);
    arp.opcode = (uint16_t)arp_opcode::reply;
    *(mac_address*)arp.sender_mac_address = ni.mac;
    arp.sender_ip_address = src_ip;
    *(mac_address*)arp.target_mac_address = dst_mac;
    arp.target_ip_address = dst_ip;

    auto status = ni.send_packet(dst_mac, ethertype::arp, *pack);
    if (!success(status))
        print("send_packet returned {}\n", status);

    pack->release();
}

static void add_arp_cache_entry(netif& ni, const mac_address& mac,
                                const ipv4_address& addr, bool to_us) {
    if (mac == NULL_MAC_ADDRESS)
        return;

    {
        lock_guard lock(ni.arp_lock);

        for (auto le = ni.arp_cache.next; le != &ni.arp_cache; le = le->next) {
            auto& c = containing_record<neighbour_cache_entry<ipv4>>(le);

            if (c.addr != addr)
                continue;

            c.mac = mac;

            if (c.status == neighbour_cache_status::incomplete) {
                c.status = neighbour_cache_status::reachable;
                set_event(static_cast<event&>(c.event.get()));
            }

            c.last_update_time = kernel_time_page.system_time;

            return;
        }
    }

    if (!to_us)
        return;

    // add new entry

    auto nce = new neighbour_cache_entry<ipv4>(ni);
    if (!nce) {
        print("new failed\n");
        return;
    }

    nce->addr = addr;
    nce->mac = mac;

    nce->event = create_event(false);
    if (!nce->event) {
        print("create_event failed\n");
        nce->release();
        return;
    }

    set_event(static_cast<event&>(nce->event.get()));

    nce->status = neighbour_cache_status::reachable;

    bool beaten_to_it = false;

    {
        lock_guard lock(ni.arp_lock);

        for (auto le = ni.arp_cache.next; le != &ni.arp_cache; le = le->next) {
            auto& c = containing_record<neighbour_cache_entry<ipv4>>(le);

            if (c.addr == addr) {
                beaten_to_it = true;

                c.mac = mac;

                if (c.status == neighbour_cache_status::incomplete) {
                    c.status = neighbour_cache_status::reachable;
                    set_event(static_cast<event&>(c.event.get()));
                }

                c.last_update_time = kernel_time_page.system_time;

                break;
            }
        }

        if (!beaten_to_it) {
            insert_tail_list(ni.arp_cache, nce->le);
            nce->last_update_time = kernel_time_page.system_time;
        }
    }

    if (beaten_to_it)
        nce->release();
}

static void handle_arp_reply(netif& ni, packet& pack) {
    if (pack.size < sizeof(arp_header)) {
        print("handle_arp_reply: buffer length was {}, expected at least {}\n",
              pack.size, sizeof(arp_header));
        return;
    }

    auto& arp = *(arp_header*)pack.data;

    add_arp_cache_entry(ni, *(mac_address*)&arp.sender_mac_address,
                        arp.sender_ip_address, *(mac_address*)&arp.target_mac_address == ni.mac);
}

static void handle_arp_packet(netif& ni, packet& pack) {
    if (pack.size < sizeof(arp_header))
        return;

    const auto& arp = *(arp_header*)pack.data;

    if (arp.hlen != sizeof(mac_address) || arp.plen != sizeof(ipv4_address))
        return;

    if (byteswap(arp.htype) != 1) // not Ethernet
        return;

    if ((ethertype)arp.ptype != ethertype::ipv4)
        return;

#if 0
    print("arp: htype = {}, ptype = {}, hlen = {}, plen = {}, opcode = {}, sender_mac_address = {}, sender_ip_address = {}, target_mac_address = {}, target_ip_address = {}\n",
          byteswap(arp.htype), (ethertype)arp.ptype, arp.hlen, arp.plen, byteswap(arp.opcode),
          *(mac_address*)arp.sender_mac_address, arp.sender_ip_address,
          *(mac_address*)arp.target_mac_address, arp.target_ip_address);
#endif

    switch ((arp_opcode)arp.opcode) {
        using enum arp_opcode;

        case request: {
            bool found = false;

            {
                lock_guard lock(ni.addr_lock);

                for (auto le = ni.addrs.next; le != &ni.addrs; le = le->next) {
                    auto& a = containing_record<ip_addr>(le);

                    auto* addr = get_if<ipv4_address>(&a.addr);

                    if (addr && *addr == arp.target_ip_address) {
                        found = true;
                        break;
                    }
                }
            }

            if (!found)
                return;

            add_arp_cache_entry(ni, *(mac_address*)&arp.sender_mac_address,
                                arp.sender_ip_address, true);

            send_arp_reply(ni, arp.target_ip_address, arp.sender_ip_address,
                        *(mac_address*)arp.sender_mac_address);

            break;
        }

        case reply:
            handle_arp_reply(ni, pack);
            break;
    }
}

void received_packet(netif& ni, packet& pack, const mac_address& src_mac,
                     ethertype type, bool skip_csumming) {
    switch (type) {
        using enum ethertype;

        case ipv4:
            handle_ipv4_packet(ni, pack, src_mac, skip_csumming);
            break;

        case ipv6:
            handle_ipv6_packet(ni, pack, src_mac, skip_csumming);
            break;

        case arp:
            handle_arp_packet(ni, pack);
            break;
    }
}
