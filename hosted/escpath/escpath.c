#include <stdio.h>
#include <string.h>

typedef unsigned char uint8_t;


#pragma warning( disable: 4100 ) /* unused parameters */
int main(int argc, char *argv[])
{
	int ch;

	while ((ch = fgetc(stdin)) != EOF) {
		fputc(ch, stdout);
		if (ch == '\\') {
			fputc(ch, stdout);
		}
	}

	return 0;
}
