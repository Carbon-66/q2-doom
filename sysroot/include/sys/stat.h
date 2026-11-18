#ifndef _SYS_STAT_H
#define _SYS_STAT_H
#include "../_q2.h"
/* no struct stat: MIPS o32's layout isn't needed, only mkdir */
int mkdir(const char *, mode_t);
#endif
