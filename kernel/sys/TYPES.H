#ifndef TYPES_H_
#define TYPES_H_

typedef unsigned int size_t;
typedef int dev_t;

#ifndef TIME_T_DEFINED_
#define TIME_T_DEFINED_
typedef unsigned long time_t;
#endif

#ifndef OFF_T_DEFINED_
#define OFF_T_DEFINED_
typedef long off_t;
#endif

typedef unsigned long ino_t;

#endif

