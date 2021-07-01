#include <stdio.h>

int main(int argc, char *argv[])
{
	FILE *fp;
	char ver[80];

	if (argc != 2) {
		fprintf(stderr, "mkver version-file\n");
		return 1;
	}

	fp = fopen(argv[1], "r");
	if (!fp) {
		fprintf(stderr, "failed to open \"%s\".\n");
		return 1;
	}

	fscanf(fp, "%s", ver);

	printf("const char *kver = \"%s\";\n", ver);

	return 0;
}

