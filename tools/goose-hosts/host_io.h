/* Shared line protocol for the Goose decision hosts.
 *
 * Every host reads one request per line from stdin and writes exactly one line
 * to stdout. This keeps a caller (python, bash, cmake) to a single subprocess
 * for a whole corpus instead of one per decision.
 *
 *   request := COMMAND [SP field]...
 *   field   := text | '-'            '-' is the empty string
 *   text    := hex of the raw bytes  (so no field can contain a space or \n)
 *   reply   := "OK <value>" | "ERR <reason>"
 *
 * Buffers are static: the hosts are single-threaded batch filters, not
 * libraries. Nothing here allocates and nothing reads a file.
 */
#ifndef BMOE_GOOSE_HOST_IO_H
#define BMOE_GOOSE_HOST_IO_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HIO_MAX_FIELDS 40
#define HIO_MAX_DECODED (1 << 20)

static uint8_t hio_a[HIO_MAX_DECODED];
static uint8_t hio_b[HIO_MAX_DECODED];
static uint8_t hio_c[HIO_MAX_DECODED];
static uint8_t hio_out[4 * HIO_MAX_DECODED];
static char hio_line[8 * HIO_MAX_DECODED];

static int hio_nib(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Decode into dst; returns the byte count, or -1 on a malformed field. */
static int64_t hio_decode(const char *s, uint8_t *dst, int64_t cap) {
    int64_t n = 0;
    size_t i = 0;
    if (s == NULL) return 0;
    if (s[0] == '-' && s[1] == 0) return 0;
    while (s[i] != 0) {
        int hi = hio_nib((unsigned char) s[i]);
        int lo = s[i + 1] ? hio_nib((unsigned char) s[i + 1]) : -1;
        if (hi < 0 || lo < 0 || n >= cap) return -1;
        dst[n] = (uint8_t) ((hi << 4) | lo);
        n++;
        i += 2;
    }
    return n;
}

/* Encode n bytes as hex, NUL-terminated, into dst. */
static char *hio_encode(const uint8_t *src, int64_t n, char *dst) {
    static const char *d = "0123456789abcdef";
    int64_t i = 0;
    if (n <= 0) {
        dst[0] = '-';
        dst[1] = 0;
        return dst;
    }
    for (i = 0; i < n; i++) {
        dst[2 * i] = d[src[i] >> 4];
        dst[2 * i + 1] = d[src[i] & 15];
    }
    dst[2 * n] = 0;
    return dst;
}

/* Split one line in place; returns the field count, -1 when it does not fit. */
static int hio_split(char *line, char **f, int cap) {
    int n = 0;
    char *p = line;
    if (p == NULL) return -1;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == 0) return 0;
    while (n < cap) {
        f[n++] = p;
        while (*p != 0 && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') p++;
        if (*p == 0) return n;
        *p++ = 0;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == 0) return n;
    }
    return -1;
}

/* Read one request line; returns 0 at end of input. */
static int hio_readline(void) {
    size_t n = 0;
    int c = 0;
    while ((c = getchar()) != EOF) {
        if (c == '\n') break;
        if (n + 1 < sizeof hio_line) hio_line[n++] = (char) c;
    }
    if (c == EOF && n == 0) return 0;
    hio_line[n] = 0;
    return 1;
}

static int64_t hio_int(const char *s) {
    return (int64_t) strtoll(s, NULL, 10);
}

/* "-" is the empty string; a plain decimal is itself. */
static int64_t hio_milli(const char *s) {
    return hio_int(s);
}

#endif /* BMOE_GOOSE_HOST_IO_H */
