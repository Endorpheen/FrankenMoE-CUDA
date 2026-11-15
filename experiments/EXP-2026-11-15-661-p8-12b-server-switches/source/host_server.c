// Host adapter for the composed server Goose unit (P8-12).
//
// The unit is compiled from goose/server/{model-manager,tools-pure,web-assets,
// mime-table,cors-policy}.goose plus jsonparse. Its 29 undefined symbols are
// 28 libc entries and one Goose helper, gs_json_strtod, which the unit borrows
// from the JSON parser. This file provides exactly that helper, the same body
// the M1 host adapter carries, so the server unit needs no ggml dependency.
//
// Nothing else from M1's 51-symbol adapter is used: the server modules do not
// touch quant primitives, metric encoders or IQ tables.

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint8_t * data;
    int64_t   len;
} sl_u8;

static char * host_dupz(sl_u8 s) {
    char * t = (char *) malloc((size_t) s.len + 1);
    if (t == NULL) {
        return NULL;
    }
    if (s.len > 0) {
        memcpy(t, s.data, (size_t) s.len);
    }
    t[s.len] = '\0';
    return t;
}

double gs_json_strtod(sl_u8 s) {
    char * t = host_dupz(s);
    if (t == NULL) {
        return 0.0;
    }
    const double v = strtod(t, NULL);
    free(t);
    return v;
}
