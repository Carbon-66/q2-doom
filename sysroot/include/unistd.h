#ifndef _UNISTD_H
#define _UNISTD_H
#include "_q2.h"
ssize_t read(int, void *, size_t);
ssize_t write(int, const void *, size_t);
int close(int);
off_t lseek(int, off_t, int);
int unlink(const char *);
int access(const char *, int);
int chdir(const char *);
char *getcwd(char *, size_t);
int isatty(int);
int usleep(unsigned);
unsigned sleep(unsigned);
pid_t getpid(void);
void _exit(int) __attribute__((noreturn));
ssize_t readlink(const char *, char *, size_t);
#define F_OK 0
#define R_OK 4
#define W_OK 2
#define X_OK 1
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
#endif
