// SPDX-License-Identifier: LGPL-2.1-or-later
#include <string_view>
#include <optional>
#include <bit>
#include <span>
#include <array>
#include <utility>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <poll.h>

import nt;
import moxnox_ioctls;
import unique_handle;
import ws2_32;
import ipv4;
import ipv6;
import print;
import ucrtbase;
import format;
import timepage;

#undef errno
import errno;

using namespace std;

struct ipv4 {
    using address = ipv4_address;
};

struct ipv6 {
    using address = ipv6_address;
};

template<typename T>
concept socket_family = is_same_v<T, ipv4> || is_same_v<T, ipv6>;

struct dns_header {
    uint16_t id;
    uint16_t recursion_desired : 1;
    uint16_t truncated : 1;
    uint16_t authoritative_answer : 1;
    uint16_t opcode : 4;
    uint16_t response : 1;
    uint16_t rcode : 4;
    uint16_t checking_disabled : 1;
    uint16_t authenticated_data : 1;
    uint16_t : 1;
    uint16_t recursion_available : 1;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
} __attribute__((packed));

struct dns_question_suffix {
    uint16_t type;
    uint16_t dns_class;
} __attribute__((packed));

struct dns_resource_record {
    uint16_t type;
    uint16_t dns_class;
    uint32_t ttl;
    uint16_t rdlength;
} __attribute__((packed));

static const char16_t netctl_path[] = u"/dev/netctl";

static const uint16_t DNS_PORT = 53;

static constexpr auto DNS_A = byteswap((uint16_t)1);
static constexpr auto DNS_CNAME = byteswap((uint16_t)5);
static constexpr auto DNS_PTR = byteswap((uint16_t)12);
static constexpr auto DNS_AAAA = byteswap((uint16_t)28);

static constexpr auto DNS_IN = byteswap((uint16_t)1);

static constexpr uint16_t DNS_FORMERR = 1;
static constexpr uint16_t DNS_SERVFAIL = 2;
static constexpr uint16_t DNS_NXDOMAIN = 3;
static constexpr uint16_t DNS_NOTIMP = 4;
static constexpr uint16_t DNS_REFUSED = 5;

static const unsigned int SYSTEM_TIME_RESOLUTION = 10000000; // 100ns
static const unsigned int DNS_TIMEOUT = 5; // secs

static constexpr size_t REVERSE_IPV6_LEN = 74;

class unique_fd {
public:
    unique_fd() : fd(-1) {
    }

    explicit unique_fd(int fd) : fd(fd) {
    }

    unique_fd(unique_fd&& that) noexcept {
        fd = that.fd;
        that.fd = -1;
    }

    unique_fd(const unique_fd&) = delete;
    unique_fd& operator=(const unique_fd&) = delete;

    unique_fd& operator=(unique_fd&& that) noexcept {
        if (fd >= 0)
            _close(fd);

        fd = that.fd;
        that.fd = -1;

        return *this;
    }

    ~unique_fd() {
        if (fd < 0)
            return;

        _close(fd);
    }

    explicit operator bool() const noexcept {
        return fd != -1;
    }

    void reset(int new_fd = -1) noexcept {
        if (fd >= 0)
            _close(fd);

        fd = new_fd;
    }

    int get() const noexcept {
        return fd;
    }

private:
    int fd;
};

static unique_handle open_netctl() {
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    HANDLE h;

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;

    us.Length = us.MaximumLength = (uint16_t)(sizeof(netctl_path) - sizeof(char16_t));
    us.Buffer = (char16_t*)netctl_path;

    auto status = NtCreateFile(&h, FILE_READ_ATTRIBUTES, &oa, &iosb, nullptr, 0,
                               0, FILE_OPEN, FILE_SYNCHRONOUS_IO_ALERT, nullptr,
                               0);
    if (!success(status)) {
        print_stderr("NtCreateFile returned {}\n", status);
        return {};
    }

    return unique_handle{h};
}

static uint32_t resolve_scope(string_view s) {
    nt_status status;
    MOXNOX_NETCTL_GET_NETIF_BY_NAME p;
    IO_STATUS_BLOCK iosb;
    uint32_t id;

    if (s.size() > sizeof(p.name))
        return 0;

    auto netctl = open_netctl();
    if (!netctl)
        return 0;

    memcpy(p.name, s.data(), s.size());
    memset(p.name + s.size(), 0, sizeof(p.name) - s.size());

    status = NtDeviceIoControlFile(netctl.get(), 0, nullptr, nullptr, &iosb,
                                   IOCTL_MOXNOX_NETCTL_GET_NETIF_BY_NAME, &p,
                                   sizeof(p), &id, (uint32_t)sizeof(id));
    if (!success(status)) {
        print_stderr("NtDeviceIoControlFile returned {}\n", status);
        return 0;
    }

    return id;
}

static optional<uint32_t> parse_scope_id(string_view n) {
    if (n.empty())
        return 0;

    bool numeric = true;
    unsigned int ret = 0;

    for (auto c : n) {
        if (c < '0' || c > '9') {
            numeric = false;
            break;
        }

        ret *= 10;
        ret += (unsigned int)(c - '0');
    }

    if (numeric)
        return ret;

    ret = resolve_scope(n);

    if (ret != 0)
        return ret;
    else
        return nullopt;
}

static optional<uint16_t> parse_service(string_view n) {
    if (n.empty())
        return 0;

    bool numeric = true;
    unsigned int ret = 0;

    for (auto c : n) {
        if (c < '0' || c > '9') {
            numeric = false;
            break;
        }

        ret *= 10;
        ret += (unsigned int)(c - '0');
    }

    if (numeric)
        return ret;

    return nullopt;
}

static size_t get_encoded_name_len(string_view name) {
    size_t ret = 1;

    while (true) {
        auto dot = name.find('.');

        ret++;

        if (dot == string_view::npos) {
            ret += name.size();
            break;
        }

        ret += dot;
        name = string_view(name.data() + dot + 1, name.size() - dot - 1);
    }

    return ret;
}

static void get_encoded_name(string_view name, uint8_t* buf) {
    while (true) {
        auto dot = name.find('.');

        if (dot == string_view::npos) {
            *buf = (uint8_t)name.size();
            buf++;

            memcpy(buf, name.data(), name.size());
            buf += name.size();

            break;
        }

        *buf = (uint8_t)dot;
        buf++;

        memcpy(buf, name.data(), dot);
        buf += dot;

        name = string_view(name.data() + dot + 1, name.size() - dot - 1);
    }

    *buf = 0;
}

static string get_dns_name(span<uint8_t>& ans, span<uint8_t> pack,
                           unsigned int hop = 0) {
    string ret;
    auto sp = ans;

    if (hop == 5) // avoid infinite loops
        return "";

    while (true) {
        if (sp.empty())
            return "";

        if ((sp[0] & 0xc0) == 0xc0) { // compression
            if (sp.size() < sizeof(uint16_t))
                return "";

            auto val = (uint16_t)(byteswap(*(uint16_t*)sp.data()) & 0x3fff);

            if (val > pack.size())
                return "";

            auto sp2 = span(pack.data() + val, pack.size() - val);
            auto ret2 = get_dns_name(sp2, pack, hop + 1);

            if (ret2.empty())
                return "";

            ret.append(ret2);
            sp = span(sp.data() + sizeof(uint16_t), sp.size() - sizeof(uint16_t));

            break;
        } else {
            auto len = (uint8_t)sp[0];

            if (sp.size() < (size_t)(len + 1))
                return "";

            ret.append(string_view((char*)sp.data(), 1 + len));
            sp = span(sp.data() + 1 + len, sp.size() - 1 - len);

            if (len == 0)
                break;
        }
    }

    if (hop == 0)
        ans = sp;

    return ret;
}

static int parse_dns_response(span<uint8_t> sp, span<uint8_t> resp,
                              uint16_t num_answers, uint16_t type,
                              span<char> spname, addrinfo** res) {
    addrinfo* last_ai = nullptr;
    auto orig_sp = sp;
    unsigned int cname_count = 0;
    string name{string_view(spname.data(), spname.size())};

    static const int MAX_CNAME_COUNT = 10;

    *res = nullptr;

    while (true) {
        bool found_cname = false;

        sp = orig_sp;

        for (uint16_t i = 0; i < num_answers; i++) {
            if (sp.empty())
                break;

            auto name2 = get_dns_name(sp, resp);
            if (name2.empty())
                break;

            if (sp.size() < sizeof(dns_resource_record))
                break;

            auto& rr = *(dns_resource_record*)sp.data();

            sp = span(sp.data() + sizeof(rr), sp.size() - sizeof(rr));

            if (sp.size() < byteswap(rr.rdlength))
                break;

            auto payload = span(sp.data(), byteswap(rr.rdlength));

            sp = span(sp.data() + payload.size(), sp.size() - payload.size());

            if (rr.dns_class != DNS_IN)
                continue;

            if (name2.size() != name.size() || memcmp(name2.data(), name.data(), name.size()))
                continue;

            if (rr.type == DNS_CNAME) {
                auto tmp = get_dns_name(payload, resp);
                if (tmp.empty())
                    continue;

                swap(name, tmp);
                found_cname = true;
                break;
            }

            if (rr.type != type)
                continue;

            if (type == DNS_A && payload.size() != sizeof(ipv4_address))
                continue;
            else if (type == DNS_AAAA && payload.size() != sizeof(ipv6_address))
                continue;

            auto ai = (addrinfo*)malloc(sizeof(addrinfo));
            if (!ai)
                return EAI_MEMORY;

            switch (type) {
                case DNS_A: {
                    auto& addr = *(ipv4_address*)payload.data();

                    auto sock = (sockaddr_in*)malloc(sizeof(sockaddr_in));
                    if (!sock) {
                        free(ai);
                        return EAI_MEMORY;
                    }

                    sock->sin_family = AF_INET;
                    *(ipv4_address*)&sock->sin_addr = addr;
                    sock->sin_port = 0;

                    ai->ai_family = AF_INET;
                    ai->ai_addrlen = sizeof(sockaddr_in);
                    ai->ai_addr = (sockaddr*)sock;

                    break;
                }

                case DNS_AAAA: {
                    auto& addr = *(ipv6_address*)payload.data();

                    auto sock = (sockaddr_in6*)malloc(sizeof(sockaddr_in6));
                    if (!sock) {
                        free(ai);
                        return EAI_MEMORY;
                    }

                    sock->sin6_family = AF_INET6;
                    sock->sin6_flowinfo = 0;
                    *(ipv6_address*)&sock->sin6_addr = addr;
                    sock->sin6_scope_id = 0;
                    sock->sin6_port = 0;

                    ai->ai_family = AF_INET6;
                    ai->ai_addrlen = sizeof(sockaddr_in6);
                    ai->ai_addr = (sockaddr*)sock;

                    break;
                }
            }

            ai->ai_flags = 0;
            ai->ai_canonname = nullptr;
            ai->ai_next = nullptr;

            if (last_ai)
                last_ai->ai_next = ai;
            else
                *res = ai;

            last_ai = ai;
        }

        if (!found_cname)
            break;

        cname_count++;

        if (cname_count == MAX_CNAME_COUNT)
            break;
    }

    if (!*res)
        return EAI_NONAME;

    return 0;
}

static int dns_lookup2(int sock, const ipv6_address& server,
                       string_view name, addrinfo** res, int socktype,
                       int protocol, uint16_t port, uint16_t type,
                       addrinfo** last_entry) {
    LARGE_INTEGER sent_time;
    int64_t deadline;
    nt_status status;
    sockaddr_in6 dest;
    int ret;

    memset(&dest, 0, sizeof(dest));

    dest.sin6_family = AF_INET6;
    dest.sin6_port = byteswap(DNS_PORT);
    *(ipv6_address*)&dest.sin6_addr = server;

    auto name_len = get_encoded_name_len(name);
    size_t question_len = name_len + sizeof(dns_question_suffix);

    size_t packlen = sizeof(dns_header) + question_len;
    auto pack = (char*)malloc(packlen);
    if (!pack) {
        print("malloc failed\n");
        return EAI_MEMORY;
    }

    auto& h = *(dns_header*)pack;
    memset(&h, 0, sizeof(h));

    h.id = (uint16_t)rand();
    h.recursion_desired = 1;
    h.opcode = 0;
    h.response = 0;
    h.qdcount = byteswap((uint16_t)1);

    get_encoded_name(name, (uint8_t*)pack + sizeof(dns_header));

    auto& suf = *(dns_question_suffix*)(pack + sizeof(dns_header) + name_len);
    suf.type = type;
    suf.dns_class = DNS_IN;

    // FIXME - connect, so we get ICMP errors? (Will need to implement connect with AF_UNSPEC to disconnect.)

    auto sent = sendto(sock, pack, (int)packlen, 0, (struct sockaddr*)&dest,
                       sizeof(dest));
    if (sent < 0) {
        print_stderr("sendto returned {} (errno {})\n", sent, errno_to_string_view(*_errno()));
        free(pack);
        return EAI_SYSTEM;
    }

    // FIXME - can have larger packets with EDNS0
    // FIXME - TCP packets

    status = NtQuerySystemTime(&sent_time);
    if (!success(status)) {
        print_stderr("NtQuerySystemTime returned {}\n", status);
        free(pack);
        return EAI_SYSTEM;
    }

    deadline = sent_time.QuadPart + (DNS_TIMEOUT * SYSTEM_TIME_RESOLUTION);

    while (true) {
        char resp[512]; // maximum size of DNS packet
        LARGE_INTEGER now;
        pollfd pfd;
        sockaddr_in6 src;
        socklen_t srclen;

        status = NtQuerySystemTime(&now);
        if (!success(status)) {
            print_stderr("NtQuerySystemTime returned {}\n", status);
            ret = EAI_SYSTEM;
            break;
        }

        if (now.QuadPart >= deadline) {
            ret = EAI_AGAIN;
            break;
        }

        pfd.fd = sock;
        pfd.events = POLLIN;

        if (deadline <= now.QuadPart)
            ret = 0;
        else
            ret = poll(&pfd, 1, (int)((deadline - now.QuadPart) / 10000)); // convert units of 100ns to ms

        if (ret == 0) {
            ret = EAI_AGAIN;
            break;
        } else if (ret < 0) {
            if (*_errno() == EINTR)
                continue;

            print_stderr("poll failed (errno {})\n", errno_to_string_view(*_errno()));
            ret = EAI_SYSTEM;
            break;
        }

        srclen = sizeof(src);

        auto bytes = recvfrom(sock, resp, sizeof(resp), 0, (sockaddr*)&src, &srclen);
        if (bytes < 0) {
            print_stderr("recvfrom returned {} (errno {})\n", bytes, errno_to_string_view(*_errno()));
            ret = EAI_SYSTEM;
            break;
        }

        // FIXME - handle case when reply comes through from previous server we tried?

        if (src.sin6_family != AF_INET6)
            continue;

        if (*(ipv6_address*)&src.sin6_addr != *(ipv6_address*)&dest.sin6_addr)
            continue;

        if (src.sin6_port != dest.sin6_port)
            continue;

        if ((size_t)bytes < sizeof(dns_header))
            continue;

        auto& h2 = *(dns_header*)resp;

        // check transaction ID matches
        if (h2.id != h.id)
            continue;

        if (!h2.response) { // FIXME - wait and retry?
            ret = EAI_FAIL;
            break;
        }

        if (h2.qdcount != h.qdcount) { // FIXME - wait and retry?
            print_stderr("DNS server returned unexpected QDCOUNT {}\n", byteswap(h2.qdcount));
            ret = EAI_FAIL;
            break;
        }

        span sp((uint8_t*)resp + sizeof(dns_header), (size_t)bytes - sizeof(dns_header));

        if (sp.size() < question_len || memcmp(sp.data(), pack + sizeof(dns_header), question_len)) { // FIXME - wait and retry?
            print_stderr("DNS server did not return question\n");
            ret = EAI_FAIL;
            break;
        }

        if (h2.rcode == DNS_NXDOMAIN) {
            ret = EAI_NONAME;
            break;
        } else if (h2.rcode == DNS_FORMERR || h2.rcode == DNS_NOTIMP || h2.rcode > DNS_REFUSED) {
            ret = EAI_FAIL;
            break;
        } else if (h2.rcode == DNS_SERVFAIL || h2.rcode == DNS_REFUSED) {
            ret = EAI_AGAIN;
            break;
        }

        if (h2.ancount == 0) { // no matching records
            // Linux returns non-standard EAI_ADDRFAMILY here
            ret = EAI_NONAME;
            break;
        }

        sp = span(sp.data() + question_len, sp.size() - question_len);

        auto num_answers = byteswap(h2.ancount);

        ret = parse_dns_response(sp, span((uint8_t*)resp, (size_t)bytes),
                                 num_answers, suf.type,
                                 span(pack + sizeof(dns_header), name_len),
                                 res);

        if (ret) {
            while (*res) {
                auto next = (*res)->ai_next;

                free((*res)->ai_addr);
                free(*res);
                *res = next;
            }
            break;
        }

        if (*res) {
            for (auto* ai = *res; ai; ai = ai->ai_next) {
                if (type == DNS_A)
                    ((sockaddr_in*)ai->ai_addr)->sin_port = byteswap(port);
                else if (type == DNS_AAAA)
                    ((sockaddr_in6*)ai->ai_addr)->sin6_port = byteswap(port);

                ai->ai_socktype = socktype;
                ai->ai_protocol = protocol;

                if (last_entry)
                    *last_entry = ai;
            }

            ret = 0;
            break;
        }
    }

    free(pack);

    return ret;
}

static int rearrange_list(addrinfo** res) {
    unique_fd sock4, sock6;
    addrinfo* first_good = nullptr;
    addrinfo* last_good = nullptr;
    addrinfo* first_bad = nullptr;
    addrinfo* last_bad = nullptr;

    while (*res) {
        auto* ai = *res;
        auto next = ai->ai_next;
        int ret;

        if (ai->ai_family == AF_INET) {
            if (!sock4) {
                if (auto fd = socket(AF_INET, SOCK_DGRAM, 0); fd < 0) {
                    print_stderr("socket returned {} (errno {})\n", fd, errno_to_string_view(*_errno()));
                    freeaddrinfo(first_good);
                    freeaddrinfo(first_bad);
                    freeaddrinfo(*res);
                    return EAI_SYSTEM;
                } else
                    sock4.reset(fd);
            } else {
                sockaddr sa;

                sa.sa_family = AF_UNSPEC;

                connect(sock4.get(), &sa, sizeof(sa));
            }

            auto old_errno = *_errno();
            ret = connect(sock4.get(), ai->ai_addr, sizeof(sockaddr_in));
            *_errno() = old_errno;
        } else if (ai->ai_family == AF_INET6) {
            if (!sock6) {
                if (auto fd = socket(AF_INET6, SOCK_DGRAM, 0); fd < 0) {
                    print_stderr("socket returned {} (errno {})\n", fd, errno_to_string_view(*_errno()));
                    freeaddrinfo(first_good);
                    freeaddrinfo(first_bad);
                    freeaddrinfo(*res);
                    return EAI_SYSTEM;
                } else
                    sock6.reset(fd);
            } else {
                sockaddr sa;

                sa.sa_family = AF_UNSPEC;

                connect(sock6.get(), &sa, sizeof(sa));
            }

            auto old_errno = *_errno();
            ret = connect(sock6.get(), ai->ai_addr, sizeof(sockaddr_in6));
            *_errno() = old_errno;
        }

        ai->ai_next = nullptr;

        if (!ret) {
            if (last_good)
                last_good->ai_next = ai;
            else
                first_good = ai;

            last_good = ai;
        } else {
            if (last_bad)
                last_bad->ai_next = ai;
            else
                first_bad = ai;

            last_bad = ai;
        }

        *res = next;
    }

    if (!first_bad) {
        *res = first_good;
        return 0;
    }

    if (!first_good) {
        *res = first_bad;
        return 0;
    }

    last_good->ai_next = first_bad;
    *res = first_good;

    return 0;
}

static int dns_lookup_static(string_view name, addrinfo** res, int socktype,
                             int protocol, uint16_t port, int family) {
    static const string_view localhost = "localhost";

    if (name.size() != localhost.size() || strncasecmp(name.data(), localhost.data(), localhost.size()))
        return EAI_NONAME;

    switch (family) {
        case AF_INET: {
            auto ret = (addrinfo*)malloc(sizeof(addrinfo));
            if (!ret)
                return EAI_MEMORY;

            auto sock = (sockaddr_in*)malloc(sizeof(sockaddr_in));
            if (!sock) {
                free(ret);
                return EAI_MEMORY;
            }

            sock->sin_family = AF_INET;
            sock->sin_port = byteswap(port);
            *(ipv4_address*)&sock->sin_addr = parse_ipv4_address("127.0.0.1");
            memset(sock->sin_zero, 0, sizeof(sock->sin_zero));

            ret->ai_flags = 0;
            ret->ai_family = AF_INET;
            ret->ai_socktype = socktype;
            ret->ai_protocol = protocol;
            ret->ai_addrlen = sizeof(sockaddr_in);
            ret->ai_addr = (sockaddr*)sock;
            ret->ai_canonname = nullptr;
            ret->ai_next = nullptr;

            *res = ret;

            return 0;
        }

        case AF_INET6: {
            auto ret = (addrinfo*)malloc(sizeof(addrinfo));
            if (!ret)
                return EAI_MEMORY;

            auto sock = (sockaddr_in6*)malloc(sizeof(sockaddr_in6));
            if (!sock) {
                free(ret);
                return EAI_MEMORY;
            }

            sock->sin6_family = AF_INET6;
            sock->sin6_port = byteswap(port);
            sock->sin6_flowinfo = 0;
            *(ipv6_address*)&sock->sin6_addr = parse_ipv6_address("::1");
            sock->sin6_scope_id = 0;

            ret->ai_flags = 0;
            ret->ai_family = AF_INET6;
            ret->ai_socktype = socktype;
            ret->ai_protocol = protocol;
            ret->ai_addrlen = sizeof(sockaddr_in6);
            ret->ai_addr = (sockaddr*)sock;
            ret->ai_canonname = nullptr;
            ret->ai_next = nullptr;

            *res = ret;

            return 0;
        }

        case AF_UNSPEC: {
            auto ret1 = (addrinfo*)malloc(sizeof(addrinfo));
            if (!ret1)
                return EAI_MEMORY;

            auto sock1 = (sockaddr_in6*)malloc(sizeof(sockaddr_in6));
            if (!sock1) {
                free(ret1);
                return EAI_MEMORY;
            }

            auto ret2 = (addrinfo*)malloc(sizeof(addrinfo));
            if (!ret2) {
                free(sock1);
                free(ret1);
                return EAI_MEMORY;
            }

            auto sock2 = (sockaddr_in*)malloc(sizeof(sockaddr_in));
            if (!sock2) {
                free(ret2);
                free(sock1);
                free(ret1);
                return EAI_MEMORY;
            }

            sock1->sin6_family = AF_INET6;
            sock1->sin6_port = byteswap(port);
            sock1->sin6_flowinfo = 0;
            *(ipv6_address*)&sock1->sin6_addr = parse_ipv6_address("::1");
            sock1->sin6_scope_id = 0;

            ret1->ai_flags = 0;
            ret1->ai_family = AF_INET6;
            ret1->ai_socktype = socktype;
            ret1->ai_protocol = protocol;
            ret1->ai_addrlen = sizeof(sockaddr_in6);
            ret1->ai_addr = (sockaddr*)sock1;
            ret1->ai_canonname = nullptr;
            ret1->ai_next = ret2;

            sock2->sin_family = AF_INET;
            sock2->sin_port = byteswap(port);
            *(ipv4_address*)&sock2->sin_addr = parse_ipv4_address("127.0.0.1");
            memset(sock2->sin_zero, 0, sizeof(sock2->sin_zero));

            ret2->ai_flags = 0;
            ret2->ai_family = AF_INET;
            ret2->ai_socktype = socktype;
            ret2->ai_protocol = protocol;
            ret2->ai_addrlen = sizeof(sockaddr_in);
            ret2->ai_addr = (sockaddr*)sock2;
            ret2->ai_canonname = nullptr;
            ret2->ai_next = nullptr;

            *res = ret1;

            return 0;
        }

        default:
            unreachable();
    }
}

static int dns_lookup(string_view name, addrinfo** res, int socktype,
                      int protocol, uint16_t port, int family) {
    if (auto ret = dns_lookup_static(name, res, socktype, protocol, port, family); ret != EAI_NONAME)
        return ret;

    auto dns_servers = get_dns_servers();
    auto num_servers = dns_servers.size();

    for (size_t i = 0; i < dns_servers.size(); i++) {
        if (dns_servers[i] == parse_ipv6_address("::")) {
            num_servers = i;
            break;
        }
    }

    if (num_servers == 0)
        return EAI_AGAIN;

    unique_fd sock6;

    bool all_eai_again = true;

    for (size_t i = 0; i < num_servers; i++) {
        if (!sock6) {
            if (auto fd = socket(AF_INET6, SOCK_DGRAM, 0); fd < 0) {
                print_stderr("socket returned {} (errno {})\n", fd, errno_to_string_view(*_errno()));
                return EAI_SYSTEM;
            } else
                sock6.reset(fd);
        }

        int ret;

        if (family == AF_UNSPEC) {
            addrinfo* res1 = nullptr;
            addrinfo* res2 = nullptr;
            addrinfo* last_entry;
            int ret1, ret2;

            ret1 = dns_lookup2(sock6.get(), dns_servers[i], name, &res1,
                               socktype, protocol, port, DNS_AAAA, &last_entry);

            if (ret1 == EAI_MEMORY || ret1 == EAI_SYSTEM)
                return ret1;
            else if (ret1 != EAI_NONAME && ret1 != 0) {
                if (ret1 != EAI_AGAIN)
                    all_eai_again = false;

                continue;
            }

            ret2 = dns_lookup2(sock6.get(), dns_servers[i], name, &res2,
                               socktype, protocol, port, DNS_A, nullptr);

            if (ret2 != EAI_NONAME && ret2 != 0)
                freeaddrinfo(res1);

            if (ret2 == EAI_MEMORY || ret2 == EAI_SYSTEM)
                return ret2;
            else if (ret2 != EAI_NONAME && ret2 != 0) {
                if (ret2 != EAI_AGAIN)
                    all_eai_again = false;

                continue;
            }

            // merge lists

            if (ret1 == 0) {
                *res = res1;

                if (ret2 == 0)
                    last_entry->ai_next = res2;
            } else if (ret2 == 0)
                *res = res2;

            if (ret1 == EAI_NONAME && ret2 == EAI_NONAME)
                return EAI_NONAME;

            return rearrange_list(res);
        } else {
            if (family == AF_INET) {
                ret = dns_lookup2(sock6.get(), dns_servers[i], name, res,
                                  socktype, protocol, port, DNS_A, nullptr);
            } else if (family == AF_INET6) {
                ret = dns_lookup2(sock6.get(), dns_servers[i], name, res,
                                  socktype, protocol, port, DNS_AAAA, nullptr);
            }

            if (ret != EAI_AGAIN)
                all_eai_again = false;

            if (ret == 0)
                return rearrange_list(res);

            if (ret == EAI_NONAME || ret == EAI_MEMORY || ret == EAI_SYSTEM)
                return ret;
        }

    }

    if (all_eai_again)
        return EAI_AGAIN;
    else
        return EAI_FAIL;
}

extern "C"
int getaddrinfo(const char* node, const char* service, const addrinfo* hints,
                addrinfo** res) {
    int socktype, protocol, family, flags;
    uint16_t port;

    static const int understood_flags = AI_PASSIVE | AI_CANONNAME | AI_NUMERICHOST |
                                        AI_V4MAPPED | AI_ALL | AI_ADDRCONFIG |
                                        AI_NUMERICSERV;

    // FIXME - AI_CANONNAME
    // FIXME - AI_V4MAPPED
    // FIXME - AI_ALL
    // FIXME - AI_ADDRCONFIG
    // FIXME - AI_NUMERICSERV

    if (!hints) {
        socktype = 0;
        protocol = 0;
        family = AF_UNSPEC;
        flags = AI_V4MAPPED | AI_ADDRCONFIG;
    } else {
        socktype = hints->ai_socktype;
        protocol = hints->ai_protocol;
        family = hints->ai_family;
        flags = hints->ai_flags;
    }

    if (family != AF_UNSPEC && family != AF_INET && family != AF_INET6)
        return EAI_FAMILY;

    if (flags & ~understood_flags)
        return EAI_BADFLAGS;

    if (service) {
        if (auto opt = parse_service(service); opt.has_value())
            port = *opt;
        else
            return EAI_NONAME;
    } else
        port = 0;

    auto return_ipv4 = [socktype, protocol, res, port](const ipv4_address& addr) -> int {
        auto ret = (addrinfo*)malloc(sizeof(addrinfo));
        if (!ret)
            return EAI_MEMORY;

        auto sock = (sockaddr_in*)malloc(sizeof(sockaddr_in));
        if (!sock) {
            free(ret);
            return EAI_MEMORY;
        }

        sock->sin_family = AF_INET;
        sock->sin_port = byteswap(port);
        *(ipv4_address*)&sock->sin_addr = addr;
        memset(sock->sin_zero, 0, sizeof(sock->sin_zero));

        ret->ai_flags = 0;
        ret->ai_family = AF_INET;
        ret->ai_socktype = socktype;
        ret->ai_protocol = protocol;
        ret->ai_addrlen = sizeof(sockaddr_in);
        ret->ai_addr = (sockaddr*)sock;
        ret->ai_canonname = nullptr;
        ret->ai_next = nullptr;

        *res = ret;

        return 0;
    };

    auto return_ipv6 = [socktype, protocol, res, port](const ipv6_address& addr, uint32_t scope) -> int {
        auto ret = (addrinfo*)malloc(sizeof(addrinfo));
        if (!ret)
            return EAI_MEMORY;

        auto sock = (sockaddr_in6*)malloc(sizeof(sockaddr_in6));
        if (!sock) {
            free(ret);
            return EAI_MEMORY;
        }

        sock->sin6_family = AF_INET6;
        sock->sin6_port = byteswap(port);
        sock->sin6_flowinfo = 0;
        *(ipv6_address*)&sock->sin6_addr = addr;
        sock->sin6_scope_id = scope;

        ret->ai_flags = 0;
        ret->ai_family = AF_INET6;
        ret->ai_socktype = socktype;
        ret->ai_protocol = protocol;
        ret->ai_addrlen = sizeof(sockaddr_in6);
        ret->ai_addr = (sockaddr*)sock;
        ret->ai_canonname = nullptr;
        ret->ai_next = nullptr;

        *res = ret;

        return 0;
    };

    auto return_dual = [socktype, protocol, res, port](const ipv6_address& addr6, uint32_t scope, const ipv4_address& addr4) -> int {
        auto ret1 = (addrinfo*)malloc(sizeof(addrinfo));
        if (!ret1)
            return EAI_MEMORY;

        auto sock1 = (sockaddr_in6*)malloc(sizeof(sockaddr_in6));
        if (!sock1) {
            free(ret1);
            return EAI_MEMORY;
        }

        auto ret2 = (addrinfo*)malloc(sizeof(addrinfo));
        if (!ret2) {
            free(sock1);
            free(ret1);
            return EAI_MEMORY;
        }

        auto sock2 = (sockaddr_in*)malloc(sizeof(sockaddr_in));
        if (!sock2) {
            free(ret2);
            free(sock1);
            free(ret1);
            return EAI_MEMORY;
        }

        sock1->sin6_family = AF_INET6;
        sock1->sin6_port = byteswap(port);
        sock1->sin6_flowinfo = 0;
        *(ipv6_address*)&sock1->sin6_addr = addr6;
        sock1->sin6_scope_id = scope;

        ret1->ai_flags = 0;
        ret1->ai_family = AF_INET6;
        ret1->ai_socktype = socktype;
        ret1->ai_protocol = protocol;
        ret1->ai_addrlen = sizeof(sockaddr_in6);
        ret1->ai_addr = (sockaddr*)sock1;
        ret1->ai_canonname = nullptr;
        ret1->ai_next = ret2;

        sock2->sin_family = AF_INET;
        sock2->sin_port = byteswap(port);
        *(ipv4_address*)&sock2->sin_addr = addr4;
        memset(sock2->sin_zero, 0, sizeof(sock2->sin_zero));

        ret2->ai_flags = 0;
        ret2->ai_family = AF_INET;
        ret2->ai_socktype = socktype;
        ret2->ai_protocol = protocol;
        ret2->ai_addrlen = sizeof(sockaddr_in);
        ret2->ai_addr = (sockaddr*)sock2;
        ret2->ai_canonname = nullptr;
        ret2->ai_next = nullptr;

        *res = ret1;

        return 0;
    };

    if (!node) {
        switch (family) {
            case AF_INET:
                if (flags & AI_PASSIVE)
                    return return_ipv4(parse_ipv4_address("0.0.0.0"));
                else
                    return return_ipv4(parse_ipv4_address("127.0.0.1"));

            case AF_INET6:
                if (flags & AI_PASSIVE)
                    return return_ipv6(parse_ipv6_address("::"), 0);
                else
                    return return_ipv6(parse_ipv6_address("::1"), 0);

            case AF_UNSPEC:
                if (flags & AI_PASSIVE)
                    return return_dual(parse_ipv6_address("::"), 0, parse_ipv4_address("0.0.0.0"));
                else
                    return return_dual(parse_ipv6_address("::1"), 0, parse_ipv4_address("127.0.0.1"));
        }
    }

    string_view n = node;

    if (family == AF_UNSPEC || family == AF_INET6) {
        string_view name, scope;
        uint32_t scope_id = 0;

        if (auto pc = n.find('%'); pc != string_view::npos) {
            name = string_view(n.data(), pc);
            scope = string_view(n.data() + pc + 1, n.size() - pc - 1);
        } else
            name = n;

        if (!scope.empty()) {
            if (auto opt = parse_scope_id(scope); opt.has_value())
                scope_id = *opt;
            else
                return EAI_NONAME;
        }

        auto opt = try_parse_ipv6_address(name);

        if (opt.has_value())
            return return_ipv6(*opt, scope_id);
    }

    if (family == AF_UNSPEC || family == AF_INET) {
        auto opt = try_parse_ipv4_address(n);

        if (opt.has_value())
            return return_ipv4(*opt);
    }

    if (flags & AI_NUMERICHOST)
        return EAI_NONAME;

    return dns_lookup(n, res, socktype, protocol, port, family);
}

extern "C"
void freeaddrinfo(addrinfo* res) {
    while (res) {
        auto next = res->ai_next;

        if (res->ai_addr)
            free(res->ai_addr);

        if (res->ai_canonname)
            free(res->ai_canonname);

        free(res);
        res = next;
    }
}

extern "C"
const char* gai_strerror(int errcode) {
    switch (errcode) {
        case 0:
            return "Success";

        case EAI_BADFLAGS:
            return "Bad value for ai_flags";

        case EAI_NONAME:
            return "Name or service not known";

        case EAI_AGAIN:
            return "Temporary failure in name resolution";

        case EAI_FAIL:
            return "Non-recoverable failure in name resolution";

        case EAI_FAMILY:
            return "ai_family not supported";

        case EAI_SOCKTYPE:
            return "ai_socktype not supported";

        case EAI_SERVICE:
            return "Servname not supported for ai_socktype";

        case EAI_MEMORY:
            return "Memory allocation failure";

        case EAI_SYSTEM:
            return "System error";

        case EAI_OVERFLOW:
            return "Result too large for supplied buffer";

        default:
            return "?";
    }
}

static uint8_t hex_digit(uint8_t c) {
    if (c <= 9)
        return c + '0';
    else
        return c + 'a' - 10;
}

template<socket_family FAMILY>
static void ptr_name(const typename FAMILY::address& addr, uint8_t* buf) {
    if constexpr (is_same_v<FAMILY, ipv4>) {
        for (size_t i = 0; i < 4; i++) {
            auto b = addr.v[4 - i - 1];

            if (b >= 100) {
                buf[0] = 3;
                buf[1] = (b / 100) + '0';
                buf[2] = ((b % 100) / 10) + '0';
                buf[3] = (b % 10) + '0';

                buf += 4;
            } else if (b >= 10) {
                buf[0] = 2;
                buf[1] = (b / 10) + '0';
                buf[2] = (b % 10) + '0';

                buf += 3;
            } else {
                buf[0] = 1;
                buf[1] = b + '0';

                buf += 2;
            }
        }

        buf[0] = 7;
        buf[1] = 'i';
        buf[2] = 'n';
        buf[3] = '-';
        buf[4] = 'a';
        buf[5] = 'd';
        buf[6] = 'd';
        buf[7] = 'r';

        buf[8] = 4;
        buf[9] = 'a';
        buf[10] = 'r';
        buf[11] = 'p';
        buf[12] = 'a';

        buf[13] = 0;
    } else if constexpr (is_same_v<FAMILY, ipv6>) {
        for (size_t i = 0; i < 16; i++) {
            auto b = addr.v[16 - i - 1];

            buf[0] = 1;
            buf[1] = hex_digit(b & 0xf);
            buf[2] = 1;
            buf[3] = hex_digit((b & 0xf0) >> 4);

            buf += 4;
        }

        buf[0] = 3;
        buf[1] = 'i';
        buf[2] = 'p';
        buf[3] = '6';

        buf[4] = 4;
        buf[5] = 'a';
        buf[6] = 'r';
        buf[7] = 'p';
        buf[8] = 'a';

        buf[9] = 0;
    }
}

static size_t ipv4_ptr_len(const ipv4_address& addr) {
    size_t ret = 18;

    for (auto b : addr.v) {
        if (b >= 100)
            ret += 3;
        else if (b >= 10)
            ret += 2;
        else
            ret++;
    }

    return ret;
}

static int parse_dns_ptr_response(span<uint8_t> sp, span<uint8_t> resp,
                                  uint16_t num_answers, span<const char> name,
                                  span<char> buf) {
    for (uint16_t i = 0; i < num_answers; i++) {
        if (sp.empty())
            break;

        auto name2 = get_dns_name(sp, resp);
        if (name2.empty())
            break;

        if (sp.size() < sizeof(dns_resource_record))
            break;

        auto& rr = *(dns_resource_record*)sp.data();

        sp = span(sp.data() + sizeof(rr), sp.size() - sizeof(rr));

        if (sp.size() < byteswap(rr.rdlength))
            break;

        auto payload = span(sp.data(), byteswap(rr.rdlength));

        sp = span(sp.data() + payload.size(), sp.size() - payload.size());

        if (rr.dns_class != DNS_IN)
            continue;

        if (name2.size() != name.size() || memcmp(name2.data(), name.data(), name.size()))
            continue;

        if (rr.type != DNS_PTR)
            continue;

        bool first = true;

        while (!payload.empty()) {
            auto len = payload[0];

            if (len == 0) {
                if (buf.empty())
                    return EAI_OVERFLOW;

                buf[0] = 0;

                return 0;
            }

            if (!first) {
                if (buf.empty())
                    return EAI_OVERFLOW;

                buf[0] = '.';
                buf = span(buf.data() + 1, buf.size() - 1);
            }

            payload = span(payload.data() + 1, payload.size() - 1);

            if (payload.size() < len) // malformed
                break;

            if (buf.size() < len)
                return EAI_OVERFLOW;

            memcpy(buf.data(), payload.data(), len);

            buf = span(buf.data() + len, buf.size() - len);
            payload = span(payload.data() + len, payload.size() - len);

            first = false;
        }
    }

    return EAI_NONAME;
}

template<socket_family FAMILY>
static int reverse_dns_lookup2(int sock, const ipv6_address& server,
                               const typename FAMILY::address& addr, span<char> buf) {
    LARGE_INTEGER sent_time;
    int64_t deadline;
    nt_status status;
    sockaddr_in6 dest;
    int ret;

    memset(&dest, 0, sizeof(dest));

    dest.sin6_family = AF_INET6;
    dest.sin6_port = byteswap(DNS_PORT);
    *(ipv6_address*)&dest.sin6_addr = server;

    size_t reverse_len;

    if constexpr (is_same_v<FAMILY, ipv4>)
        reverse_len = ipv4_ptr_len(addr);
    else if constexpr (is_same_v<FAMILY, ipv6>)
        reverse_len = REVERSE_IPV6_LEN;

    size_t question_len = reverse_len + sizeof(dns_question_suffix);

    size_t packlen = sizeof(dns_header) + question_len;
    auto pack = (char*)malloc(packlen);
    if (!pack) {
        print("malloc failed\n");
        return EAI_MEMORY;
    }

    auto& h = *(dns_header*)pack;
    memset(&h, 0, sizeof(h));

    h.id = (uint16_t)rand();
    h.recursion_desired = 1;
    h.opcode = 0;
    h.response = 0;
    h.qdcount = byteswap((uint16_t)1);

    ptr_name<FAMILY>(addr, (uint8_t*)pack + sizeof(dns_header));

    auto& suf = *(dns_question_suffix*)(pack + sizeof(dns_header) + reverse_len);
    suf.type = DNS_PTR;
    suf.dns_class = DNS_IN;

    // FIXME - connect, so we get ICMP errors?

    auto sent = sendto(sock, pack, (int)packlen, 0, (struct sockaddr*)&dest,
                      sizeof(dest));
    if (sent < 0) {
        print_stderr("sendto returned {} (errno {})\n", sent, errno_to_string_view(*_errno()));
        free(pack);
        return EAI_SYSTEM;
    }

    // FIXME - can have larger packets with EDNS0
    // FIXME - TCP packets

    status = NtQuerySystemTime(&sent_time);
    if (!success(status)) {
        print_stderr("NtQuerySystemTime returned {}\n", status);
        free(pack);
        return EAI_SYSTEM;
    }

    deadline = sent_time.QuadPart + (DNS_TIMEOUT * SYSTEM_TIME_RESOLUTION);

    while (true) {
        char resp[512]; // maximum size of DNS packet
        LARGE_INTEGER now;
        pollfd pfd;
        sockaddr_in6 src;
        socklen_t srclen;

        status = NtQuerySystemTime(&now);
        if (!success(status)) {
            print_stderr("NtQuerySystemTime returned {}\n", status);
            ret = EAI_SYSTEM;
            break;
        }

        if (now.QuadPart >= deadline) {
            ret = EAI_AGAIN;
            break;
        }

        pfd.fd = sock;
        pfd.events = POLLIN;

        if (deadline <= now.QuadPart)
            ret = 0;
        else
            ret = poll(&pfd, 1, (int)((deadline - now.QuadPart) / 10000)); // convert units of 100ns to ms

        if (ret == 0) {
            ret = EAI_AGAIN;
            break;
        } else if (ret < 0) {
            print_stderr("poll failed (errno {})\n", errno_to_string_view(*_errno()));
            ret = EAI_SYSTEM;
            break;
        }

        srclen = sizeof(src);

        auto bytes = recvfrom(sock, resp, sizeof(resp), 0, (sockaddr*)&src,
                              &srclen);
        if (bytes < 0) {
            print_stderr("recvfrom returned {} (errno {})\n", bytes, errno_to_string_view(*_errno()));
            ret = EAI_SYSTEM;
            break;
        }

        // FIXME - handle case when reply comes through from previous server we tried?
        // FIXME - move duff servers to end of the list

        if (src.sin6_family != AF_INET6)
            continue;

        if (*(ipv6_address*)&src.sin6_addr != *(ipv6_address*)&dest.sin6_addr)
            continue;

        if (src.sin6_port != dest.sin6_port)
            continue;

        if ((size_t)bytes < sizeof(dns_header))
            continue;

        auto& h2 = *(dns_header*)resp;

        // check transaction ID matches
        if (h2.id != h.id)
            continue;

        if (!h2.response) { // FIXME - wait and retry?
            ret = EAI_FAIL;
            break;
        }

        if (h2.qdcount != h.qdcount) { // FIXME - wait and retry?
            print_stderr("DNS server returned unexpected QDCOUNT {}\n", byteswap(h2.qdcount));
            ret = EAI_FAIL;
            break;
        }

        span sp((uint8_t*)resp + sizeof(dns_header), (size_t)bytes - sizeof(dns_header));

        if (sp.size() < question_len || memcmp(sp.data(), pack + sizeof(dns_header), question_len)) { // FIXME - wait and retry?
            print_stderr("DNS server did not return question\n");
            ret = EAI_FAIL;
            break;
        }

        if (h2.rcode == DNS_NXDOMAIN) {
            ret = EAI_NONAME;
            break;
        } else if (h2.rcode == DNS_FORMERR || h2.rcode == DNS_NOTIMP || h2.rcode > DNS_REFUSED) {
            ret = EAI_FAIL;
            break;
        } else if (h2.rcode == DNS_SERVFAIL || h2.rcode == DNS_REFUSED) {
            ret = EAI_AGAIN;
            break;
        }

        if (h2.ancount == 0) { // no matching records
            // Linux returns non-standard EAI_ADDRFAMILY here
            ret = EAI_NONAME;
            break;
        }

        sp = span(sp.data() + question_len, sp.size() - question_len);

        auto num_answers = byteswap(h2.ancount);

        ret = parse_dns_ptr_response(sp, span((uint8_t*)resp, (size_t)bytes),
                                     num_answers,
                                     span(pack + sizeof(dns_header), reverse_len),
                                     buf);
        break;
    }

    free(pack);

    return ret;
}

template<socket_family FAMILY>
static int reverse_dns_lookup_static(const typename FAMILY::address& addr, span<char> buf) {
    static const string_view localhost = "localhost";

    if constexpr (is_same_v<FAMILY, ipv4>) {
        if (addr != parse_ipv4_address("127.0.0.1"))
            return EAI_NONAME;
    } else if constexpr (is_same_v<FAMILY, ipv6>) {
        if (addr != parse_ipv6_address("::1"))
            return EAI_NONAME;
    }

    if (buf.size() < localhost.size() + 1)
        return EAI_OVERFLOW;

    memcpy(buf.data(), localhost.data(), localhost.size());
    buf[localhost.size()] = 0;

    return 0;
}

template<socket_family FAMILY>
static int reverse_dns_lookup(const typename FAMILY::address& addr, span<char> buf) {
    if (auto ret = reverse_dns_lookup_static<FAMILY>(addr, buf); ret != EAI_NONAME)
        return ret;

    auto dns_servers = get_dns_servers();
    auto num_servers = dns_servers.size();

    for (size_t i = 0; i < dns_servers.size(); i++) {
        if (dns_servers[i] == parse_ipv6_address("::")) {
            num_servers = i;
            break;
        }
    }

    if (num_servers == 0)
        return EAI_AGAIN;

    unique_fd sock6;

    bool all_eai_again = true;

    for (size_t i = 0; i < num_servers; i++) {
        int ret;

        if (!sock6) {
            if (int fd = socket(AF_INET6, SOCK_DGRAM, 0); fd < 0) {
                print_stderr("socket returned {} (errno {})\n", fd, errno_to_string_view(*_errno()));
                return EAI_SYSTEM;
            } else
                sock6.reset(fd);
        }

        ret = reverse_dns_lookup2<FAMILY>(sock6.get(), dns_servers[i], addr,
                                          buf);

        if (ret != EAI_AGAIN)
            all_eai_again = false;

        if (ret != EAI_NONAME && ret != 0 && ret != EAI_MEMORY && ret != EAI_SYSTEM)
            continue;

        return ret;
    }

    if (all_eai_again)
        return EAI_AGAIN;
    else
        return EAI_FAIL;
}

static int getnameinfo_ipv6(const sockaddr* addr, socklen_t addrlen,
                            char* host, socklen_t hostlen,
                            char* serv, socklen_t servlen, int flags) {
    static const int understood_flags = NI_NUMERICHOST | NI_NUMERICSERV |
                                        NI_NAMEREQD;

    // FIXME - NI_NOFQDN, NI_NUMERICSCOPE, NI_DGRAM

    if (flags & ~understood_flags)
        return EAI_BADFLAGS;

    if ((size_t)addrlen < sizeof(sockaddr_in6))
        return EAI_FAMILY;

    if (!host && !serv)
        return EAI_NONAME;

    auto& a = *(sockaddr_in6*)addr;

    if (host) {
        bool done = false;

        if (hostlen < 0)
            return EAI_OVERFLOW;

        auto& ip = *(ipv6_address*)&a.sin6_addr;

        if (!(flags & NI_NUMERICHOST)) {
            auto ret = reverse_dns_lookup<ipv6>(ip, span(host, (size_t)hostlen));

            if (ret && (ret != EAI_NONAME || flags & NI_NAMEREQD))
                return ret;

            done = true;
        }

        if (!done) {
            char s[39];
            span sp(s, sizeof(s));

            sp = format(sp, "{}", ip);

            auto sv = string_view(s, (size_t)(sp.data() - s));

            if (sv.size() + 1 > (size_t)hostlen)
                return EAI_OVERFLOW;

            memcpy(host, sv.data(), sv.size());
            host[sv.size()] = 0;
        }
    }

    if (serv) {
        char s[5];
        span sp(s, sizeof(s));

        sp = format(sp, "{}", byteswap(a.sin6_port));

        auto sv = string_view(s, (size_t)(sp.data() - s));

        if (sv.size() + 1 > (size_t)servlen)
            return EAI_OVERFLOW;

        memcpy(serv, sv.data(), sv.size());
        serv[sv.size()] = 0;
    }

    return 0;
}

static int getnameinfo_ipv4(const sockaddr* addr, socklen_t addrlen,
                            char* host, socklen_t hostlen,
                            char* serv, socklen_t servlen, int flags) {
    static const int understood_flags = NI_NUMERICHOST | NI_NUMERICSERV |
                                        NI_NAMEREQD;

    // FIXME - NI_NOFQDN, NI_NUMERICSCOPE, NI_DGRAM

    if (flags & ~understood_flags)
        return EAI_BADFLAGS;

    if ((size_t)addrlen < sizeof(sockaddr_in))
        return EAI_FAMILY;

    if (!host && !serv)
        return EAI_NONAME;

    auto& a = *(sockaddr_in*)addr;

    if (host) {
        bool done = false;

        if (hostlen < 0)
            return EAI_OVERFLOW;

        auto& ip = *(ipv4_address*)&a.sin_addr;

        if (!(flags & NI_NUMERICHOST)) {
            auto ret = reverse_dns_lookup<ipv4>(ip, span(host, (size_t)hostlen));

            if (ret && (ret != EAI_NONAME || flags & NI_NAMEREQD))
                return ret;

            done = true;
        }

        if (!done) {
            char s[16];
            span sp(s, sizeof(s));

            sp = format(sp, "{}", ip);

            auto sv = string_view(s, (size_t)(sp.data() - s));

            if (sv.size() + 1 > (size_t)hostlen)
                return EAI_OVERFLOW;

            memcpy(host, sv.data(), sv.size());
            host[sv.size()] = 0;
        }
    }

    if (serv) {
        char s[5];
        span sp(s, sizeof(s));

        sp = format(sp, "{}", byteswap(a.sin_port));

        auto sv = string_view(s, (size_t)(sp.data() - s));

        if (sv.size() + 1 > (size_t)servlen)
            return EAI_OVERFLOW;

        memcpy(serv, sv.data(), sv.size());
        serv[sv.size()] = 0;
    }

    return 0;
}

extern "C"
int getnameinfo(const sockaddr* addr, socklen_t addrlen, char* host,
                socklen_t hostlen, char* serv, socklen_t servlen,
                int flags) {
    if ((size_t)addrlen < sizeof(sockaddr))
        return EAI_FAMILY;

    switch (addr->sa_family) {
        case AF_INET:
            return getnameinfo_ipv4(addr, addrlen, host, hostlen, serv,
                                    servlen, flags);

        case AF_INET6:
            return getnameinfo_ipv6(addr, addrlen, host, hostlen, serv,
                                    servlen, flags);

        default:
            return EAI_FAMILY;
    }
}
