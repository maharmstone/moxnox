// SPDX-License-Identifier: GPL-2.0-only
module;

#include <utility>
#include <mutex>
#include <algorithm>
#include <span>
#include <bit>
#include <atomic>
#include <variant>
#include <optional>

module net;

import print;
import object_ref;
import thread;
import scheduler;
import kernel_time_page;
import config;
import vec;

using namespace std;

class base_udp_socket : public socket {
public:
    base_udp_socket(bool is_ipv4) : is_ipv4(is_ipv4) { }

    bool is_ipv4;
    spinlock buffer_lock;
    list_entry buffer = { &buffer, &buffer };
    size_t buffer_size = 0;
    spinlock bind_lock;
    bool bind_done = false, connect_done = false, autobound = false;
    uint16_t bound_port, connected_port;
    uint32_t connected_scope = 0;
    nt_status pending_error = nt_status::SUCCESS;
    atomic<bool> dead = false, shutdown_read = false, shutdown_write = false;
    bool ipv6_only = false;
};

template<socket_family FAMILY>
class udp_socket : public base_udp_socket {
public:
    udp_socket() : base_udp_socket(is_same_v<FAMILY, ipv4> ? true : false) { }
    ~udp_socket() override;

    void last_handle_closed() override;

    nt_status recvfrom(span<const span<uint8_t>> v, int flags,
                       sockaddr_storage* from, size_t* fromlen,
                       uint64_t& recv_bytes, span<HANDLE>, size_t&) override {
        return do_recvfrom<false>(v, flags, from, fromlen, recv_bytes);
    }

    nt_status sendto(span<const span<const uint8_t>> v, int flags, sockaddr* to,
                     size_t tolen, uint64_t& sent_bytes,
                     span<const HANDLE> handles) override;

    nt_status bind(const sockaddr* addr, size_t addrlen) override;
    nt_status connect(const sockaddr* addr, size_t addrlen) override;

    nt_status listen(int) override {
        return nt_status::INVALID_PARAMETER;
    }

    nt_status accept(HANDLE&, void*, int&) override {
        return nt_status::INVALID_PARAMETER;
    }

    nt_status getsockname(span<uint8_t> buf, size_t& len) override;
    nt_status getpeername(span<uint8_t> buf, size_t& len) override;

    nt_status recvfrom_nonblock(span<const span<uint8_t>> v, int flags,
                                sockaddr_storage* from, size_t* fromlen,
                                uint64_t& recv_bytes, span<HANDLE>, size_t&) override {
        return do_recvfrom<true>(v, flags, from, fromlen, recv_bytes);
    }

    nt_status sendto_nonblock(span<const span<const uint8_t>> v, int flags,
                              sockaddr* to, size_t tolen,
                              uint64_t& sent_bytes, span<const HANDLE> handles) override;
    nt_status shutdown(int how) override;
    nt_status getsockopt_ipv6(sockopt_ipv6 opt, span<uint8_t> buf,
                              socklen_t& retlen) override;
    nt_status setsockopt_ipv6(sockopt_ipv6 opt, span<const uint8_t> buf) override;

    typename FAMILY::address bound_address, connected_address;

private:
    template<bool NONBLOCK>
    nt_status do_recvfrom(span<const span<uint8_t>> v, int flags,
                          sockaddr_storage* from, size_t* fromlen,
                          uint64_t& recv_bytes);
};

static spinlock udp_ports_lock;
static base_udp_socket* udp_ports_v4[65536];
static udp_socket<ipv6>* udp_ports_v6[65536];

static const size_t MAX_BUFFER_SIZE = 1048576;

static void send_port_unreachable_v4(netif& ni, const packet& pack,
                                     const mac_address& mac) {
    static const size_t MIN_MTU = 576;

    const auto& orig_ip = *(ipv4_header*)pack.data;

    auto pack2 = new packet;
    if (!pack2) {
        print("new failed\n");
        return;
    }

    size_t payload_size = min((size_t)byteswap((uint16_t)orig_ip.total_length),
                              MIN_MTU - sizeof(ipv4_header) - sizeof(icmpv4_error));

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
    ip.src = orig_ip.dst; // FIXME - make sure not multicast?
    ip.dst = orig_ip.src;

    ip.header_checksum = calc_ipv4_checksum(span(pack2->data, sizeof(ipv4_header)));

    auto& err = *(icmpv4_error*)(pack2->data + sizeof(ipv4_header));

    err.type = icmpv4_type::dest_unreachable;
    err.code = (uint8_t)icmpv4_dest_unreachable_code::dest_port_unreachable;
    err.checksum = 0;
    err.param = 0;

    memcpy(&err.header, pack.data, payload_size);

    err.checksum = calc_ipv4_checksum(span((uint8_t*)&err, offsetof(icmpv4_error, header) + payload_size));

    auto status = ni.send_packet(mac, ethertype::ipv4, *pack2);
    if (!success(status))
        print("netif::send_packet returned {}\n", status);

    pack2->release();
}

static void send_port_unreachable_v6(netif& ni, const packet& pack,
                                     const mac_address& mac) {
    static const size_t MIN_MTU = 1280;

    const auto& orig_ip = *(ipv6_header*)pack.data;

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
    ip.src = orig_ip.dst; // FIXME - make sure not multicast?
    ip.dst = orig_ip.src;

    auto& err = *(icmpv6_error*)(pack2->data + sizeof(ipv6_header));

    err.type = icmpv6_type::dest_unreachable;
    err.code = (uint8_t)icmpv6_dest_unreachable_code::no_port;
    err.checksum = 0;
    err.reserved = 0;

    memcpy(&err.header, pack.data, payload_size);

    err.checksum = calc_ipv6_checksum(ip, span((uint8_t*)&err, offsetof(icmpv6_error, header) + payload_size), IPPROTO_ICMPV6);

    auto status = ni.send_packet(mac, ethertype::ipv6, *pack2);
    if (!success(status))
        print("netif::send_packet returned {}\n", status);

    pack2->release();
}

void received_ipv4_udp_packet(netif& ni, packet& pack,
                              const mac_address& src_mac, bool skip_checksum) {
    if (pack.size < sizeof(ipv4_header))
        return;

    auto& ip = *(ipv4_header*)pack.data;
    auto ip_header_size = ip.ihl * sizeof(uint32_t);

    if (ip_header_size < sizeof(ipv4_header))
        return;

    if (pack.size < ip_header_size + sizeof(udp_header))
        return;

    if (is_multicast_address(ip.dst))
        return;

    auto& udp = *(udp_header*)(pack.data + ip_header_size);

    if (byteswap(udp.length) < sizeof(udp_header))
        return;

    if (pack.size < ip_header_size + byteswap(udp.length))
        return;

    if (!skip_checksum && udp.checksum != 0) {
        auto checksum = calc_ipv4_checksum(ip, span((uint8_t*)&udp, min((size_t)byteswap(udp.length), pack.size - ip_header_size)), IPPROTO_UDP);

        if (checksum != 0 && checksum != 0xffff)
            return;
    }

    if (udp.srcport == DHCP_SERVER_PORT && udp.dstport == DHCP_CLIENT_PORT)
        received_dhcp_packet(ni, pack);

#if 0
    print("received_udp_packet (src {}, dst {}, {}, {:04x})\n",
          byteswap(udp.srcport), byteswap(udp.dstport), byteswap(udp.length),
          byteswap(udp.checksum));
#endif

    base_udp_socket* sock = nullptr;

    {
        lock_guard lock(udp_ports_lock);

        if (udp_ports_v4[udp.dstport]) {
            sock = udp_ports_v4[udp.dstport];

            if (sock->is_ipv4) {
                auto& s2 = static_cast<udp_socket<ipv4>&>(*sock);

                {
                    lock_guard lock(sock->bind_lock);

                    if (sock->bind_done && s2.bound_address != parse_ipv4_address("0.0.0.0") && s2.bound_address != ip.dst)
                        sock = nullptr;
                    else
                        sock->acquire();
                }
            } else {
                auto& s2 = static_cast<udp_socket<ipv6>&>(*sock);

                {
                    lock_guard lock(sock->bind_lock);

                    if (!sock->bind_done || (s2.bound_address != parse_ipv6_address("::") && s2.bound_address != map_ipv4_address_to_ipv6(ip.dst)))
                        sock = nullptr;
                    else
                        sock->acquire();
                }
            }
        }
    }

    if (!sock) {
        if (!is_broadcast_address(ni, ip.dst))
            send_port_unreachable_v4(ni, pack, src_mac);

        return;
    }

    {
        bool ignore = false;

        {
            lock_guard lock(sock->bind_lock);

            if (sock->connect_done) {
                if (sock->is_ipv4) {
                    auto& s2 = static_cast<udp_socket<ipv4>&>(*sock);

                    if (s2.connected_address != ip.src || sock->connected_port != udp.srcport)
                        ignore = true;
                } else {
                    auto& s2 = static_cast<udp_socket<ipv6>&>(*sock);

                    if (s2.connected_address != map_ipv4_address_to_ipv6(ip.src) || sock->connected_port != udp.srcport)
                        ignore = true;
                }
            }
        }

        if (ignore) {
            sock->release();
            return;
        }
    }

    auto p = new socket_packet(pack);
    if (!p) {
        print("new failed\n");
        sock->release();
        return;
    }

    p->scope_id = ni.num;

    bool done = false;

    {
        sync_lock sl;
        lock_guard lock(sl);
        lock_guard lock2(sock->buffer_lock);

        if (sock->buffer_size + pack.size <= MAX_BUFFER_SIZE) {
            insert_tail_list(sock->buffer, p->le);
            sock->buffer_size += pack.size;
            done = true;
        }

        if (!sock->signalled) {
            wake_up_waiters(sock->waiters, WAIT_TYPE_READING);
            sock->signalled = true;
        }
    }

    if (!done)
        delete p;

    sock->release();
}

void received_ipv6_udp_packet(netif& ni, packet& pack,
                              const mac_address& src_mac, bool skip_checksum) {
    if (pack.size < sizeof(ipv6_header) + sizeof(udp_header))
        return;

    auto& ip = *(ipv6_header*)pack.data;

    if (is_multicast_address(ip.dst))
        return;

    auto& udp = *(udp_header*)(pack.data + sizeof(ipv6_header));

    if (!skip_checksum) {
        if (calc_ipv6_checksum(ip, span((uint8_t*)&udp, pack.size - sizeof(ipv6_header)), IPPROTO_UDP) != 0)
            return;
    }

#if 0
    print("received_udp_packet (src {}, dst {}, {}, {:04x})\n",
          byteswap(udp.srcport), byteswap(udp.dstport), byteswap(udp.length),
          byteswap(udp.checksum));
#endif

    udp_socket<ipv6>* sock = nullptr;

    {
        lock_guard lock(udp_ports_lock);

        if (udp_ports_v6[udp.dstport]) {
            sock = udp_ports_v6[udp.dstport];

            {
                lock_guard lock2(sock->bind_lock);

                if (sock->bind_done && sock->bound_address != parse_ipv6_address("::") && sock->bound_address != ip.dst)
                    sock = nullptr;
                else
                    sock->acquire();
            }
        }
    }

    if (!sock) {
        send_port_unreachable_v6(ni, pack, src_mac);
        return;
    }

    {
        bool ignore = false;

        {
            lock_guard lock(sock->bind_lock);

            if (sock->connect_done && (sock->connected_address != ip.src || sock->connected_port != udp.srcport))
                ignore = true;
        }

        if (ignore) {
            sock->release();
            return;
        }
    }

    auto p = new socket_packet(pack);
    if (!p) {
        print("new failed\n");
        sock->release();
        return;
    }

    p->scope_id = ni.num;

    bool done = false;

    {
        sync_lock sl;
        lock_guard lock(sl);
        lock_guard lock2(sock->buffer_lock);

        if (sock->buffer_size + pack.size <= MAX_BUFFER_SIZE) {
            insert_tail_list(sock->buffer, p->le);
            sock->buffer_size += pack.size;
            done = true;
        }

        if (!sock->signalled) {
            wake_up_waiters(sock->waiters, WAIT_TYPE_READING);
            sock->signalled = true;
        }
    }

    if (!done)
        delete p;

    sock->release();
}

template<socket_family FAMILY>
udp_socket<FAMILY>::~udp_socket() {
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

template<socket_family FAMILY>
template<bool NONBLOCK>
nt_status udp_socket<FAMILY>::do_recvfrom(span<const span<uint8_t>> v, int flags,
                                          sockaddr_storage* from, size_t* fromlen,
                                          uint64_t& recv_bytes) {
    // FIXME - MSG_TRUNC

    if (flags & ~MSG_PEEK)
        return nt_status::INVALID_PARAMETER;

    recv_bytes = 0;

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
        } else if (shutdown_read)
            break;

        auto udp_read = [&](socket_packet& sp) {
            size_t overhead;
            uint32_t to_copy;
            bool ipv4_packet;

            if constexpr (is_same_v<FAMILY, ipv4>)
                ipv4_packet = true;
            else if constexpr (is_same_v<FAMILY, ipv6>) {
                struct ip_common {
                    uint32_t : 4;
                    uint32_t version : 4;
                    uint32_t : 24;
                };

                auto& c = *(ip_common*)sp.pack.data;

                ipv4_packet = c.version == 4;
            }

            if (ipv4_packet) {
                auto& ip = *(ipv4_header*)sp.pack.data;
                auto& udp = *(udp_header*)(sp.pack.data + (ip.ihl * sizeof(uint32_t)));

                overhead = (ip.ihl * sizeof(uint32_t)) + sizeof(udp_header);
                to_copy = (uint32_t)min(byteswap(udp.length) - sizeof(udp_header), vec_size);
            } else {
                overhead = sizeof(typename FAMILY::header) + sizeof(udp_header);
                to_copy = (uint32_t)min(sp.pack.size - overhead, vec_size);
            }

            memcpy_vec(v, top, sp.pack.data + overhead, to_copy);

            recv_bytes = to_copy;

            if (from && fromlen && *fromlen > 0) {
                if constexpr (is_same_v<FAMILY, ipv4>) {
                    auto& addr = *(sockaddr_in*)from;
                    auto& ip = *(ipv4_header*)sp.pack.data;
                    auto& udp = *(udp_header*)(sp.pack.data + (ip.ihl * sizeof(uint32_t)));

                    memset(&addr, 0, sizeof(addr));

                    addr.sin_family = AF_INET;
                    addr.sin_port = udp.srcport;
                    memcpy(&addr.sin_addr, ip.src.v.data(), sizeof(ipv4_address));

                    *fromlen = sizeof(addr);
                } else if constexpr (is_same_v<FAMILY, ipv6>) {
                    auto& addr = *(sockaddr_in6*)from;

                    memset(&addr, 0, sizeof(addr));

                    addr.sin6_family = AF_INET6;
                    addr.sin6_flowinfo = 0; // FIXME? Allowed by RFC3493, but not necessarily what Linux does

                    if (ipv4_packet) {
                        auto& ip = *(ipv4_header*)sp.pack.data;
                        auto& udp = *(udp_header*)(sp.pack.data + (ip.ihl * sizeof(uint32_t)));

                        addr.sin6_port = udp.srcport;
                        *(ipv6_address*)&addr.sin6_addr = map_ipv4_address_to_ipv6(ip.src);
                    } else {
                        auto& ip = *(ipv6_header*)sp.pack.data;
                        auto& udp = *(udp_header*)(sp.pack.data + sizeof(ipv6_header));

                        addr.sin6_port = udp.srcport;
                        memcpy(&addr.sin6_addr, ip.src.v.data(), sizeof(ipv6_address));
                    }

                    addr.sin6_scope_id = sp.scope_id;

                    *fromlen = sizeof(addr);
                }
            }
        };

        nt_status error = nt_status::SUCCESS;

        if (flags & MSG_PEEK) {
            {
                lock_guard lock(buffer_lock);

                if (!success(pending_error))
                    error = pending_error;

                if (is_list_empty(buffer))
                    continue;

                if (success(error)) {
                    auto& sp = containing_record<socket_packet>(buffer.next);

                    udp_read(sp);
                    break;
                }
            }

            release();

            return error;
        } else {
            socket_packet* sp = nullptr;

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

            udp_read(*sp);

            delete sp;

            break;
        }
    }

    release();

    return nt_status::SUCCESS;
}

template<socket_family FAMILY>
uint16_t find_ephemeral_udp_port(bool dual_stack) {
    auto rnd = kernel_time_page.system_time / (SYSTEM_TIME_RESOLUTION / TICK_RATE);
    uint16_t port = 49152 + (rnd & 0x3fff);
    uint16_t orig_port = port;

    while (true) {
        if (dual_stack) {
            if (!udp_ports_v4[byteswap(port)] && !udp_ports_v6[byteswap(port)])
                return byteswap(port);
        } else {
            if constexpr (is_same_v<FAMILY, ipv4>) {
                if (!udp_ports_v4[byteswap(port)])
                    return byteswap(port);
            } else if constexpr (is_same_v<FAMILY, ipv6>) {
                if (!udp_ports_v6[byteswap(port)])
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

template<socket_family FAMILY>
static nt_status do_sendto(base_udp_socket& sock,
                           span<const span<const uint8_t>> v,
                           int flags, const typename FAMILY::address& dst,
                           uint16_t dstport, uint32_t dstscope,
                           uint64_t& sent_bytes) {
    nt_status status;
    typename FAMILY::address src;
    uint16_t srcport;
    auto vec_size = get_vec_size(v);
    span<const uint8_t> top = vec_size == 0 ? span<const uint8_t>() : v.front();

    if (sock.shutdown_write)
        return nt_status::CONNECTION_ABORTED;

    if (flags != 0)
        return nt_status::INVALID_PARAMETER;

    bool has_error = false;

    {
        lock_guard lock(sock.buffer_lock);
        has_error = !success(sock.pending_error);
    }

    if (has_error) { // avoid getting sync_lock unless we have to
        nt_status err = nt_status::SUCCESS;

        {
            sync_lock sl;
            lock_guard lock(sl);
            lock_guard lock2(sock.buffer_lock);

            if (!success(sock.pending_error)) {
                err = sock.pending_error;
                sock.pending_error = nt_status::SUCCESS;

                if (is_list_empty(sock.buffer))
                    sock.signalled = false;
            }
        }

        if (!success(err))
            return err;
    }

    // get source port (implicit bind on first sendto)

    {
        lock_guard lock(udp_ports_lock);
        lock_guard lock2(sock.bind_lock);

        if (!sock.bind_done) {
            srcport = find_ephemeral_udp_port<FAMILY>(!sock.is_ipv4 && !sock.ipv6_only);

            if (srcport == 0) // no ephemeral ports left
                return nt_status::ADDRESS_ALREADY_ASSOCIATED;

            if constexpr (is_same_v<FAMILY, ipv4>) {
                udp_ports_v4[srcport] = &sock;

                if (!sock.is_ipv4 && !sock.ipv6_only) {
                    udp_ports_v6[srcport] = &static_cast<udp_socket<ipv6>&>(sock);
                    sock.acquire();
                }
            } else if constexpr (is_same_v<FAMILY, ipv6>) {
                udp_ports_v6[srcport] = &static_cast<udp_socket<ipv6>&>(sock);

                if (!sock.ipv6_only) {
                    udp_ports_v4[srcport] = &sock;
                    sock.acquire();
                }
            }

            sock.acquire();

            sock.bound_port = srcport;

            if constexpr (is_same_v<FAMILY, ipv4>) {
                if (sock.is_ipv4) {
                    auto& s2 = static_cast<udp_socket<ipv4>&>(sock);

                    s2.bound_address = parse_ipv4_address("0.0.0.0");
                } else {
                    auto& s2 = static_cast<udp_socket<ipv6>&>(sock);

                    s2.bound_address = parse_ipv6_address("::");
                }
            } else if constexpr (is_same_v<FAMILY, ipv6>) {
                auto& s2 = static_cast<udp_socket<ipv6>&>(sock);

                s2.bound_address = parse_ipv6_address("::");
            }

            sock.bind_done = true;
            sock.autobound = true;
        }

        srcport = sock.bound_port;

        if constexpr (is_same_v<FAMILY, ipv4>) {
            if (sock.is_ipv4) {
                auto& s2 = static_cast<udp_socket<ipv4>&>(sock);

                src = s2.bound_address;
            } else {
                auto& s2 = static_cast<udp_socket<ipv6>&>(sock);

                src = extract_mapped_address(s2.bound_address);
            }
        } else if constexpr (is_same_v<FAMILY, ipv6>) {
            auto& s2 = static_cast<udp_socket<ipv6>&>(sock);

            src = s2.bound_address;
        }
    }

    typename FAMILY::address gateway;

    netif* pni;

    if constexpr (is_same_v<FAMILY, ipv4>)
        status = ipv4_choose_interface(dst, pni, src, gateway);
    else if constexpr (is_same_v<FAMILY, ipv6>)
        status = ipv6_choose_interface(dst, dstscope, pni, src);

    if (!success(status))
        return status;

    auto& ni = *pni;

    if (vec_size + sizeof(typename FAMILY::header) + sizeof(udp_header) > ni.mtu) {
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
            else if constexpr (is_same_v<FAMILY, ipv6>)
                mac = find_mac_address(ni, dst);
        }
    }

    auto pack = new packet;
    if (!pack) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    pack->size = sizeof(typename FAMILY::header) + sizeof(udp_header) + vec_size;

    auto& udp = *(udp_header*)(pack->data + sizeof(typename FAMILY::header));
    udp.srcport = srcport;
    udp.dstport = dstport;
    udp.length = byteswap((uint16_t)(sizeof(udp_header) + vec_size));
    udp.checksum = 0;

    memcpy_vec(pack->data + sizeof(typename FAMILY::header) + sizeof(udp_header),
               v, top, vec_size);

    if constexpr (is_same_v<FAMILY, ipv4>) {
        auto& ip = *(ipv4_header*)pack->data;

        ip.ihl = sizeof(ipv4_header) / sizeof(uint32_t);
        ip.version = 4;
        ip.ecn = 0;
        ip.dscp = 0;
        ip.total_length = byteswap((uint16_t)(sizeof(ipv4_header) + sizeof(udp_header) + vec_size));
        ip.identification = 0;
        ip.fragment_offset1 = 0;
        ip.MF = 0;
        ip.DF = 1;
        ip.R = 0;
        ip.fragment_offset2 = 0;
        ip.ttl = DEFAULT_HOP_LIMIT;
        ip.protocol = IPPROTO_UDP;
        ip.header_checksum = 0;
        ip.src = src;
        ip.dst = dst;

        ip.header_checksum = calc_ipv4_checksum(span(pack->data, sizeof(ipv4_header)));

        udp.checksum = calc_ipv4_checksum(ip, span((uint8_t*)&udp, pack->size - sizeof(ipv4_header)),
                                          IPPROTO_UDP);

        if (udp.checksum == 0)
            udp.checksum = 0xffff;
    } else if constexpr (is_same_v<FAMILY, ipv6>) {
        auto& ip = *(ipv6_header*)pack->data;
        ip.traffic_class1 = 0;
        ip.version = 6;
        ip.traffic_class2 = 0;
        ip.flow_label_high = 0;
        ip.flow_label_medium = 0;
        ip.flow_label_low = 0;
        ip.payload_length = byteswap((uint16_t)(sizeof(udp_header) + vec_size));
        ip.next_header = IPPROTO_UDP;
        ip.hop_limit = DEFAULT_HOP_LIMIT;
        ip.src = src;
        ip.dst = dst;

        if (!ni.tx_udpv6_csum_offload)
            udp.checksum = calc_ipv6_checksum(ip, span((uint8_t*)&udp, pack->size - sizeof(ipv6_header)),
                                              IPPROTO_UDP);
        else {
            udp.checksum = calc_ipv6_checksum_offload(ip, (uint32_t)(pack->size - sizeof(ipv6_header)),
                                                      IPPROTO_UDP);
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

template<socket_family FAMILY>
nt_status udp_socket<FAMILY>::bind(const sockaddr* addr, size_t addrlen) {
    if (addr->sa_family != FAMILY::family)
        return nt_status::INVALID_PARAMETER;

    if (addrlen < sizeof(typename FAMILY::sockaddr))
        return nt_status::BUFFER_TOO_SMALL;

    auto& b = *(typename FAMILY::sockaddr*)addr;
    typename FAMILY::address ip;

    if constexpr (is_same_v<FAMILY, ipv4>) {
        ip = *(typename FAMILY::address*)&b.sin_addr;

        if (ip != parse_ipv4_address("0.0.0.0")) {
            if (!is_our_ip(ip))
                return nt_status::INVALID_ADDRESS_COMPONENT;
        }
    } else if constexpr (is_same_v<FAMILY, ipv6>) {
        ip = *(typename FAMILY::address*)&b.sin6_addr;

        if (ip != parse_ipv6_address("::")) {
            if (!is_our_ip(ip))
                return nt_status::INVALID_ADDRESS_COMPONENT;
        }
    }

    {
        uint16_t port;
        bool dual_stack = false;

        lock_guard lock(udp_ports_lock);
        lock_guard lock2(bind_lock);

        if (bind_done)
            return nt_status::INVALID_PARAMETER;

        if constexpr (is_same_v<FAMILY, ipv4>) {
            if (b.sin_port == 0) {
                port = find_ephemeral_udp_port<FAMILY>(false);
                if (port == 0) // no ephemeral ports left
                    return nt_status::ADDRESS_ALREADY_ASSOCIATED;
            } else {
                port = b.sin_port;

                if (udp_ports_v4[b.sin_port])
                    return nt_status::ADDRESS_ALREADY_ASSOCIATED;
            }

            udp_ports_v4[port] = this;
        } else if constexpr (is_same_v<FAMILY, ipv6>) {
            dual_stack = ip == parse_ipv6_address("::") && !ipv6_only;

            if (b.sin6_port == 0) {
                port = find_ephemeral_udp_port<FAMILY>(dual_stack);
                if (port == 0) // no ephemeral ports left
                    return nt_status::ADDRESS_ALREADY_ASSOCIATED;
            } else {
                port = b.sin6_port;

                if (udp_ports_v6[b.sin6_port])
                    return nt_status::ADDRESS_ALREADY_ASSOCIATED;

                if (dual_stack && udp_ports_v4[b.sin6_port])
                    return nt_status::ADDRESS_ALREADY_ASSOCIATED;
            }

            udp_ports_v6[port] = this;

            if (dual_stack) {
                udp_ports_v4[port] = this;
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

template<socket_family FAMILY>
nt_status udp_socket<FAMILY>::connect(const sockaddr* addr, size_t addrlen) {
    nt_status status;

    if (addr->sa_family == AF_UNSPEC) {
        lock_guard lock(udp_ports_lock);
        lock_guard lock2(bind_lock);

        connect_done = false;

        if (bind_done && autobound) {
            if constexpr (is_same_v<FAMILY, ipv4>)
                udp_ports_v4[bound_port] = nullptr;
            else if constexpr (is_same_v<FAMILY, ipv6>) {
                udp_ports_v6[bound_port] = nullptr;

                if (udp_ports_v4[bound_port] == this) {
                    udp_ports_v4[bound_port] = nullptr;
                    this->release();
                }
            }

            this->release();

            bind_done = false;
            autobound = false;
        }

        return nt_status::SUCCESS;
    }

    if (addr->sa_family != FAMILY::family)
        return nt_status::INVALID_PARAMETER;

    if (addrlen < sizeof(typename FAMILY::sockaddr))
        return nt_status::BUFFER_TOO_SMALL;

    auto& c = *(typename FAMILY::sockaddr*)addr;

    uint16_t port;
    typename FAMILY::address src;

    {
        netif* pni = nullptr;

        if constexpr (is_same_v<FAMILY, ipv4>) {
            ipv4_address gateway;

            src = parse_ipv4_address("0.0.0.0");

            {
                lock_guard lock(bind_lock);

                if (bind_done)
                    src = bound_address;
            }

            status = ipv4_choose_interface(*(ipv4_address*)&c.sin_addr, pni,
                                        src, gateway);
            if (!success(status))
                return status;
        } else if constexpr (is_same_v<FAMILY, ipv6>) {
            auto& addr = *(ipv6_address*)&c.sin6_addr;

            if (is_mapped_address(addr) && ipv6_only)
                return nt_status::INVALID_PARAMETER;

            src = parse_ipv6_address("::");

            {
                lock_guard lock(bind_lock);

                if (bind_done)
                    src = bound_address;
            }

            if (src != parse_ipv6_address("::") && !!is_mapped_address(addr) != !!is_mapped_address(src))
                return nt_status::INVALID_PARAMETER;

            if (is_mapped_address(addr)) {
                ipv4_address gateway, src4;

                if (src == parse_ipv6_address("::"))
                    src4 = parse_ipv4_address("0.0.0.0");
                else
                    src4 = extract_mapped_address(src);

                status = ipv4_choose_interface(extract_mapped_address(addr), pni,
                                               src4, gateway);
                if (!success(status))
                    return status;

                src = map_ipv4_address_to_ipv6(src4);
            } else {
                status = ipv6_choose_interface(addr, c.sin6_scope_id, pni, src);
                if (!success(status))
                    return status;
            }
        }

        if (pni)
            pni->release();
    }

    {
        lock_guard lock(udp_ports_lock);
        lock_guard lock2(bind_lock);

        if (!bind_done) { // implicit bind if not already done
            if constexpr (is_same_v<FAMILY, ipv4>) {
                port = find_ephemeral_udp_port<FAMILY>(false);
                if (port == 0) // no ephemeral ports left
                    return nt_status::ADDRESS_ALREADY_ASSOCIATED;

                udp_ports_v4[port] = this;
            } else if constexpr (is_same_v<FAMILY, ipv6>) {
                port = find_ephemeral_udp_port<FAMILY>(!ipv6_only);
                if (port == 0) // no ephemeral ports left
                    return nt_status::ADDRESS_ALREADY_ASSOCIATED;

                udp_ports_v6[port] = this;

                if (!ipv6_only) {
                    udp_ports_v4[port] = this;
                    this->acquire();
                }
            }

            this->acquire();

            bound_port = port;
            bound_address = src;

            bind_done = true;
            autobound = true;
        }

        if constexpr (is_same_v<FAMILY, ipv4>) {
            connected_port = c.sin_port;
            connected_address = *(ipv4_address*)&c.sin_addr;
        } else {
            connected_port = c.sin6_port;
            connected_address = *(ipv6_address*)&c.sin6_addr;
            connected_scope = c.sin6_scope_id;
        }

        connect_done = true;
    }

    return nt_status::SUCCESS;
}

template<socket_family FAMILY>
void udp_socket<FAMILY>::last_handle_closed() {
    {
        lock_guard lock(udp_ports_lock);
        lock_guard lock2(bind_lock);

        if (bind_done && udp_ports_v4[bound_port] == this) {
            udp_ports_v4[bound_port] = nullptr;
            this->release();
        }

        if constexpr (is_same_v<FAMILY, ipv6>) {
            if (bind_done && udp_ports_v6[bound_port] == this) {
                udp_ports_v6[bound_port] = nullptr;
                this->release();
            }
        }
    }

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

void handle_icmpv4_error_udp(packet& pack) {
    const auto& orig_ip = *(ipv4_header*)pack.data;
    size_t ip_header_size = orig_ip.ihl * sizeof(uint32_t);
    const auto& icmp = *(icmpv4_error*)(pack.data + ip_header_size);

    if (pack.size < ip_header_size + sizeof(icmpv4_error) + sizeof(udp_header))
        return;

    const auto& udp = *(udp_header*)(pack.data + ip_header_size + sizeof(icmpv4_error));

    base_udp_socket* sock;

    {
        lock_guard lock(udp_ports_lock);

        if (udp_ports_v4[udp.srcport]) {
            sock = udp_ports_v4[udp.srcport];
            sock->acquire();
        } else
            return;
    }

    bool drop = false;

    {
        lock_guard lock(sock->bind_lock);

        if (sock->is_ipv4) {
            auto& s2 = static_cast<udp_socket<ipv4>&>(*sock);

            if (s2.bound_address != parse_ipv4_address("0.0.0.0") && s2.bound_address != icmp.header.src)
                drop = true;
            else if (sock->connect_done && (s2.connected_address != icmp.header.dst || sock->connected_port != udp.dstport))
                drop = true;
        } else {
            auto& s2 = static_cast<udp_socket<ipv6>&>(*sock);

            if (s2.bound_address != parse_ipv6_address("::") && s2.bound_address != map_ipv4_address_to_ipv6(icmp.header.src))
                drop = true;
            else if (sock->connect_done && (s2.connected_address != map_ipv4_address_to_ipv6(icmp.header.dst) || sock->connected_port != udp.dstport))
                drop = true;
        }

    }

    if (drop) {
        sock->release();
        return;
    }

    nt_status err;

    switch (icmp.type) {
        case icmpv4_type::dest_unreachable:
            switch ((icmpv4_dest_unreachable_code)icmp.code) {
                using enum icmpv4_dest_unreachable_code;

                case dest_net_unreachable:
                case dest_net_unknown:
                    err = nt_status::NETWORK_UNREACHABLE;
                    break;

                case host_administratively_prohibited:
                case net_administratively_prohibited:
                case communication_administratively_prohibited:
                    err = nt_status::ACCESS_DENIED;
                    break;

                case dest_host_unreachable:
                case dest_host_unknown:
                    err = nt_status::HOST_UNREACHABLE;
                    break;

                case dest_proto_unreachable:
                case dest_port_unreachable:
                    err = nt_status::CONNECTION_REFUSED;
                    break;

                default:
                    err = nt_status::HOST_UNREACHABLE;
                    break;
            }
            break;

        case icmpv4_type::time_exceeded:
            err = nt_status::HOST_UNREACHABLE;
            break;

        case icmpv4_type::param_problem:
            err = nt_status::PROTOCOL_UNREACHABLE;
            break;

        default:
            unreachable();
    }

    {
        sync_lock sl;
        lock_guard lock(sl);
        lock_guard lock2(sock->buffer_lock);

        if (success(sock->pending_error)) {
            sock->pending_error = err;

            if (!sock->signalled) {
                wake_up_waiters(sock->waiters, WAIT_TYPE_READING);
                sock->signalled = true;
            }
        }
    }

    sock->release();
}

void handle_icmpv6_error_udp(packet& pack) {
    const auto& icmp = *(icmpv6_error*)(pack.data + sizeof(ipv6_header));

    if (pack.size < sizeof(ipv6_header) + sizeof(icmpv6_error) + sizeof(udp_header))
        return;

    const auto& udp = *(udp_header*)(pack.data + sizeof(ipv6_header) + sizeof(icmpv6_error));

    udp_socket<ipv6>* sock;

    {
        lock_guard lock(udp_ports_lock);

        if (udp_ports_v6[udp.srcport]) {
            sock = udp_ports_v6[udp.srcport];
            sock->acquire();
        } else
            return;
    }

    bool drop = false;

    {
        lock_guard lock(sock->bind_lock);

        if (sock->bound_address != parse_ipv6_address("::") && sock->bound_address != icmp.header.src)
            drop = true;
        else if (sock->connect_done && (sock->connected_address != icmp.header.dst || sock->connected_port != udp.dstport))
            drop = true;
    }

    if (drop) {
        sock->release();
        return;
    }

    nt_status err;

    switch (icmp.type) {
        case icmpv6_type::dest_unreachable:
            switch ((icmpv6_dest_unreachable_code)icmp.code) {
                case no_route:
                case reject_route:
                    err = nt_status::NETWORK_UNREACHABLE;
                    break;

                case administratively_prohibited:
                case policy_fail:
                    err = nt_status::ACCESS_DENIED;
                    break;

                case beyond_scope:
                case address_unreachable:
                    err = nt_status::HOST_UNREACHABLE;
                    break;

                case no_port:
                    err = nt_status::CONNECTION_REFUSED;
                    break;

                default:
                    err = nt_status::HOST_UNREACHABLE;
                    break;
            }
            break;

        case icmpv6_type::packet_too_big:
            err = nt_status::BUFFER_TOO_SMALL;
            break;

        case icmpv6_type::time_exceeded:
            err = nt_status::HOST_UNREACHABLE;
            break;

        case icmpv6_type::param_problem:
            err = nt_status::PROTOCOL_UNREACHABLE;
            break;

        default:
            unreachable();
    }

    {
        sync_lock sl;
        lock_guard lock(sl);
        lock_guard lock2(sock->buffer_lock);

        if (success(sock->pending_error)) {
            sock->pending_error = err;

            if (!sock->signalled) {
                wake_up_waiters(sock->waiters, WAIT_TYPE_READING);
                sock->signalled = true;
            }
        }
    }

    sock->release();
}

nt_status create_new_ipv4_udp_socket(object_ref& ref) {
    auto s = new udp_socket<ipv4>;
    if (!s) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    ref = object_ref{*s};

    return nt_status::SUCCESS;
}

nt_status create_new_ipv6_udp_socket(object_ref& ref) {
    auto s = new udp_socket<ipv6>;
    if (!s) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    ref = object_ref{*s};

    return nt_status::SUCCESS;
}

template<socket_family FAMILY>
nt_status udp_socket<FAMILY>::getsockname(span<uint8_t> buf, size_t& len) {
    if (buf.size() < sizeof(typename FAMILY::sockaddr))
        return nt_status::BUFFER_TOO_SMALL;

    auto& addr = *(typename FAMILY::sockaddr*)buf.data();

    memset(&addr, 0, sizeof(typename FAMILY::sockaddr));

    if constexpr (is_same_v<FAMILY, ipv4>)
        addr.sin_family = FAMILY::family;
    else if constexpr (is_same_v<FAMILY, ipv6>)
        addr.sin6_family = FAMILY::family;

    {
        lock_guard lock(bind_lock);

        if (bind_done) {
            if constexpr (is_same_v<FAMILY, ipv4>) {
                addr.sin_port = bound_port;
                *(ipv4_address*)&addr.sin_addr = bound_address;
            } else if constexpr (is_same_v<FAMILY, ipv6>) {
                addr.sin6_port = bound_port;
                *(ipv6_address*)&addr.sin6_addr = bound_address;
                addr.sin6_scope_id = connected_scope;
            }
        }
    }

    len = sizeof(typename FAMILY::sockaddr);

    return nt_status::SUCCESS;
}

template<socket_family FAMILY>
nt_status udp_socket<FAMILY>::getpeername(span<uint8_t> buf, size_t& len) {
    if (buf.size() < sizeof(typename FAMILY::sockaddr))
        return nt_status::BUFFER_TOO_SMALL;

    auto& addr = *(typename FAMILY::sockaddr*)buf.data();

    memset(&addr, 0, sizeof(typename FAMILY::sockaddr));

    if constexpr (is_same_v<FAMILY, ipv4>)
        addr.sin_family = FAMILY::family;
    else if constexpr (is_same_v<FAMILY, ipv6>)
        addr.sin6_family = FAMILY::family;

    {
        lock_guard lock(bind_lock);

        if (!connect_done)
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

    len = sizeof(typename FAMILY::sockaddr);

    return nt_status::SUCCESS;
}

template<socket_family FAMILY>
nt_status udp_socket<FAMILY>::shutdown(int how) {
    if (how != SHUT_RD && how != SHUT_WR && how != SHUT_RDWR)
        return nt_status::INVALID_PARAMETER;

    {
        lock_guard lock(bind_lock);

        if (!connect_done)
            return nt_status::CONNECTION_DISCONNECTED;
    }

    if (how == SHUT_RD || how == SHUT_RDWR) {
        shutdown_read = true;

        {
            sync_lock sl;
            lock_guard lock(sl);

            if (!signalled) {
                wake_up_waiters(waiters, WAIT_TYPE_READING);
                signalled = true;
            }
        }
    }

    if (how == SHUT_WR || how == SHUT_RDWR)
        shutdown_write = true;

    return nt_status::SUCCESS;
}

template<socket_family FAMILY>
nt_status udp_socket<FAMILY>::setsockopt_ipv6(sockopt_ipv6 opt, span<const uint8_t> buf) {
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
nt_status udp_socket<FAMILY>::getsockopt_ipv6(sockopt_ipv6 opt, span<uint8_t> buf,
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

template<socket_family FAMILY>
nt_status udp_socket<FAMILY>::sendto(span<const span<const uint8_t>> v,
                                     int flags, sockaddr* to,
                                     size_t tolen, uint64_t& sent_bytes,
                                     span<const HANDLE> handles) {
    typename FAMILY::address dst;
    uint16_t dstport;
    uint32_t dstscope;

    if (!handles.empty())
        return nt_status::NOT_SUPPORTED;

    {
        lock_guard lock(bind_lock);

        if (!connect_done && (!to || tolen < sizeof(typename FAMILY::sockaddr)))
            return nt_status::INVALID_PARAMETER;

        // if connected, sendto destination is ignored
        if (connect_done) {
            if constexpr (is_same_v<FAMILY, ipv4>) {
                auto& s2 = static_cast<udp_socket<ipv4>&>(*this);

                dst = s2.connected_address;
                dstport = s2.connected_port;
                dstscope = 0;
            } else if constexpr (is_same_v<FAMILY, ipv6>) {
                auto& s2 = static_cast<udp_socket<ipv6>&>(*this);

                dst = s2.connected_address;
                dstport = s2.connected_port;
                dstscope = s2.connected_scope;
            }
        } else {
            if constexpr (is_same_v<FAMILY, ipv4>) {
                dst = *(ipv4_address*)&((sockaddr_in*)to)->sin_addr;
                dstport = ((sockaddr_in*)to)->sin_port;
                dstscope = 0;
            } else if constexpr (is_same_v<FAMILY, ipv6>) {
                dst = *(ipv6_address*)&((sockaddr_in6*)to)->sin6_addr;
                dstport = ((sockaddr_in6*)to)->sin6_port;
                dstscope = ((sockaddr_in6*)to)->sin6_scope_id;
            }
        }

        if constexpr (is_same_v<FAMILY, ipv6>) {
            if (bind_done && bound_address != parse_ipv6_address("::") && !!is_mapped_address(bound_address) != !!is_mapped_address(dst))
                return nt_status::INVALID_PARAMETER;
        }
    }

    if constexpr (is_same_v<FAMILY, ipv4>) {
        return do_sendto<FAMILY>(*this, v, flags, dst, dstport, dstscope,
                                sent_bytes);
    } else if constexpr (is_same_v<FAMILY, ipv6>) {
        if (is_mapped_address(dst)) {
            if (ipv6_only)
                return nt_status::INVALID_PARAMETER;

            return do_sendto<ipv4>(*this, v, flags, extract_mapped_address(dst),
                                   dstport, dstscope, sent_bytes);
        } else
            return do_sendto<FAMILY>(*this, v, flags, dst, dstport, dstscope,
                                     sent_bytes);
    }

}

template<socket_family FAMILY>
nt_status udp_socket<FAMILY>::sendto_nonblock(span<const span<const uint8_t>> v,
                                              int flags, sockaddr* to,
                                              size_t tolen, uint64_t& sent_bytes,
                                              span<const HANDLE> handles) {
    return sendto(v, flags, to, tolen, sent_bytes, handles);
}
