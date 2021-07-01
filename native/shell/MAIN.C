#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <io.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/wait.h>

#pragma warning( disable : 4704 )

extern int errno;

#if 0
void conputc(char cha)
{
    _asm mov ah, 0Eh
    _asm mov al, [cha]
    _asm mov bx, 0007h
    _asm int 10h
}

void puthex(unsigned v)
{
    int i;
    unsigned j;
    char str[4];

    for (i = 0; i < 4; i++) {
        j = v & 0x0f;
        v >>= 4;

        j |= 0x30;
        if (j >= 0x3a) {
            j = j + 'A' - '9';
        }
        str[3-i] = (char)j;
    }

    for (i = 0; i < 4; i++) {
        conputc(str[i]);
    }
}

void putstr(char *s) 
{
    while (*s) {
        conputc(*s++);
    }
}
#endif

char crlf[] = "\r\n";
char prompt[] = "$ ";

#define BUFSZ 128

void ls(void)
{
    FILE *fp;
    char c[32];
    int i,j;

    if ((fp = fopen("/", "r")) == NULL) {
        printf("failed to open / %d\n", errno);
    } else {
        while (fread(c, 32, 1, fp) == 1) {
            /* TODO a file system ought to abstract this stuff out into
             * a canonical directory entry that's not FAT specific
             */
            if (c[0] == 0) {
                break;
            }

            if (c[0] == 0xe5) {
                /* entry is deleted
                 */
                continue;
            }

            if (c[11] == 0x0f) {
                /* entry is part of a long file name
                 */
                continue;
            }

            for (i = 7; i >= 0; i--) {
                if (c[i] != ' ') {
                    break;
                }
            }
            i++;

            for (j = 0; j < i; j++) {
                putc(c[j], stdout);
            }

            if (c[8] != ' ') {
                putc('.', stdout);
                for (j = 8; j < 11; j++) {
                    if (c[j] == ' ') {
                        break;
                    }
                    putc(c[j], stdout);
                }
            }

            putc('\n', stdout);
        }
        if ((fp->flags & _FILE_ERR) != 0) {
            printf("error reading directory\n");
        }
    }
    fclose(fp);
}

int parse(char *line, int maxargc, char **argv)
{
    register char *p = line;
    int arg = 0;
    char quote;

    while (*p) {
        while (isspace(*p)) {
            p++;
        }

        if (!*p) {
            break;
        }

        quote = 0;
        if (*p == '\'' || *p == '"') {
            quote = *p++;
        }

        argv[arg++] = p;

        if (quote) {
            while (*p && *p != quote) {
                p++;
            }
        } else {
            while (*p && !isspace(*p)) {
                p++;
            }
        }


        if (!*p) {
            break;
        }

        *p++ = '\0';
        if (arg == maxargc - 1) {
            break;
        }
    }

    argv[arg] = NULL;
    return arg;
}

#define MAX_ARG 16

int main()
{
    int fd;
    char *linebuf;
    size_t len;
    char *argv[MAX_ARG];
    int argc;
    int i;
    int status;

    fd = open("/dev/tty0", O_RDWR);
    dup(fd);
    dup(fd);

    fputs(crlf, stdout);

    linebuf = malloc(BUFSZ);
    if (linebuf == NULL) {
        printf("sh: out of memory\n");
        return 1;
    }

    fputs(crlf, stdout);
    fputs(prompt, stdout);

	for(;;) {
        if (fgets(linebuf, BUFSZ, stdin) == NULL) {
            break;
        }
        len = strlen(linebuf);
        if (linebuf[len-1] == '\n') {
            linebuf[len-1] = '\0';
        }

        if (strcmp(linebuf, "exit") == 0) {
            break;
        }

        argc = parse(linebuf, MAX_ARG, argv);

        if (argc >= 1) {
            i = exec(argv[0]);
            if (i < 0) {
                printf("cannot find %s (%d)\n", argv[0], i);
            } else {
                wait(&status);
            }
        }

        fputs(prompt, stdout);
	}

	return 0;
}
	 
