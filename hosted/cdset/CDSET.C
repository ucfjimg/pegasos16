#include <stdio.h>
#include <direct.h>

int main(int argc, char *argv[])
{
	char cwd[256];

	if (argc != 2) {
		fprintf(stderr, "cdset: variable\n");
		return 1;
	}

	getcwd(cwd, sizeof(cwd) - 1);
	printf("SET %s=\"%s\"\n", argv[1], cwd);


	return 0;
}
