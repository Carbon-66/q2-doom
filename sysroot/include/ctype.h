#ifndef _CTYPE_H
#define _CTYPE_H
/* glibc exports these as functions too (no __ctype_b_loc tables needed). */
int isalnum(int), isalpha(int), iscntrl(int), isdigit(int), isgraph(int), islower(int),
    isprint(int), ispunct(int), isspace(int), isupper(int), isxdigit(int), tolower(int),
    toupper(int);
#endif
