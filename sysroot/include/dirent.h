#ifndef _DIRENT_H
#define _DIRENT_H
/* glibc 2.28 MIPS o32 readdir's struct dirent (32-bit ino and off) */
typedef struct __dirstream DIR;
struct dirent {
    unsigned long d_ino;
    long d_off;
    unsigned short d_reclen;
    unsigned char d_type;
    char d_name[256];
};
DIR *opendir(const char *);
struct dirent *readdir(DIR *);
int closedir(DIR *);
#endif
