#ifndef _PTHREAD_H
#define _PTHREAD_H
typedef unsigned long pthread_t;
/* glibc 2.28 o32: a mutex is 24 bytes, zero is PTHREAD_MUTEX_INITIALIZER */
typedef union { char size[24]; long align; } pthread_mutex_t;
#define PTHREAD_MUTEX_INITIALIZER { { 0 } }
int pthread_create(pthread_t *, const void *, void *(*)(void *), void *);
int pthread_join(pthread_t, void **);
int pthread_mutex_lock(pthread_mutex_t *);
int pthread_mutex_unlock(pthread_mutex_t *);
#endif
