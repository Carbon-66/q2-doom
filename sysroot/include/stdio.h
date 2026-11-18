#ifndef _STDIO_H
#define _STDIO_H
#include "_q2.h"
#include <stdarg.h>
typedef struct _IO_FILE FILE;
extern FILE *stdin, *stdout, *stderr;
#define EOF (-1)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define BUFSIZ 8192
#define FILENAME_MAX 4096
FILE *fopen(const char *, const char *);
FILE *freopen(const char *, const char *, FILE *);
int fclose(FILE *);
size_t fread(void *, size_t, size_t, FILE *);
size_t fwrite(const void *, size_t, size_t, FILE *);
int fseek(FILE *, long, int);
long ftell(FILE *);
int fflush(FILE *);
int feof(FILE *);
int ferror(FILE *);
int fgetc(FILE *);
int getc(FILE *);
int fputc(int, FILE *);
int putc(int, FILE *);
int fputs(const char *, FILE *);
char *fgets(char *, int, FILE *);
int puts(const char *);
int putchar(int);
int printf(const char *, ...) __attribute__((format(printf, 1, 2)));
int fprintf(FILE *, const char *, ...) __attribute__((format(printf, 2, 3)));
int sprintf(char *, const char *, ...);
int snprintf(char *, size_t, const char *, ...) __attribute__((format(printf, 3, 4)));
int vprintf(const char *, va_list);
int vfprintf(FILE *, const char *, va_list);
int vsnprintf(char *, size_t, const char *, va_list);
int vsprintf(char *, const char *, va_list);
int sscanf(const char *, const char *, ...);
int fscanf(FILE *, const char *, ...);
int remove(const char *);
int rename(const char *, const char *);
int fileno(FILE *);
void setbuf(FILE *, char *);
int setvbuf(FILE *, char *, int, size_t);
#define _IONBF 2
void perror(const char *);
#endif
