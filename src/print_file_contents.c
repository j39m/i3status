// vim:ts=4:sw=4:expandtab

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>
#include <sys/types.h>

#include <sys/fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "i3status.h"

#define STRING_SIZE 10

static void *scalloc(size_t size) {
    void *result = calloc(size, 1);
    if (result == NULL) {
        die("Error: out of memory (calloc(%zu))\n", size);
    }
    return result;
}

/*
 * Attempts to read |max_chars| (counted in UTF-8 sequences) out of
 * |buffer| and NUL-terminate the sequence.
 */
static void truncate_to_max_chars(char *buffer, size_t max_chars) {
    int i;
    size_t cnt;
    for (i = 0, cnt = 0; cnt < max_chars; cnt++) {
        unsigned char current = buffer[i] & 0xff;
        if (current == '\0') {
            break;
        } else if (current >> 7 == 0x00) {  // starts with "0b0"
            i++;
        } else if (current >> 5 == 0x06) {  // starts with "0b110"
            i += 2;
        } else if (current >> 4 == 0x0e) {  // starts with "0b1110"
            i += 3;
        } else if (current >> 3 == 0x1e) {  // starts with "0b11110"
            i += 4;
        } else {  // possibly malformed UTF-8
            break;
        }
    }
    buffer[i] = '\0';
}

void print_file_contents(yajl_gen json_gen, char *buffer, const char *title, const char *path, const char *format, const char *format_bad, const int max_chars) {
    const char *walk = format;
    char *outwalk = buffer;

    // UTF-8 sequences are at most 4 bytes each.
    size_t max_bytes = 4 * max_chars * sizeof(char);
    char *buf = scalloc(max_bytes + 1);

    int n = -1;
    int fd = open(path, O_RDONLY);

    INSTANCE(path);

    if (fd > -1) {
        n = read(fd, buf, max_bytes);
        if (n != -1) {
            buf[n] = '\0';
        }
        (void)close(fd);
        START_COLOR("color_good");
    } else if (errno != 0) {
        walk = format_bad;
        START_COLOR("color_bad");
    }

    truncate_to_max_chars(buf, max_chars);

    // remove newline chars
    char *src, *dst;
    for (src = dst = buf; *src != '\0'; src++) {
        *dst = *src;
        if (*dst != '\n') {
            dst++;
        }
    }
    *dst = '\0';

    char string_errno[STRING_SIZE];

    sprintf(string_errno, "%d", errno);

    placeholder_t placeholders[] = {
        {.name = "%title", .value = title},
        {.name = "%content", .value = buf},
        {.name = "%errno", .value = string_errno},
        {.name = "%error", .value = strerror(errno)}};

    const size_t num = sizeof(placeholders) / sizeof(placeholder_t);
    buffer = format_placeholders(walk, &placeholders[0], num);

    free(buf);

    END_COLOR;
    OUTPUT_FULL_TEXT(buffer);
    free(buffer);
}
