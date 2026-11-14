// Host-side runtime hooks that the accepted json/jinja modules borrow through
// `extern "gs_*"`. The canonical gates supply them from their C++ fixtures (see
// experiments/EXP-2026-11-14-551-p7-08-strftime/tests/stmt_fixture.cpp); a
// shared runtime needs them in one small library instead.
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct { uint8_t *data; int64_t len; } gs_sl_u8;

double gs_json_strtod(gs_sl_u8 s) {
    char tmp[512];
    size_t n = (size_t)s.len;
    if (n >= sizeof tmp) n = sizeof tmp - 1;
    if (n) memcpy(tmp, s.data, n);
    tmp[n] = 0;
    return strtod(tmp, NULL);
}

int64_t gs_json_fmt_e(gs_sl_u8 out, int64_t prec, double x) {
    int n = snprintf((char *)out.data, (size_t)out.len, "%.*e", (int)prec, x);
    return n < 0 ? -1 : (int64_t)n;
}

int64_t gs_json_fmt_g(gs_sl_u8 out, double x) {
    int n = snprintf((char *)out.data, (size_t)out.len, "%.17g", x);
    return n < 0 ? -1 : (int64_t)n;
}

double gs_frexp_mant(double x) {
    int e = 0;
    return frexp(x, &e);
}

/* std::to_string(double): fixed notation, six decimals */
int64_t gs_to_string_f64(gs_sl_u8 out, double x) {
    int n = snprintf((char *)out.data, (size_t)out.len, "%f", x);
    return n < 0 ? -1 : (int64_t)n;
}

int64_t gs_strftime_epoch(gs_sl_u8 out, int64_t epoch, gs_sl_u8 fmt) {
    char f[128];
    size_t fn = (size_t)fmt.len;
    if (fn >= sizeof f) fn = sizeof f - 1;
    if (fn) memcpy(f, fmt.data, fn);
    f[fn] = 0;
    const time_t t = epoch < 0 ? time(NULL) : (time_t)epoch;
    const struct tm *tmv = localtime(&t);
    if (!tmv) return -1;
    return (int64_t)strftime((char *)out.data, (size_t)out.len, f, tmv);
}
