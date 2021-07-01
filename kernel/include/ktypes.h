/*
 * standard types for the kernel
 */
#ifndef KTYPES_H_
#define KTYPES_H_

/* when an exact size is needed
 */
typedef unsigned char uint8_t;
typedef signed char int8_t;
typedef unsigned uint16_t;
typedef int int16_t;
typedef unsigned long uint32_t;
typedef long int32_t;

/* boolean (if in stuct, can use as :bitfield)
 */
typedef unsigned bool_t;
#define FALSE 0
#define TRUE 1

/* device and subunit identifiers
 */
/* NB this is in MSVC headers typedef int dev_t */;
typedef int unit_t;


/* an error
 */
typedef int kerrno_t;

/* address of a block on a device
 */
typedef uint32_t blkno_t;


/* construct a far pointer
 */
#define MKFP(s, o) (void __far *)(((uint32_t)(s) << 16) | ((uint32_t)((o) & 0xffff)))
#define KOFFS_OF(fp)	(*(uint16_t __far *)&(fp))
#define KSEG_OF(fp)		(*(1+(uint16_t __far *)&(fp)))
#define KLINEAR(fp)     ((((uint32_t)(KSEG_OF(fp)))<<4)+KOFFS_OF(fp))

/* tag an unreferenced parameter so it's not a warning/error
 */
#define UNREFERENCED(x) x=x


#ifndef NULL
# define NULL 0
#endif


/* paragraph - 16 bytes
 * page (exe)- 512 bytes
 * k         - 1024 bytes
 */
#define PARASHIFT 	4
#define PAGESHIFT 	9
#define KSHIFT     10

#define PARASIZE (1 << PARASHIFT)

#endif
