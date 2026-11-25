// SPDX-License-Identifier: MIT
module;

#include <stdint.h>
#include <stddef.h>
#include <span>
#include <string_view>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

export module ws2_32;

import format;

using namespace std;

// netinet/in.h
export using ::in_addr;
export using ::sockaddr_in;
export using ::in6_addr;
export using ::sockaddr_in6;
export using ::in_addr_t;

// sys/socket.h
export using ::sa_family_t;
export using ::socklen_t;
export using ::sockaddr;
export using ::sockaddr_storage;
export using ::accept;
export using ::bind;
export using ::connect;
export using ::getpeername;
export using ::getsockname;
export using ::getsockopt;
export using ::listen;
export using ::recv;
export using ::recvfrom;
export using ::send;
export using ::sendto;
export using ::setsockopt;
export using ::socket;
export using ::linger;
export using ::shutdown;

// netdb.h
export using ::addrinfo;
export using ::freeaddrinfo;
export using ::gai_strerror;
export using ::getaddrinfo;
export using ::servent;
export using ::hostent;
export using ::getnameinfo;

// arpa/inet.h
export using ::htonl;
export using ::htons;
export using ::ntohl;
export using ::ntohs;
export using ::inet_addr;
export using ::inet_ntoa;
export using ::inet_ntop;
export using ::inet_pton;

constexpr uint16_t _AF_UNSPEC = AF_UNSPEC;
constexpr uint16_t _AF_UNIX = AF_UNIX;
constexpr uint16_t _AF_INET = AF_INET;
constexpr uint16_t _AF_INET6 = AF_INET6;

constexpr int _SOCK_STREAM = SOCK_STREAM;
constexpr int _SOCK_DGRAM = SOCK_DGRAM;
constexpr int _SOCK_RAW = SOCK_RAW;
constexpr int _SOCK_RDM = SOCK_RDM;
constexpr int _SOCK_SEQPACKET = SOCK_SEQPACKET;

constexpr uint8_t _IPPROTO_IP = IPPROTO_IP;
constexpr uint8_t _IPPROTO_HOPOPTS = IPPROTO_HOPOPTS;
constexpr uint8_t _IPPROTO_ICMP = IPPROTO_ICMP;
constexpr uint8_t _IPPROTO_IGMP = IPPROTO_IGMP;
constexpr uint8_t _IPPROTO_GGP = IPPROTO_GGP;
constexpr uint8_t _IPPROTO_IPV4 = IPPROTO_IPV4;
constexpr uint8_t _IPPROTO_ST = IPPROTO_ST;
constexpr uint8_t _IPPROTO_TCP = IPPROTO_TCP;
constexpr uint8_t _IPPROTO_CBT = IPPROTO_CBT;
constexpr uint8_t _IPPROTO_EGP = IPPROTO_EGP;
constexpr uint8_t _IPPROTO_IGP = IPPROTO_IGP;
constexpr uint8_t _IPPROTO_PUP = IPPROTO_PUP;
constexpr uint8_t _IPPROTO_UDP = IPPROTO_UDP;
constexpr uint8_t _IPPROTO_IDP = IPPROTO_IDP;
constexpr uint8_t _IPPROTO_RDP = IPPROTO_RDP;
constexpr uint8_t _IPPROTO_IPV6 = IPPROTO_IPV6;
constexpr uint8_t _IPPROTO_ROUTING = IPPROTO_ROUTING;
constexpr uint8_t _IPPROTO_FRAGMENT = IPPROTO_FRAGMENT;
constexpr uint8_t _IPPROTO_ESP = IPPROTO_ESP;
constexpr uint8_t _IPPROTO_AH = IPPROTO_AH;
constexpr uint8_t _IPPROTO_ICMPV6 = IPPROTO_ICMPV6;
constexpr uint8_t _IPPROTO_NONE = IPPROTO_NONE;
constexpr uint8_t _IPPROTO_DSTOPTS = IPPROTO_DSTOPTS;
constexpr uint8_t _IPPROTO_ND = IPPROTO_ND;
constexpr uint8_t _IPPROTO_ICLFXBM = IPPROTO_ICLFXBM;
constexpr uint8_t _IPPROTO_PIM = IPPROTO_PIM;
constexpr uint8_t _IPPROTO_PGM = IPPROTO_PGM;
constexpr uint8_t _IPPROTO_L2TP = IPPROTO_L2TP;
constexpr uint8_t _IPPROTO_SCTP = IPPROTO_SCTP;
constexpr uint8_t _IPPROTO_RAW = IPPROTO_RAW;

constexpr int _SOL_SOCKET = SOL_SOCKET;

constexpr int _SO_DEBUG = SO_DEBUG;
constexpr int _SO_ACCEPTCONN = SO_ACCEPTCONN;
constexpr int _SO_REUSEADDR = SO_REUSEADDR;
constexpr int _SO_KEEPALIVE = SO_KEEPALIVE;
constexpr int _SO_DONTROUTE = SO_DONTROUTE;
constexpr int _SO_BROADCAST = SO_BROADCAST;
constexpr int _SO_USELOOPBACK = SO_USELOOPBACK;
constexpr int _SO_LINGER = SO_LINGER;
constexpr int _SO_OOBINLINE = SO_OOBINLINE;
constexpr int _SO_SNDBUF = SO_SNDBUF;
constexpr int _SO_RCVBUF = SO_RCVBUF;
constexpr int _SO_SNDLOWAT = SO_SNDLOWAT;
constexpr int _SO_RCVLOWAT = SO_RCVLOWAT;
constexpr int _SO_SNDTIMEO = SO_SNDTIMEO;
constexpr int _SO_RCVTIMEO = SO_RCVTIMEO;
constexpr int _SO_ERROR = SO_ERROR;
constexpr int _SO_TYPE = SO_TYPE;

constexpr int _AI_PASSIVE = AI_PASSIVE;
constexpr int _AI_CANONNAME = AI_CANONNAME;
constexpr int _AI_NUMERICHOST = AI_NUMERICHOST;
constexpr int _AI_NUMERICSERV = AI_NUMERICSERV;
constexpr int _AI_ALL = AI_ALL;
constexpr int _AI_ADDRCONFIG = AI_ADDRCONFIG;
constexpr int _AI_V4MAPPED = AI_V4MAPPED;
constexpr int _AI_NON_AUTHORITATIVE = AI_NON_AUTHORITATIVE;
constexpr int _AI_SECURE = AI_SECURE;
constexpr int _AI_RETURN_PREFERRED_NAMES = AI_RETURN_PREFERRED_NAMES;
constexpr int _AI_FQDN = AI_FQDN;
constexpr int _AI_FILESERVER = AI_FILESERVER;
constexpr int _AI_DISABLE_IDN_ENCODING = AI_DISABLE_IDN_ENCODING;

constexpr int _EAI_BADFLAGS = EAI_BADFLAGS;
constexpr int _EAI_NONAME = EAI_NONAME;
constexpr int _EAI_AGAIN = EAI_AGAIN;
constexpr int _EAI_FAIL = EAI_FAIL;
constexpr int _EAI_FAMILY = EAI_FAMILY;
constexpr int _EAI_SOCKTYPE = EAI_SOCKTYPE;
constexpr int _EAI_SERVICE = EAI_SERVICE;
constexpr int _EAI_MEMORY = EAI_MEMORY;
constexpr int _EAI_SYSTEM = EAI_SYSTEM;
constexpr int _EAI_OVERFLOW = EAI_OVERFLOW;

constexpr int _NI_NOFQDN = NI_NOFQDN;
constexpr int _NI_NUMERICHOST = NI_NUMERICHOST;
constexpr int _NI_NAMEREQD = NI_NAMEREQD;
constexpr int _NI_NUMERICSERV = NI_NUMERICSERV;
constexpr int _NI_NUMERICSCOPE = NI_NUMERICSCOPE;
constexpr int _NI_DGRAM = NI_DGRAM;
constexpr size_t _NI_MAXHOST = NI_MAXHOST;
constexpr size_t _NI_MAXSERV = NI_MAXSERV;

constexpr int _MSG_PEEK = MSG_PEEK;
constexpr int _MSG_CTRUNC = MSG_CTRUNC;
constexpr int _MSG_TRUNC = MSG_TRUNC;
constexpr int _MSG_NOSIGNAL = MSG_NOSIGNAL;

constexpr in_addr_t _INADDR_ANY = INADDR_ANY;
constexpr in_addr_t _INADDR_BROADCAST = INADDR_BROADCAST;
constexpr in_addr_t _INADDR_LOOPBACK = INADDR_LOOPBACK;

constexpr int _TCP_NODELAY = TCP_NODELAY;
constexpr int _TCP_MAXSEG = TCP_MAXSEG;
constexpr int _TCP_CORK = TCP_CORK;
constexpr int _TCP_KEEPIDLE = TCP_KEEPIDLE;
constexpr int _TCP_KEEPINTVL = TCP_KEEPINTVL;
constexpr int _TCP_KEEPCNT = TCP_KEEPCNT;
constexpr int _TCP_SYNCNT = TCP_SYNCNT;
constexpr int _TCP_LINGER2 = TCP_LINGER2;
constexpr int _TCP_DEFER_ACCEPT = TCP_DEFER_ACCEPT;
constexpr int _TCP_WINDOW_CLAMP = TCP_WINDOW_CLAMP;
constexpr int _TCP_INFO = TCP_INFO;
constexpr int _TCP_QUICKACK = TCP_QUICKACK;
constexpr int _TCP_CONGESTION = TCP_CONGESTION;
constexpr int _TCP_MD5SIG = TCP_MD5SIG;
constexpr int _TCP_COOKIE_TRANSACTIONS = TCP_COOKIE_TRANSACTIONS;
constexpr int _TCP_THIN_LINEAR_TIMEOUTS = TCP_THIN_LINEAR_TIMEOUTS;
constexpr int _TCP_THIN_DUPACK = TCP_THIN_DUPACK;
constexpr int _TCP_USER_TIMEOUT = TCP_USER_TIMEOUT;
constexpr int _TCP_REPAIR = TCP_REPAIR;
constexpr int _TCP_REPAIR_QUEUE = TCP_REPAIR_QUEUE;
constexpr int _TCP_QUEUE_SEQ = TCP_QUEUE_SEQ;
constexpr int _TCP_REPAIR_OPTIONS = TCP_REPAIR_OPTIONS;
constexpr int _TCP_FASTOPEN = TCP_FASTOPEN;
constexpr int _TCP_TIMESTAMP = TCP_TIMESTAMP;
constexpr int _TCP_NOTSENT_LOWAT = TCP_NOTSENT_LOWAT;
constexpr int _TCP_CC_INFO = TCP_CC_INFO;
constexpr int _TCP_SAVE_SYN = TCP_SAVE_SYN;
constexpr int _TCP_SAVED_SYN = TCP_SAVED_SYN;
constexpr int _TCP_REPAIR_WINDOW = TCP_REPAIR_WINDOW;
constexpr int _TCP_FASTOPEN_CONNECT = TCP_FASTOPEN_CONNECT;
constexpr int _TCP_ULP = TCP_ULP;
constexpr int _TCP_MD5SIG_EXT = TCP_MD5SIG_EXT;
constexpr int _TCP_FASTOPEN_KEY = TCP_FASTOPEN_KEY;
constexpr int _TCP_FASTOPEN_NO_COOKIE = TCP_FASTOPEN_NO_COOKIE;
constexpr int _TCP_ZEROCOPY_RECEIVE = TCP_ZEROCOPY_RECEIVE;
constexpr int _TCP_INQ = TCP_INQ;
constexpr int _TCP_TX_DELAY = TCP_TX_DELAY;

constexpr int _SHUT_RD = SHUT_RD;
constexpr int _SHUT_WR = SHUT_WR;
constexpr int _SHUT_RDWR = SHUT_RDWR;

export {

using SOCKET = int;

#undef AF_UNSPEC
#undef AF_UNIX
#undef AF_INET
#undef AF_INET6

constexpr uint16_t AF_UNSPEC = _AF_UNSPEC;
constexpr uint16_t AF_UNIX = _AF_UNIX;
constexpr uint16_t AF_INET = _AF_INET;
constexpr uint16_t AF_INET6 = _AF_INET6;

#undef SOCK_STREAM
#undef SOCK_DGRAM
#undef SOCK_RAW
#undef SOCK_RDM
#undef SOCK_SEQPACKET

constexpr int SOCK_STREAM = _SOCK_STREAM;
constexpr int SOCK_DGRAM = _SOCK_DGRAM;
constexpr int SOCK_RAW = _SOCK_RAW;
constexpr int SOCK_RDM = _SOCK_RDM;
constexpr int SOCK_SEQPACKET = _SOCK_SEQPACKET;

#undef IPPROTO_IP
#undef IPPROTO_HOPOPTS
#undef IPPROTO_ICMP
#undef IPPROTO_IGMP
#undef IPPROTO_GGP
#undef IPPROTO_IPV4
#undef IPPROTO_ST
#undef IPPROTO_TCP
#undef IPPROTO_CBT
#undef IPPROTO_EGP
#undef IPPROTO_IGP
#undef IPPROTO_PUP
#undef IPPROTO_UDP
#undef IPPROTO_IDP
#undef IPPROTO_RDP
#undef IPPROTO_IPV6
#undef IPPROTO_ROUTING
#undef IPPROTO_FRAGMENT
#undef IPPROTO_ESP
#undef IPPROTO_AH
#undef IPPROTO_ICMPV6
#undef IPPROTO_NONE
#undef IPPROTO_DSTOPTS
#undef IPPROTO_ND
#undef IPPROTO_ICLFXBM
#undef IPPROTO_PIM
#undef IPPROTO_PGM
#undef IPPROTO_L2TP
#undef IPPROTO_SCTP
#undef IPPROTO_RAW

constexpr uint8_t IPPROTO_IP = _IPPROTO_IP;
constexpr uint8_t IPPROTO_HOPOPTS = _IPPROTO_HOPOPTS;
constexpr uint8_t IPPROTO_ICMP = _IPPROTO_ICMP;
constexpr uint8_t IPPROTO_IGMP = _IPPROTO_IGMP;
constexpr uint8_t IPPROTO_GGP = _IPPROTO_GGP;
constexpr uint8_t IPPROTO_IPV4 = _IPPROTO_IPV4;
constexpr uint8_t IPPROTO_ST = _IPPROTO_ST;
constexpr uint8_t IPPROTO_TCP = _IPPROTO_TCP;
constexpr uint8_t IPPROTO_CBT = _IPPROTO_CBT;
constexpr uint8_t IPPROTO_EGP = _IPPROTO_EGP;
constexpr uint8_t IPPROTO_IGP = _IPPROTO_IGP;
constexpr uint8_t IPPROTO_PUP = _IPPROTO_PUP;
constexpr uint8_t IPPROTO_UDP = _IPPROTO_UDP;
constexpr uint8_t IPPROTO_IDP = _IPPROTO_IDP;
constexpr uint8_t IPPROTO_RDP = _IPPROTO_RDP;
constexpr uint8_t IPPROTO_IPV6 = _IPPROTO_IPV6;
constexpr uint8_t IPPROTO_ROUTING = _IPPROTO_ROUTING;
constexpr uint8_t IPPROTO_FRAGMENT = _IPPROTO_FRAGMENT;
constexpr uint8_t IPPROTO_ESP = _IPPROTO_ESP;
constexpr uint8_t IPPROTO_AH = _IPPROTO_AH;
constexpr uint8_t IPPROTO_ICMPV6 = _IPPROTO_ICMPV6;
constexpr uint8_t IPPROTO_NONE = _IPPROTO_NONE;
constexpr uint8_t IPPROTO_DSTOPTS = _IPPROTO_DSTOPTS;
constexpr uint8_t IPPROTO_ND = _IPPROTO_ND;
constexpr uint8_t IPPROTO_ICLFXBM = _IPPROTO_ICLFXBM;
constexpr uint8_t IPPROTO_PIM = _IPPROTO_PIM;
constexpr uint8_t IPPROTO_PGM = _IPPROTO_PGM;
constexpr uint8_t IPPROTO_L2TP = _IPPROTO_L2TP;
constexpr uint8_t IPPROTO_SCTP = _IPPROTO_SCTP;
constexpr uint8_t IPPROTO_RAW = _IPPROTO_RAW;

#undef SOL_SOCKET
constexpr int SOL_SOCKET = _SOL_SOCKET;

const int INVALID_SOCKET = -1;
const int SOCKET_ERROR = -1;

#undef AI_PASSIVE
#undef AI_CANONNAME
#undef AI_NUMERICHOST
#undef AI_NUMERICSERV
#undef AI_ALL
#undef AI_ADDRCONFIG
#undef AI_V4MAPPED
#undef AI_NON_AUTHORITATIVE
#undef AI_SECURE
#undef AI_RETURN_PREFERRED_NAMES
#undef AI_FQDN
#undef AI_FILESERVER
#undef AI_DISABLE_IDN_ENCODING

constexpr int AI_PASSIVE = _AI_PASSIVE;
constexpr int AI_CANONNAME = _AI_CANONNAME;
constexpr int AI_NUMERICHOST = _AI_NUMERICHOST;
constexpr int AI_NUMERICSERV = _AI_NUMERICSERV;
constexpr int AI_ALL = _AI_ALL;
constexpr int AI_ADDRCONFIG = _AI_ADDRCONFIG;
constexpr int AI_V4MAPPED = _AI_V4MAPPED;
constexpr int AI_NON_AUTHORITATIVE = _AI_NON_AUTHORITATIVE;
constexpr int AI_SECURE = _AI_SECURE;
constexpr int AI_RETURN_PREFERRED_NAMES = _AI_RETURN_PREFERRED_NAMES;
constexpr int AI_FQDN = _AI_FQDN;
constexpr int AI_FILESERVER = _AI_FILESERVER;
constexpr int AI_DISABLE_IDN_ENCODING = _AI_DISABLE_IDN_ENCODING;

#undef EAI_BADFLAGS
#undef EAI_NONAME
#undef EAI_AGAIN
#undef EAI_FAIL
#undef EAI_FAMILY
#undef EAI_SOCKTYPE
#undef EAI_SERVICE
#undef EAI_MEMORY
#undef EAI_SYSTEM
#undef EAI_OVERFLOW

constexpr int EAI_BADFLAGS = _EAI_BADFLAGS;
constexpr int EAI_NONAME = _EAI_NONAME;
constexpr int EAI_AGAIN = _EAI_AGAIN;
constexpr int EAI_FAIL = _EAI_FAIL;
constexpr int EAI_FAMILY = _EAI_FAMILY;
constexpr int EAI_SOCKTYPE = _EAI_SOCKTYPE;
constexpr int EAI_SERVICE = _EAI_SERVICE;
constexpr int EAI_MEMORY = _EAI_MEMORY;
constexpr int EAI_SYSTEM = _EAI_SYSTEM;
constexpr int EAI_OVERFLOW = _EAI_OVERFLOW;

#undef SO_DEBUG
#undef SO_ACCEPTCONN
#undef SO_REUSEADDR
#undef SO_KEEPALIVE
#undef SO_DONTROUTE
#undef SO_BROADCAST
#undef SO_USELOOPBACK
#undef SO_LINGER
#undef SO_OOBINLINE
#undef SO_SNDBUF
#undef SO_RCVBUF
#undef SO_SNDLOWAT
#undef SO_RCVLOWAT
#undef SO_SNDTIMEO
#undef SO_RCVTIMEO
#undef SO_ERROR
#undef SO_TYPE

constexpr int SO_DEBUG = _SO_DEBUG;
constexpr int SO_ACCEPTCONN = _SO_ACCEPTCONN;
constexpr int SO_REUSEADDR = _SO_REUSEADDR;
constexpr int SO_KEEPALIVE = _SO_KEEPALIVE;
constexpr int SO_DONTROUTE = _SO_DONTROUTE;
constexpr int SO_BROADCAST = _SO_BROADCAST;
constexpr int SO_USELOOPBACK = _SO_USELOOPBACK;
constexpr int SO_LINGER = _SO_LINGER;
constexpr int SO_OOBINLINE = _SO_OOBINLINE;
constexpr int SO_SNDBUF = _SO_SNDBUF;
constexpr int SO_RCVBUF = _SO_RCVBUF;
constexpr int SO_SNDLOWAT = _SO_SNDLOWAT;
constexpr int SO_RCVLOWAT = _SO_RCVLOWAT;
constexpr int SO_SNDTIMEO = _SO_SNDTIMEO;
constexpr int SO_RCVTIMEO = _SO_RCVTIMEO;
constexpr int SO_ERROR = _SO_ERROR;
constexpr int SO_TYPE = _SO_TYPE;

#undef NI_NOFQDN
#undef NI_NUMERICHOST
#undef NI_NAMEREQD
#undef NI_NUMERICSERV
#undef NI_NUMERICSCOPE
#undef NI_DGRAM
#undef NI_MAXHOST
#undef NI_MAXSERV

constexpr int NI_NOFQDN = _NI_NOFQDN;
constexpr int NI_NUMERICHOST = _NI_NUMERICHOST;
constexpr int NI_NAMEREQD = _NI_NAMEREQD;
constexpr int NI_NUMERICSERV = _NI_NUMERICSERV;
constexpr int NI_NUMERICSCOPE = _NI_NUMERICSCOPE;
constexpr int NI_DGRAM = _NI_DGRAM;
constexpr size_t NI_MAXHOST = _NI_MAXHOST;
constexpr size_t NI_MAXSERV = _NI_MAXSERV;

#undef MSG_PEEK
#undef MSG_CTRUNC
#undef MSG_TRUNC
#undef MSG_NOSIGNAL

constexpr int MSG_PEEK = _MSG_PEEK;
constexpr int MSG_CTRUNC = _MSG_CTRUNC;
constexpr int MSG_TRUNC = _MSG_TRUNC;
constexpr int MSG_NOSIGNAL = _MSG_NOSIGNAL;

#undef INADDR_ANY
#undef INADDR_BROADCAST
#undef INADDR_LOOPBACK

constexpr in_addr_t INADDR_ANY = _INADDR_ANY;
constexpr in_addr_t INADDR_BROADCAST = _INADDR_BROADCAST;
constexpr in_addr_t INADDR_LOOPBACK = _INADDR_LOOPBACK;

#undef TCP_NODELAY
#undef TCP_MAXSEG
#undef TCP_CORK
#undef TCP_KEEPIDLE
#undef TCP_KEEPINTVL
#undef TCP_KEEPCNT
#undef TCP_SYNCNT
#undef TCP_LINGER2
#undef TCP_DEFER_ACCEPT
#undef TCP_WINDOW_CLAMP
#undef TCP_INFO
#undef TCP_QUICKACK
#undef TCP_CONGESTION
#undef TCP_MD5SIG
#undef TCP_COOKIE_TRANSACTIONS
#undef TCP_THIN_LINEAR_TIMEOUTS
#undef TCP_THIN_DUPACK
#undef TCP_USER_TIMEOUT
#undef TCP_REPAIR
#undef TCP_REPAIR_QUEUE
#undef TCP_QUEUE_SEQ
#undef TCP_REPAIR_OPTIONS
#undef TCP_FASTOPEN
#undef TCP_TIMESTAMP
#undef TCP_NOTSENT_LOWAT
#undef TCP_CC_INFO
#undef TCP_SAVE_SYN
#undef TCP_SAVED_SYN
#undef TCP_REPAIR_WINDOW
#undef TCP_FASTOPEN_CONNECT
#undef TCP_ULP
#undef TCP_MD5SIG_EXT
#undef TCP_FASTOPEN_KEY
#undef TCP_FASTOPEN_NO_COOKIE
#undef TCP_ZEROCOPY_RECEIVE
#undef TCP_INQ
#undef TCP_TX_DELAY

constexpr int TCP_NODELAY = _TCP_NODELAY;
constexpr int TCP_MAXSEG = _TCP_MAXSEG;
constexpr int TCP_CORK = _TCP_CORK;
constexpr int TCP_KEEPIDLE = _TCP_KEEPIDLE;
constexpr int TCP_KEEPINTVL = _TCP_KEEPINTVL;
constexpr int TCP_KEEPCNT = _TCP_KEEPCNT;
constexpr int TCP_SYNCNT = _TCP_SYNCNT;
constexpr int TCP_LINGER2 = _TCP_LINGER2;
constexpr int TCP_DEFER_ACCEPT = _TCP_DEFER_ACCEPT;
constexpr int TCP_WINDOW_CLAMP = _TCP_WINDOW_CLAMP;
constexpr int TCP_INFO = _TCP_INFO;
constexpr int TCP_QUICKACK = _TCP_QUICKACK;
constexpr int TCP_CONGESTION = _TCP_CONGESTION;
constexpr int TCP_MD5SIG = _TCP_MD5SIG;
constexpr int TCP_COOKIE_TRANSACTIONS = _TCP_COOKIE_TRANSACTIONS;
constexpr int TCP_THIN_LINEAR_TIMEOUTS = _TCP_THIN_LINEAR_TIMEOUTS;
constexpr int TCP_THIN_DUPACK = _TCP_THIN_DUPACK;
constexpr int TCP_USER_TIMEOUT = _TCP_USER_TIMEOUT;
constexpr int TCP_REPAIR = _TCP_REPAIR;
constexpr int TCP_REPAIR_QUEUE = _TCP_REPAIR_QUEUE;
constexpr int TCP_QUEUE_SEQ = _TCP_QUEUE_SEQ;
constexpr int TCP_REPAIR_OPTIONS = _TCP_REPAIR_OPTIONS;
constexpr int TCP_FASTOPEN = _TCP_FASTOPEN;
constexpr int TCP_TIMESTAMP = _TCP_TIMESTAMP;
constexpr int TCP_NOTSENT_LOWAT = _TCP_NOTSENT_LOWAT;
constexpr int TCP_CC_INFO = _TCP_CC_INFO;
constexpr int TCP_SAVE_SYN = _TCP_SAVE_SYN;
constexpr int TCP_SAVED_SYN = _TCP_SAVED_SYN;
constexpr int TCP_REPAIR_WINDOW = _TCP_REPAIR_WINDOW;
constexpr int TCP_FASTOPEN_CONNECT = _TCP_FASTOPEN_CONNECT;
constexpr int TCP_ULP = _TCP_ULP;
constexpr int TCP_MD5SIG_EXT = _TCP_MD5SIG_EXT;
constexpr int TCP_FASTOPEN_KEY = _TCP_FASTOPEN_KEY;
constexpr int TCP_FASTOPEN_NO_COOKIE = _TCP_FASTOPEN_NO_COOKIE;
constexpr int TCP_ZEROCOPY_RECEIVE = _TCP_ZEROCOPY_RECEIVE;
constexpr int TCP_INQ = _TCP_INQ;
constexpr int TCP_TX_DELAY = _TCP_TX_DELAY;

#undef SHUT_RD
#undef SHUT_WR
#undef SHUT_RDWR

constexpr int SHUT_RD = _SHUT_RD;
constexpr int SHUT_WR = _SHUT_WR;
constexpr int SHUT_RDWR = _SHUT_RDWR;

struct proto_def {
    const char* name;
    const char* alias1;
    const char* alias2;
    int num;
};

const proto_def protos[] = {
    { "ip", "IP", nullptr, 0 },
    { "hopopt", "HOPOPT", nullptr, 0 },
    { "icmp", "ICMP", nullptr, 1 },
    { "igmp", "IGMP", nullptr, 2 },
    { "ggp", "GGP", nullptr, 3 },
    { "ipencap", "IP-ENCAP", nullptr, 4 },
    { "st", "ST", nullptr, 5 },
    { "tcp", "TCP", nullptr, 6 },
    { "cbt", "CBT", nullptr, 7 },
    { "egp", "EGP", nullptr, 8 },
    { "igp", "IGP", nullptr, 9 },
    { "bbn-rcc", "BBN-RCC-MON", nullptr, 10 },
    { "nvp", "NVP-II", nullptr, 11 },
    { "pup", "PUP", nullptr, 12 },
    { "argus", "ARGUS", nullptr, 13 },
    { "emcon", "EMCON", nullptr, 14 },
    { "xnet", "XNET", nullptr, 15 },
    { "chaos", "CHAOS", nullptr, 16 },
    { "udp", "UDP", nullptr, 17 },
    { "mux", "MUX", nullptr, 18 },
    { "dcn", "DCN-MEAS", nullptr, 19 },
    { "hmp", "HMP", nullptr, 20 },
    { "prm", "PRM", nullptr, 21 },
    { "xns-idp", "XNS-IDP", nullptr, 22 },
    { "trunk-1", "TRUNK-1", nullptr, 23 },
    { "trunk-2", "TRUNK-2", nullptr, 24 },
    { "leaf-1", "LEAF-1", nullptr, 25 },
    { "leaf-2", "LEAF-2", nullptr, 26 },
    { "rdp", "RDP", nullptr, 27 },
    { "irtp", "IRTP", nullptr, 28 },
    { "iso-tp4", "ISO-TP4", nullptr, 29 },
    { "netblt", "NETBLT", nullptr, 30 },
    { "mfe-nsp", "MFE-NSP", nullptr, 31 },
    { "merit-inp", "MERIT-INP", nullptr, 32 },
    { "dccp", "DCCP", nullptr, 33 },
    { "3pc", "3PC", nullptr, 34 },
    { "idpr", "IDPR", nullptr, 35 },
    { "xtp", "XTP", nullptr, 36 },
    { "ddp", "DDP", nullptr, 37 },
    { "idpr-cmtp", "IDPR-CMTP", nullptr, 38 },
    { "tp++", "TP++", nullptr, 39 },
    { "il", "IL", nullptr, 40 },
    { "ipv6", "IPv6", nullptr, 41 },
    { "sdrp", "SDRP", nullptr, 42 },
    { "ipv6-route", "IPv6-Route", nullptr, 43 },
    { "ipv6-frag", "IPv6-Frag", nullptr, 44 },
    { "idrp", "IDRP", nullptr, 45 },
    { "rsvp", "RSVP", nullptr, 46 },
    { "gre", "GRE", nullptr, 47 },
    { "dsr", "DSR", nullptr, 48 },
    { "bna", "BNA", nullptr, 49 },
    { "esp", "ESP", nullptr, 50 },
    { "ah", "AH", nullptr, 51 },
    { "i-nlsp", "I-NLSP", nullptr, 52 },
    { "swipe", "SWIPE", nullptr, 53 },
    { "narp", "NARP", nullptr, 54 },
    { "mobile", "MOBILE", nullptr, 55 },
    { "tlsp", "TLSP", nullptr, 56 },
    { "skip", "SKIP", nullptr, 57 },
    { "ipv6-icmp", "IPv6-ICMP", nullptr, 58 },
    { "ipv6-nonxt", "IPv6-NoNxt", nullptr, 59 },
    { "ipv6-opts", "IPv6-Opts", nullptr, 60 },
    { "cftp", "CFTP", nullptr, 62 },
    { "sat-expak", "SAT-EXPAK", nullptr, 64 },
    { "kryptolan", "KRYPTOLAN", nullptr, 65 },
    { "rvd", "RVD", nullptr, 66 },
    { "ippc", "IPPC", nullptr, 67 },
    { "sat-mon", "SAT-MON", nullptr, 69 },
    { "visa", "VISA", nullptr, 70 },
    { "ipcv", "IPCV", nullptr, 71 },
    { "cpnx", "CPNX", nullptr, 72 },
    { "cphb", "CPHB", nullptr, 73 },
    { "wsn", "WSN", nullptr, 74 },
    { "pvp", "PVP", nullptr, 75 },
    { "br-sat-mon", "BR-SAT-MON", nullptr, 76 },
    { "sun-nd", "SUN-ND", nullptr, 77 },
    { "wb-mon", "WB-MON", nullptr, 78 },
    { "wb-expak", "WB-EXPAK", nullptr, 79 },
    { "iso-ip", "ISO-IP", nullptr, 80 },
    { "vmtp", "VMTP", nullptr, 81 },
    { "secure-vmtp", "SECURE-VMTP", nullptr, 82 },
    { "vines", "VINES", nullptr, 83 },
    { "ttp", "TTP", nullptr, 84 },
    { "nsfnet-igp", "NSFNET-IGP", nullptr, 85 },
    { "dgp", "DGP", nullptr, 86 },
    { "tcf", "TCF", nullptr, 87 },
    { "eigrp", "EIGRP", nullptr, 88 },
    { "ospf", "OSPFIGP", nullptr, 89 },
    { "sprite-rpc", "Sprite-RPC", nullptr, 90 },
    { "larp", "LARP", nullptr, 91 },
    { "mtp", "MTP", nullptr, 92 },
    { "ax.25", "AX.25", nullptr, 93 },
    { "ipip", "IPIP", nullptr, 94 },
    { "micp", "MICP", nullptr, 95 },
    { "scc-sp", "SCC-SP", nullptr, 96 },
    { "etherip", "ETHERIP", nullptr, 97 },
    { "encap", "ENCAP", nullptr, 98 },
    { "gmtp", "GMTP", nullptr, 100 },
    { "ifmp", "IFMP", nullptr, 101 },
    { "pnni", "PNNI", nullptr, 102 },
    { "pim", "PIM", nullptr, 103 },
    { "aris", "ARIS", nullptr, 104 },
    { "scps", "SCPS", nullptr, 105 },
    { "qnx", "QNX", nullptr, 106 },
    { "a/n", "A/N", nullptr, 107 },
    { "ipcomp", "IPComp", nullptr, 108 },
    { "snp", "SNP", nullptr, 109 },
    { "compaq-peer", "Compaq-Peer", nullptr, 110 },
    { "ipx-in-ip", "IPX-in-IP", nullptr, 111 },
    { "carp", "vrrp", "CARP", 112 },
    { "pgm", "PGM", nullptr, 113 },
    { "l2tp", "L2TP", nullptr, 115 },
    { "ddx", "DDX", nullptr, 116 },
    { "iatp", "IATP", nullptr, 117 },
    { "stp", "STP", nullptr, 118 },
    { "srp", "SRP", nullptr, 119 },
    { "uti", "UTI", nullptr, 120 },
    { "smp", "SMP", nullptr, 121 },
    { "sm", "SM", nullptr, 122 },
    { "ptp", "PTP", nullptr, 123 },
    { "isis", "ISIS", nullptr, 124 },
    { "fire", "FIRE", nullptr, 125 },
    { "crtp", "CRTP", nullptr, 126 },
    { "crudp", "CRUDP", nullptr, 127 },
    { "sscopmce", "SSCOPMCE", nullptr, 128 },
    { "iplt", "IPLT", nullptr, 129 },
    { "sps", "SPS", nullptr, 130 },
    { "pipe", "PIPE", nullptr, 131 },
    { "sctp", "SCTP", nullptr, 132 },
    { "fc", "FC", nullptr, 133 },
    { "rsvp-e2e-ignore", "RSVP-E2E-IGNORE", nullptr, 134 },
    { "mobility-header", "Mobility-Header", nullptr, 135 },
    { "udplite", "UDPLite", nullptr, 136 },
    { "mpls-in-ip", "MPLS-in-IP", nullptr, 137 },
    { "manet", "manet", nullptr, 138 },
    { "hip", "HIP", nullptr, 139 },
    { "shim6", "Shim6", nullptr, 140 },
    { "pfsync", "PFSYNC", nullptr, 240 },
    { "divert", "DIVERT", nullptr, 258 },
};

enum class sockopt_socket {
    DEBUG = SO_DEBUG,
    ACCEPTCONN = SO_ACCEPTCONN,
    REUSEADDR = SO_REUSEADDR,
    KEEPALIVE = SO_KEEPALIVE,
    DONTROUTE = SO_DONTROUTE,
    BROADCAST = SO_BROADCAST,
    USELOOPBACK = SO_USELOOPBACK,
    LINGER = SO_LINGER,
    OOBINLINE = SO_OOBINLINE,
    SNDBUF = SO_SNDBUF,
    RCVBUF = SO_RCVBUF,
    SNDLOWAT = SO_SNDLOWAT,
    RCVLOWAT = SO_RCVLOWAT,
    SNDTIMEO = SO_SNDTIMEO,
    RCVTIMEO = SO_RCVTIMEO,
    ERROR = SO_ERROR,
    TYPE = SO_TYPE,
};

enum class sockopt_tcp {
    NODELAY = TCP_NODELAY,
    MAXSEG = TCP_MAXSEG,
    CORK = TCP_CORK,
    KEEPIDLE = TCP_KEEPIDLE,
    KEEPINTVL = TCP_KEEPINTVL,
    KEEPCNT = TCP_KEEPCNT,
    SYNCNT = TCP_SYNCNT,
    LINGER2 = TCP_LINGER2,
    DEFER_ACCEPT = TCP_DEFER_ACCEPT,
    WINDOW_CLAMP = TCP_WINDOW_CLAMP,
    INFO = TCP_INFO,
    QUICKACK = TCP_QUICKACK,
    CONGESTION = TCP_CONGESTION,
    MD5SIG = TCP_MD5SIG,
    COOKIE_TRANSACTIONS = TCP_COOKIE_TRANSACTIONS,
    THIN_LINEAR_TIMEOUTS = TCP_THIN_LINEAR_TIMEOUTS,
    THIN_DUPACK = TCP_THIN_DUPACK,
    USER_TIMEOUT = TCP_USER_TIMEOUT,
    REPAIR = TCP_REPAIR,
    REPAIR_QUEUE = TCP_REPAIR_QUEUE,
    QUEUE_SEQ = TCP_QUEUE_SEQ,
    REPAIR_OPTIONS = TCP_REPAIR_OPTIONS,
    FASTOPEN = TCP_FASTOPEN,
    TIMESTAMP = TCP_TIMESTAMP,
    NOTSENT_LOWAT = TCP_NOTSENT_LOWAT,
    CC_INFO = TCP_CC_INFO,
    SAVE_SYN = TCP_SAVE_SYN,
    SAVED_SYN = TCP_SAVED_SYN,
    REPAIR_WINDOW = TCP_REPAIR_WINDOW,
    FASTOPEN_CONNECT = TCP_FASTOPEN_CONNECT,
    ULP = TCP_ULP,
    MD5SIG_EXT = TCP_MD5SIG_EXT,
    FASTOPEN_KEY = TCP_FASTOPEN_KEY,
    FASTOPEN_NO_COOKIE = TCP_FASTOPEN_NO_COOKIE,
    ZEROCOPY_RECEIVE = TCP_ZEROCOPY_RECEIVE,
    INQ = TCP_INQ,
    TX_DELAY = TCP_TX_DELAY,
};

enum class sockopt_ipv6 {
    UNICAST_HOPS = IPV6_UNICAST_HOPS,
    MULTICAST_IF = IPV6_MULTICAST_IF,
    MULTICAST_HOPS = IPV6_MULTICAST_HOPS,
    MULTICAST_LOOP = IPV6_MULTICAST_LOOP,
    JOIN_GROUP = IPV6_JOIN_GROUP,
    LEAVE_GROUP = IPV6_LEAVE_GROUP,
    V6ONLY = IPV6_V6ONLY,
};

template<>
consteval bool format_spec_valid<sockopt_socket>(string_view fmt) {
    return fmt.empty();
}

span<char> do_format_arg(span<char> sp, string_view, sockopt_socket o) {
    switch (o) {
        using enum sockopt_socket;

        case DEBUG:
            return do_format_arg(sp, "", "DEBUG");
        case ACCEPTCONN:
            return do_format_arg(sp, "", "ACCEPTCONN");
        case REUSEADDR:
            return do_format_arg(sp, "", "REUSEADDR");
        case KEEPALIVE:
            return do_format_arg(sp, "", "KEEPALIVE");
        case DONTROUTE:
            return do_format_arg(sp, "", "DONTROUTE");
        case BROADCAST:
            return do_format_arg(sp, "", "BROADCAST");
        case USELOOPBACK:
            return do_format_arg(sp, "", "USELOOPBACK");
        case LINGER:
            return do_format_arg(sp, "", "LINGER");
        case OOBINLINE:
            return do_format_arg(sp, "", "OOBINLINE");
        case SNDBUF:
            return do_format_arg(sp, "", "SNDBUF");
        case RCVBUF:
            return do_format_arg(sp, "", "RCVBUF");
        case SNDLOWAT:
            return do_format_arg(sp, "", "SNDLOWAT");
        case RCVLOWAT:
            return do_format_arg(sp, "", "RCVLOWAT");
        case SNDTIMEO:
            return do_format_arg(sp, "", "SNDTIMEO");
        case RCVTIMEO:
            return do_format_arg(sp, "", "RCVTIMEO");
        case ERROR:
            return do_format_arg(sp, "", "ERROR");
        case TYPE:
            return do_format_arg(sp, "", "TYPE");
        default:
            return do_format_arg(sp, "", (unsigned int)o);
    }

    return sp;
}

template<>
consteval bool format_spec_valid<sockopt_tcp>(string_view fmt) {
    return fmt.empty();
}

span<char> do_format_arg(span<char> sp, string_view, sockopt_tcp o) {
    switch (o) {
        using enum sockopt_tcp;

        case NODELAY:
            return do_format_arg(sp, "", "NODELAY");
        case MAXSEG:
            return do_format_arg(sp, "", "MAXSEG");
        case CORK:
            return do_format_arg(sp, "", "CORK");
        case KEEPIDLE:
            return do_format_arg(sp, "", "KEEPIDLE");
        case KEEPINTVL:
            return do_format_arg(sp, "", "KEEPINTVL");
        case KEEPCNT:
            return do_format_arg(sp, "", "KEEPCNT");
        case SYNCNT:
            return do_format_arg(sp, "", "SYNCNT");
        case LINGER2:
            return do_format_arg(sp, "", "LINGER2");
        case DEFER_ACCEPT:
            return do_format_arg(sp, "", "DEFER_ACCEPT");
        case WINDOW_CLAMP:
            return do_format_arg(sp, "", "WINDOW_CLAMP");
        case INFO:
            return do_format_arg(sp, "", "INFO");
        case QUICKACK:
            return do_format_arg(sp, "", "QUICKACK");
        case CONGESTION:
            return do_format_arg(sp, "", "CONGESTION");
        case MD5SIG:
            return do_format_arg(sp, "", "MD5SIG");
        case COOKIE_TRANSACTIONS:
            return do_format_arg(sp, "", "COOKIE_TRANSACTIONS");
        case THIN_LINEAR_TIMEOUTS:
            return do_format_arg(sp, "", "THIN_LINEAR_TIMEOUTS");
        case THIN_DUPACK:
            return do_format_arg(sp, "", "THIN_DUPACK");
        case USER_TIMEOUT:
            return do_format_arg(sp, "", "USER_TIMEOUT");
        case REPAIR:
            return do_format_arg(sp, "", "REPAIR");
        case REPAIR_QUEUE:
            return do_format_arg(sp, "", "REPAIR_QUEUE");
        case QUEUE_SEQ:
            return do_format_arg(sp, "", "QUEUE_SEQ");
        case REPAIR_OPTIONS:
            return do_format_arg(sp, "", "REPAIR_OPTIONS");
        case FASTOPEN:
            return do_format_arg(sp, "", "FASTOPEN");
        case TIMESTAMP:
            return do_format_arg(sp, "", "TIMESTAMP");
        case NOTSENT_LOWAT:
            return do_format_arg(sp, "", "NOTSENT_LOWAT");
        case CC_INFO:
            return do_format_arg(sp, "", "CC_INFO");
        case SAVE_SYN:
            return do_format_arg(sp, "", "SAVE_SYN");
        case SAVED_SYN:
            return do_format_arg(sp, "", "SAVED_SYN");
        case REPAIR_WINDOW:
            return do_format_arg(sp, "", "REPAIR_WINDOW");
        case FASTOPEN_CONNECT:
            return do_format_arg(sp, "", "FASTOPEN_CONNECT");
        case ULP:
            return do_format_arg(sp, "", "ULP");
        case MD5SIG_EXT:
            return do_format_arg(sp, "", "MD5SIG_EXT");
        case FASTOPEN_KEY:
            return do_format_arg(sp, "", "FASTOPEN_KEY");
        case FASTOPEN_NO_COOKIE:
            return do_format_arg(sp, "", "FASTOPEN_NO_COOKIE");
        case ZEROCOPY_RECEIVE:
            return do_format_arg(sp, "", "ZEROCOPY_RECEIVE");
        case INQ:
            return do_format_arg(sp, "", "INQ");
        case TX_DELAY:
            return do_format_arg(sp, "", "TX_DELAY");
        default:
            return do_format_arg(sp, "", (unsigned int)o);
    }

    return sp;
}

template<>
consteval bool format_spec_valid<sockopt_ipv6>(string_view fmt) {
    return fmt.empty();
}

span<char> do_format_arg(span<char> sp, string_view, sockopt_ipv6 o) {
    switch (o) {
        using enum sockopt_ipv6;

        case UNICAST_HOPS:
            return do_format_arg(sp, "", "UNICAST_HOPS");
        case MULTICAST_IF:
            return do_format_arg(sp, "", "MULTICAST_IF");
        case MULTICAST_HOPS:
            return do_format_arg(sp, "", "MULTICAST_HOPS");
        case MULTICAST_LOOP:
            return do_format_arg(sp, "", "MULTICAST_LOOP");
        case JOIN_GROUP:
            return do_format_arg(sp, "", "JOIN_GROUP");
        case LEAVE_GROUP:
            return do_format_arg(sp, "", "LEAVE_GROUP");
        case V6ONLY:
            return do_format_arg(sp, "", "V6ONLY");
        default:
            return do_format_arg(sp, "", (unsigned int)o);
    }

    return sp;
}

extern "C" {

int closesocket(SOCKET s);

}

}
