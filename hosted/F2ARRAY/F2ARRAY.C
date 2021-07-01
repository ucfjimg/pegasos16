#include <stdio.h>
#include <string.h>

typedef unsigned char uint8_t;

int main(int argc, char *argv[])
{
    static uint8_t buffer[512];
    FILE *fp, *fout;
    size_t i, j;
    size_t rd;
    size_t size;
    const int PERLINE = 16;
    const char *inname, *outname, *varname;
    int wroteok;

    if (argc != 4) {
        fprintf(stderr, "f2array: binary-file output-file var-name\n");
        return 1;
    }

    inname = argv[1];
    outname = argv[2];
    varname = argv[3];

    fp = fopen(inname, "rb");
    if (!fp) {
        perror(inname);
        return 1;
    }

    fout = fopen(outname, "w");
    if (!fout) {
        perror(outname);
        return 1;
    }

    fprintf(fout, "/*\n * this file was generated from %s\n */\n", inname);
    fprintf(fout, "#if defined(__TURBOC__) || (defined(_MSC_VER) && (_MSC_VER <= 800))\n");
    fprintf(fout, "typedef unsigned char uint8_t;\n");
    fprintf(fout, "#else\n");
    fprintf(fout, "# include <stdint.h>\n");
	fprintf(fout, "#endif\n");
    fprintf(fout, "#include <stdlib.h>\n\n");
    fprintf(fout, "uint8_t %s[] = {\n", varname);

    size = 0;
    while (!feof(fp)) {
        rd = fread(buffer, 1, sizeof(buffer), fp);
        size += rd;

        for (i = 0; i < rd; i += PERLINE) {
            size_t line = PERLINE;
            if (i + line > rd) {
                line = rd - i;
            }
            fprintf(fout, "    ");
            for (j = 0; j < line; j++) {
                fprintf(fout, "0x%02x, ", buffer[i+j]);
            }
            fprintf(fout, "\n");
        }
    }

    if (ferror(fp)) {
        fprintf(stderr, "Error reading from %s\n", inname);
        fclose(fout);
        remove(outname);
        return 1;
    }
    fprintf(fout, "};\n\n");
    fprintf(fout, "size_t n_%s = %u;\n", varname, size);

    wroteok = 1;
    if (ferror(fout)) {
        wroteok = 0;
        fclose(fout);
    } else if (fclose(fout) != 0) {
        wroteok = 0;
    }

    if (!wroteok) {
        fprintf(stderr, "Error writing to %s\n", outname);
        remove(outname);
        return 1;
    }

    return 0;
}
