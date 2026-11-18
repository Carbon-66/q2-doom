#ifndef _POLL_H
#define _POLL_H
struct pollfd { int fd; short events, revents; };
#define POLLIN 1
int poll(struct pollfd *, unsigned long, int);
#endif
