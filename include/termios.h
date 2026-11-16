// SPDX-License-Identifier: MIT
#pragma once

#include <sys/types.h>

typedef unsigned int tcflag_t;
typedef unsigned char cc_t;
typedef unsigned int speed_t;

#define NCCS 32

struct termios {
    tcflag_t c_iflag;
    tcflag_t c_oflag;
    tcflag_t c_cflag;
    tcflag_t c_lflag;
    cc_t c_line;
    cc_t c_cc[NCCS];
    speed_t c_ispeed;
    speed_t c_ospeed;
};

/* c_iflag */
#define IGNBRK  0x0001
#define BRKINT  0x0002
#define IGNPAR  0x0004
#define PARMRK  0x0008
#define INPCK   0x0010
#define ISTRIP  0x0020
#define INLCR   0x0040
#define IGNCR   0x0080
#define ICRNL   0x0100
#define IUCLC   0x0200
#define IXON    0x0400
#define IXANY   0x0800
#define IXOFF   0x1000
#define IMAXBEL 0x2000
#define IUTF8   0x4000

/* c_oflag */
#define OPOST   0x0001
#define OLCUC   0x0002
#define ONLCR   0x0004
#define OCRNL   0x0008
#define ONOCR   0x0010
#define ONLRET  0x0020
#define OFILL   0x0040
#define OFDEL   0x0080
#define NLDLY   0x0100
#define NL0     0x0000
#define NL1     0x0100
#define CRDLY   0x0600
#define CR0     0x0000
#define CR1     0x0200
#define CR2     0x0400
#define CR3     0x0600
#define TABDLY  0x1800
#define TAB0    0x0000
#define TAB1    0x0800
#define TAB2    0x1000
#define TAB3    0x1800
#define BSDLY   0x2000
#define BS0     0x0000
#define BS1     0x2000
#define VTDLY   0x4000
#define VT0     0x0000
#define VT1     0x4000
#define FFDLY   0x8000
#define FF0     0x0000
#define FF1     0x8000

/* c_cflag */
#define CSIZE   0x0030
#define CS5     0x0000
#define CS6     0x0010
#define CS7     0x0020
#define CS8     0x0030
#define CSTOPB  0x0040
#define CREAD   0x0080
#define PARENB  0x0100
#define PARODD  0x0200
#define HUPCL   0x0400
#define CLOCAL  0x0800
#define CBAUD   0x100f
#define CRTSCTS 0x80000000

/* c_lflag */
#define ISIG    0x0001
#define ICANON  0x0002
#define XCASE   0x0004
#define ECHO    0x0008
#define ECHOE   0x0010
#define ECHOK   0x0020
#define ECHONL  0x0040
#define NOFLSH  0x0080
#define TOSTOP  0x0100
#define ECHOCTL 0x0200
#define ECHOPRT 0x0400
#define ECHOKE  0x0800
#define FLUSHO  0x1000
#define PENDIN  0x4000
#define IEXTEN  0x8000

/* c_cc indices */
#define VINTR    0
#define VQUIT    1
#define VERASE   2
#define VKILL    3
#define VEOF     4
#define VTIME    5
#define VMIN     6
#define VSWTC    7
#define VSTART   8
#define VSTOP    9
#define VSUSP   10
#define VEOL    11
#define VREPRINT 12
#define VDISCARD 13
#define VWERASE 14
#define VLNEXT  15
#define VEOL2   16

/* tcsetattr actions */
#define TCSANOW   0
#define TCSADRAIN 1
#define TCSAFLUSH 2

/* tcflush queue selectors */
#define TCIFLUSH  0
#define TCOFLUSH  1
#define TCIOFLUSH 2

/* tcflow actions */
#define TCOOFF 0
#define TCOON  1
#define TCIOFF 2
#define TCION  3

/* Baud rates */
#define B0       0x00000000
#define B50      0x00000001
#define B75      0x00000002
#define B110     0x00000003
#define B134     0x00000004
#define B150     0x00000005
#define B200     0x00000006
#define B300     0x00000007
#define B600     0x00000008
#define B1200    0x00000009
#define B1800    0x0000000a
#define B2400    0x0000000b
#define B4800    0x0000000c
#define B9600    0x0000000d
#define B19200   0x0000000e
#define B38400   0x0000000f
#define B57600   0x00001001
#define B115200  0x00001002
#define B230400  0x00001003
#define B460800  0x00001004
#define B500000  0x00001005
#define B576000  0x00001006
#define B921600  0x00001007
#define B1000000 0x00001008
#define B1152000 0x00001009
#define B1500000 0x0000100a
#define B2000000 0x0000100b
#define B2500000 0x0000100c
#define B3000000 0x0000100d
#define B3500000 0x0000100e
#define B4000000 0x0000100f

struct winsize {
    unsigned short ws_row;
    unsigned short ws_col;
};

#ifdef __cplusplus
extern "C" {
#endif

int tcgetattr(int fd, struct termios* t);
int tcsetattr(int fd, int action, const struct termios* t);
int tcflush(int fd, int queue);
int tcdrain(int fd);
int tcflow(int fd, int action);
speed_t cfgetispeed(const struct termios* t);
speed_t cfgetospeed(const struct termios* t);
int cfsetispeed(struct termios* t, speed_t speed);
int cfsetospeed(struct termios* t, speed_t speed);
int tcgetwinsize(int fd, struct winsize* winsize_p);
int tcsetwinsize(int fd, const struct winsize* winsize_p);
int tcsendbreak(int fd, int duration);
pid_t tcgetsid(int fd);

#ifdef __cplusplus
}
#endif
