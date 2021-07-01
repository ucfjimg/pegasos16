#ifndef WAIT_H_
#define WAIT_H_

#define _WIFEXITED      0x0100
#define _WIFSIGNALED    0x0200

#define WIFEXITED(x)    ((x) & _WIFEXITED)
#define WIFSIGNALED(x)  ((x) & _WIFSIGNALED)
#define WEXITSTATUS(x)  ((x) & 0xff)
#define WTERMSIG(x)     ((x) & 0xff)

extern int wait(int __far *status);

#endif
