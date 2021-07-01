#include "fat.h"

#include <ctype.h>
#include <string.h>

#ifdef KERNEL
int toupper(int ch)
{
	if (ch >= 'a' && ch <= 'z') {
		return 'A' + ch - 'a';
	}
	return ch;
}

#endif

/* Parse an 8.3 filename into the format stored in the directory
 *
 * dst must be an 11 byte array, which will contain the result
 * src is the filename to parser
 * len is the number of bytes to parse from src
 *
 * Returns 0 on success
 * -EINVAL if the filename is not a legal 8.3 name
 */ 
int path_parse_fn(char *dst, const char *src, int len)
{
    int s, d;

    /* Check for the special cases of . and .., which are just stored as themselves
     * followed by spaces
     */
    if (src[0] == '.' && (len == 1 || (src[1] == '.' || len == 2))) {
        d = 0;
        dst[d++] = '.';
        if (src[1] == '.') {
            dst[d++] = '.';
        }
        for (;d < 11; d++) {
            dst[d] = '\0';
        }
        return 0;
    }

    /* Up to 8 characters of filename, terminated by end of string or '.'
     * The file system is case insensitive and names are stored on disk
     * in upper case
     *
     * TODO this and the extension code should check for invalid characters
     */
    for (d = 0; d < 8; d++) {
        if (d == len || src[d] == '.') {
            break;
        }
		dst[d] = (char)toupper(src[d]);
    }

    s = d;
    if (s < len && src[s] != '.') {
        /* base part of name is longer than 8 characters
		 */
		return -EINVAL;
    }

    /* pad out the rest of the base name with spaces
     */
    for (; d < 8; d++) {
        dst[d] = ' ';
    }

    /* if there's an extension, copy it
     */
    if (src[s] == '.') {
        s++;
        for (; d < 11; d++) {
            if (s == len) {
                break;
            }
            dst[d] = (char)toupper(src[s]);
            s++;
        }

        if (d == 11 && s < len) {
            /* extension is longer than 3 characters
			 */
			return -EINVAL;
        }
    }

    /* pad extension with spaces
     */
    for (;d < 11; d++) {
        dst[d] = ' ';
    }

    return 0;
}

/* Parse the next directory element in a path (in fn)
 * start is the index to start at
 *
 * Returns a (start,length) pair containing the bounds of the
 * element. If there are no more elements, the returned length
 * will be zero.
 */
substr_t path_next_dir_ele(const char *fn, int start)
{
    substr_t ss;
	ss.start = start;
	ss.len = 0;

    while (fn[start] == '\\' || fn[start] == '/') {
        start++;
    }

    ss.start = start;

    while (fn[start] && fn[start] != '\\' && fn[start] != '/') {
        start++;
    }

    ss.len = start - ss.start;
    return ss;
}

/* Split a rooted path into a directory and name (last element)
 * No check is done to see if the name part is actually a file,
 * this function merely parses.
 *
 * Modifies fn in place. Returns the path part in dir (which
 * will be NULL if there is no path), and the name part
 * in name.
 */
void path_split(char *fn, char **dir, char **name)
{
    char *sep = strrchr(fn, '\\');

    if (sep) {
        *sep++ = '\0';
        *dir = fn;
        *name = sep;
    } else {
        *dir = "";
        *name = fn;
    }
}

