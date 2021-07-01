#ifndef STDIO_H_
#define STDIO_H_

#include <sys/types.h>

struct _file_t {
    int fd;
    char *buf;
    char *next;
    int cnt;
    unsigned flags;
};

#define _FILE_READ  0x01
#define _FILE_WRITE 0x02
#define _FILE_APP   0x04
#define _FILE_EOF   0x08
#define _FILE_ERR   0x10
#define _FILE_PLUS  0x20

extern struct _file_t _files[];

#define stdin  (&_files[0])
#define stdout (&_files[1])
#define stderr (&_files[2])

#define EOF (-1)

typedef struct _file_t FILE;

extern FILE *fopen(const char *fn, const char *mode);
extern int fclose(FILE *fp);
extern int fflush(FILE *fp);
extern int fputs(const char *s, FILE *fp);
extern int puts(const char *s);
extern char *fgets(char *s, int size, FILE *fp);
extern int printf(const char *fmt, ...);
extern size_t fread(void *ptr, size_t size, size_t nmemb, FILE *fp);


extern int _ffill(FILE *fp);
extern int _fflush(FILE *fp, int ch);

#define getc(stm) (--((stm)->cnt) >= 0 ? ((*(stm)->next++) & 0xff) : _ffill(stm))
#define putc(ch, stm) (--((stm)->cnt) >= 0 ? ((*(stm)->next++ = ch) & 0xff) : _fflush(stm, ch))


#endif
