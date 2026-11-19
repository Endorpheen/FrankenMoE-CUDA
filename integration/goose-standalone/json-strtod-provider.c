// gs_json_strtod: the one host primitive the ported JSON parser needs.
//
// The parser reads a decimal number as a byte slice and needs the C library's strtod on it. This is substrate, not a decision:
// the grammar of the number (where it ends, what is legal) stays in Goose; only the conversion of a NUL-terminated copy is here.
// Copied from integration/m1/host/goose_host.c, which is generated rather than written, so the shipped unit needs its own copy.
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct { const uint8_t * data; int64_t len; } sl_u8;

double gs_json_strtod(sl_u8 s) {
    char stack[64];
    char * t = stack;
    if (s.len < 0) {
        return 0.0;
    }
    if ((size_t) s.len + 1 > sizeof(stack)) {
        t = (char *) malloc((size_t) s.len + 1);
        if (t == NULL) {
            return 0.0;
        }
    }
    memcpy(t, s.data, (size_t) s.len);
    t[s.len] = '\0';
    const double v = strtod(t, NULL);
    if (t != stack) {
        free(t);
    }
    return v;
}

// gs_json_fmt_e: the dumper's primitive, "%.*e" into a caller slice. Substrate like the one above - the choice of format stays in
// the module, only the formatting call is C.
#include <stdio.h>

int64_t gs_json_fmt_e(sl_u8 out, int64_t prec, double x) {
    return (int64_t) snprintf((char *) out.data, (size_t) out.len, "%.*e", (int) prec, x);
}
