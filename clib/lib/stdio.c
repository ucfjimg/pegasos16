#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXFILE 16

struct _file_t _files[MAXFILE];

#define DEF_BUFSIZE 128

static unsigned parse_mode(const char *mode);
static FILE *get_empty_file(void);

void _init_stdio(void)
{
    int i;
    FILE *fp;

    for (i = 0; i < 3; i++) {
        fp = &_files[i];

        /* TODO eventually the process will get fd's 0 1 2 
         * for stdin, stdout, and stderr. Right now all three
         * ard just hard-coded to 0 which is the owning tty
         */
        fp->fd = i;
        fp->buf = malloc(DEF_BUFSIZE);
        fp->next = fp->buf;
        fp->cnt = 0;
        fp->flags = (i == 0) ? _FILE_READ : _FILE_WRITE;
    }
}

void _close_stdio(void)
{
    int fd;

    for (fd = 0; fd < MAXFILE; fd++) {
        if (_files[fd].flags) {
            fclose(&_files[fd]);
        }
    }
}

FILE *fopen(const char *fn, const char *mode)
{
    unsigned flags;
    int oflags = 0;
    FILE *fp;

    errno = 0;

    if ((flags = parse_mode(mode)) == 0) {
        errno = EINVAL;
        return NULL;
    }

    if ((fp = get_empty_file()) == NULL) {
        errno = EMFILE;
        return NULL;
    }

    if ((fp->buf = malloc(DEF_BUFSIZE)) == NULL) {
        errno = ENOMEM;
        return NULL;
    }

    switch (flags) {
    case _FILE_READ:
        oflags = O_RDONLY;
        break;

    case _FILE_READ | _FILE_PLUS:
        flags |= _FILE_WRITE;
        oflags = O_RDWR;
        break;

    case _FILE_WRITE:
        oflags = O_WRONLY | O_CREAT | O_TRUNC;
        break;

    case _FILE_WRITE | _FILE_PLUS:
        flags |= _FILE_READ;
        oflags = O_RDWR | O_CREAT | O_TRUNC;
        break;

    case _FILE_APP:
        flags |= _FILE_WRITE;
        oflags = O_WRONLY | O_CREAT;
        break;

    case _FILE_APP | _FILE_PLUS:
        flags |= _FILE_READ | _FILE_WRITE;
        oflags = O_RDWR | O_CREAT;
        break;

    default:
        free(fp->buf);
        return NULL;
    }

    if ((fp->fd = open(fn, oflags)) == -1) {
        free(fp->buf);
        fp->buf = NULL;
        return NULL;
    }

    fp->flags = flags;
    fp->cnt = 0;
    fp->next = fp->buf;

    return fp;
}

int fclose(FILE *fp) 
{
    int rc = 0;

    if (fp->flags & _FILE_WRITE) {
        rc = fflush(fp);
    }

    close(fp->fd);
    free(fp->buf);
    
    fp->buf = NULL;
    fp->next = NULL;
    fp->flags = NULL;

    return rc;
}

int fputs(const char *s, FILE *fp)
{
    while (*s) {
        char ch = *s++;
        if (putc(ch, fp) == EOF) {
            return EOF;
        }
    }

    return 0;
}

int puts(const char *s)
{
    if (fputs(s, stdout) == EOF) {
        return -1;
    }

    return putc('\n', stdout);
}

char *fgets(char *s, int size, FILE *fp)
{
    register char *p = s;

    if (size == 0) {
        return NULL;
    }
    size--;
    
    while (p - s < size) {
        int ch = getc(fp);
        if (ch == EOF) {
            break;
        }
        *p++ = (char)ch;
        if (ch == '\n') {
            break;
        }        
    }

    *p++ = '\0';

    return (p == s) ? NULL : s;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *fp)
{
    unsigned char *pb = ptr;
    size_t got;
    unsigned nptr = (unsigned )ptr;
    unsigned long left;
    unsigned readsize;

    if (fp->flags & (_FILE_ERR | _FILE_EOF)) {
        return 0;
    }

    /* since this is a near data call, we can't read more than size_t
     * total bytes; we have to be careful not to let the pointer
     * wrap. adjust nmemb down to how many elements will actually
     * fit in the remainder of the segment
     */
    left = (0x10000ul - nptr) / size;
    if (nmemb > left) {
        nmemb = (size_t)left;
    }

    /* at this point we know this is safe and won't wrap
     */
    readsize = nmemb * size;

    /* first use up any data already buffered in the fp
     */
    if (fp->cnt) {
        unsigned copy = ((unsigned)fp->cnt > readsize) ? readsize : fp->cnt;
        memcpy(fp->next, pb, copy);
        fp->cnt -= copy;
        fp->next += copy;
        readsize -= copy;
        pb += copy;
    }

    /* now try to read from the file
     */
    if (readsize) {
        got = read(fp->fd, (void*)pb, readsize);
        if (got == (unsigned)-1) {
            fp->flags |= _FILE_ERR;
        } else {
            pb += got;
        }
    }

    /* we want to return the integral number of elements we read,
     * even if we read a partial one at the end
     */
    got = pb - (unsigned char*)ptr;
    got /= size;

    /* don't set EOF if we already set error. The user should
     * get only one indication of the most important thing that
     * went wrong.
     */
    if (got == 0 && (fp->flags & _FILE_ERR) == 0) {
        fp->flags |= _FILE_EOF;
    }

    return got;
}


int fflush(FILE *fp)
{
    int cnt;
    int wrote = 0;

    if (fp->flags & (_FILE_EOF | _FILE_ERR)) {
        return EOF;
    }

    if (fp->flags & _FILE_WRITE) {
        cnt = fp->next - fp->buf;
        if (cnt) {
            wrote = write(fp->fd, fp->buf, cnt);

            if (wrote < cnt) {
                fp->flags |= _FILE_ERR;
            }
        }
    }

    fp->next = fp->buf;
    fp->cnt = DEF_BUFSIZE;

    return (fp->flags & _FILE_ERR) ? EOF : wrote;    
}

int _ffill(FILE *fp)
{
    if (fp->fd == 0) {
        fflush(stdout);
    }

    if (fp->flags & (_FILE_EOF | _FILE_ERR)) {
        return EOF;
    }

    /* TODO we should never call this if there is already still pending
     * data - check and assert!
     */
    fp->cnt = 0;

    if ((fp->flags & (_FILE_EOF | _FILE_ERR)) == 0) {
        fp->cnt = read(fp->fd, fp->buf, DEF_BUFSIZE);
        fp->next = fp->buf;
        if (fp->cnt == -1) {
            fp->flags |= _FILE_ERR;
        } else if (fp->cnt == 0) {
            fp->flags |= _FILE_EOF;
        }
    }

    if (fp->flags & (_FILE_EOF | _FILE_ERR)) {
        return EOF;
    }
    
    fp->cnt--;
    return *fp->next++;
}

int _fflush(FILE *fp, int ch)
{
    if (fp->flags & (_FILE_EOF | _FILE_ERR)) {
        return EOF;
    }

    fflush(fp);

    *fp->next++ = (char)ch;
    --fp->cnt;

    return  (fp->flags & _FILE_ERR) ? EOF : ch;
}

unsigned parse_mode(const char *mode)
{
    unsigned flags = 0;

    /* enforce that only one of r,w,a is set and occurs before + 
     */
    while (*mode) {
        switch (*mode++) {
        case 'r':
            if (flags) {
                return 0;
            }
            flags |= _FILE_READ;
            break;

        case 'w':
            if (flags) {
                return 0;
            }
            flags |= _FILE_WRITE;
            break;

        case 'a':
            if (flags) {
                return 0;
            }
            flags |= _FILE_APP;
            break;

        case 'b':
            /* for DOS compat */
            break;

        case '+':
            flags |= _FILE_PLUS;
            break;

        default:
            return 0;
        }
    }

    /* + on its own is not a valid flag */
    if ((flags & _FILE_PLUS) == _FILE_PLUS) {
        return 0;
    }

    return flags;
}

FILE *get_empty_file(void)
{
    int i;

    for (i = 0; i < MAXFILE; i++) {
        if (_files[i].flags == 0) {
            return _files + i;
        }
    }
    return NULL;
}
