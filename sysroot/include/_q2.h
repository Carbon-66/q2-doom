/* Shared types for q2-doom's minimal glibc 2.28 MIPS o32 headers. There is no MIPS sysroot in
 * the build: these declare only what Doom and the Q2 backend use, with the Q2's ABI (32-bit
 * longs and time_t, 32-bit off_t, MIPS numbering for the constants that differ from x86). */
#ifndef Q2_SYSROOT_H
#define Q2_SYSROOT_H
typedef unsigned int size_t;
typedef int ssize_t;
typedef long off_t;
typedef long time_t;
typedef int pid_t;
typedef unsigned int mode_t;
typedef unsigned int uid_t;
typedef unsigned int gid_t;
#ifndef NULL
#define NULL ((void *)0)
#endif
#endif
