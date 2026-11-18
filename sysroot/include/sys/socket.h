#ifndef _SYS_SOCKET_H
#define _SYS_SOCKET_H
#include "../_q2.h"
typedef unsigned int socklen_t;
struct sockaddr { unsigned short sa_family; char sa_data[14]; };
#define AF_UNIX 1
#define SOCK_STREAM 2 /* MIPS swaps these two */
#define SOCK_DGRAM 1
#define SOCK_CLOEXEC 0x80000
#define SOCK_NONBLOCK 0x80
#define MSG_DONTWAIT 0x40
int socket(int, int, int);
int bind(int, const void *, socklen_t);
ssize_t recv(int, void *, size_t, int);
ssize_t sendto(int, const void *, size_t, int, const void *, socklen_t);
#endif
