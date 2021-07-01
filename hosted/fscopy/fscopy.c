#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "blkcache.h"
#include "fat\fat.h"
#include "blkcache\blkdev.h"

int main(int argc, char *argv[])
{
    int to = 0;
    int from = 0;
    struct stat st;
    int n;
    unit_t unit;
    int rc;
    fatvol_t fv;
    static uint8_t data[512];
    int fd;
    FILE *fp;
    int got = 0, put;

    if (argc != 4) {
        printf("fscopy: fs-image source dest\n");
        return 1;
    }

    if (argv[2][0] == '~' && argv[2][1] == ':') {
        from = 1;
    }

    if (argv[3][0] == '~' && argv[3][1] == ':') {
        to = 1;
    }

    if (from + to != 1) {
        fprintf(stderr, "exactly one filename must be on the image.\n");
        return 1;
    }
    
    /* we check here to prevent the loop device from creating an unformatted
     * image.
     */
    if (stat(argv[1], &st) != 0) {
        fprintf(stderr, "image %s does not exist.\n", argv[1]);
        return 1;
    }

    n = loop_open(argv[1]);
    if (n != 0) {
        fprintf(stderr, "failed to open loop file %s\n", argv[1]);
        return 1;
    }
    unit = LOOP_UNIT + n;

    rc = bc_init();
    if (rc < 0) {
		fprintf(stderr, "init virtual i/o: %s\n", strerror(-rc));
		return 1;
    }

	rc = f_mount(0, unit, &fv);
	if (rc < 0) {
		fprintf(stderr, "mount volume: %s\n", strerror(-rc));
		return 1;
	}

    if (from) {
        fd = f_open(&fv, argv[2] + 2, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "open %s: %s\n", argv[2] + 2, strerror(-fd));
            return 1;
        }

        fp = fopen(argv[3], "wb");
        if (fp == NULL) {
            perror(argv[3]);
            return 1;
        }

        for(;;) {
            got = f_read(&fv, fd, (char __far *)data, sizeof(data));
            if (got < 0) {
                fprintf(stderr, "read %s\n", strerror(-got));
                return 1;
            }
            if (got == 0) {
                break;
            }

            if (fwrite(data, 1, got, fp) != (size_t)got) {
                fprintf(stderr, "failed to write\n");
                return 1;
            }
        }

        f_close(&fv, fd);
        fclose(fp);
    } else {
        fp = fopen(argv[2], "rb");
        if (fp == NULL) {
            perror(argv[2]);
            return 1;
        }

        fd = f_open(&fv, argv[3] + 2, O_CREAT | O_WRONLY);
        if (fd < 0) {
            fprintf(stderr, "open %s: %s\n", argv[3] + 2, strerror(-fd));
            return 1;
        }

        for(;;) {
            got = fread(data, 1, sizeof(data), fp);
            printf("got %d\n", got);
            if (got == 0) {
                break;
            }

            if (got < 0) {
                perror(argv[2]);
                break;
            }

            put = f_write(&fv, fd, (char __far *)data, got);
            if (put < 0) {
                fprintf(stderr, "write %d %s\n", put, strerror(-put));
                return 1;
            }
            printf("put %d\n", put);
        }

        f_close(&fv, fd);
        fclose(fp);
    }

    f_umount(&fv);
    rc = bc_sync();
    if (rc < 0) {
        fprintf(stderr, "sync %s\n", strerror(-rc));
        return 1;
    }

    return 0;
}