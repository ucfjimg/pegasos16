#include <stdio.h>
#include <stdlib.h>

int main()
{
    fputs("hello, world!\n", stdout);
    fflush(stdout);
    sleep(1000);
    fputs("one second has passed\n", stdout);
    return 0;
}
