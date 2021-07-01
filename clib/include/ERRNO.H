/*
 * Error numbers
 */
#ifndef ERRNO_H_
#define ERRNO_H_

extern int errno;

#define ENODEV 	    1
#define ENOMEM 	    2
#define EBUSY 	    3
#define EIO 		4
#define EINVAL 	    5
#define ENOSPC      6
#define ENOENT      7
#define ENFILE      8
#define EEXIST      9
#define EBADF      10
#define EMFILE     11
#define EACCES     12
#define EOVERFLOW  13

#define EMAX       32

#endif
