# A 16-bit OS kernel

This project came about due to speculation about what it would have looked like if the original MS-DOS had supported multitasking (plus, I always wanted to write some form of kernel from scratch.)

Pegasos (a pun on Pegasus, UCF's logo) is written in both assembler and C, using period Microsoft tools. It was primarily developed on a Windows 98 machine; the reason there is little git history is there was no git client, so the original source control was SVN :)

For simplicity, Pegasos uses the MS-DOS file system (FAT) and executable format (MZ). This allowed easy bootstrapping of the OS using existing compiler and OS tools to build bootable images. It was a goal from the start to run on real hardware as well as a virtual machine, so most testing was done on a 386 ISA maching booting from a 3.5" floppy (yes, I'm a packrat).

Internally, the kernel supports a fully preemptive task scheduler; tasks are used both for drivers and as the basis for processes. The process model is Unix-like. The system call interface is very small, just enough to be able to read and write files and fork and spawn new processes. However, with a local console driver, a serial driver, and a floppy driver (after booting, the kernel no longer uses BIOS at all) this is enough to prove that two shell instances can be run.

As expected, while all this works, the problems come from lack of virtual memory management. 16-bit 8086 means real mode, with all the segmentation and lack of memory protection that implies. With multiple processes coming and going, and very little memory available, fragmentation quickly becomes a problem. Swapping can help, but it can't overcome the fact that a process has two choices: use only near pointers or never be relocatable. The real solution to this, of course, is to move to a CPU that supports virtual memory in hardware, but that's a project for another summer.
