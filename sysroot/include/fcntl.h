#ifndef _FCNTL_H
#define _FCNTL_H
#include "_q2.h"
/* MIPS numbering */
#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#define O_APPEND 0x0008
#define O_NONBLOCK 0x0080
#define O_CREAT 0x0100
#define O_TRUNC 0x0200
#define O_EXCL 0x0400
#define O_CLOEXEC 0x80000
#define F_GETFL 3
#define F_SETFL 4
int open(const char *, int, ...);
int fcntl(int, int, ...);
#endif
