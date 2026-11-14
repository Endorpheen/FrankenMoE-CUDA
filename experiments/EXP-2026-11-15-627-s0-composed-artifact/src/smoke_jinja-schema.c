// Smoke call for the jinja-schema set (jinja/runtime + json/format-grammar):
// convert {"type":"string"} to a GBNF grammar, and name a chat role through the
// runtime module so both halves of the set are exercised.
//
// The converter takes a flattened schema tree: one node is 11 i64 lanes
// (kind, unsupported, dump_sid, vf, vn, prop_first, prop_count, items, min, max,
// add_key). The node below is the same shape the accepted fixture builds for a
// string primitive; see experiments/EXP-2026-11-14-568-p7-08-schema.
#include "jinja-schema.h"

#include <stdio.h>
#include <string.h>

void goose_init_jinja_schema(int argc, char **argv);
int64_t json_schema_convert(int64_t root, sl_i64 meta, sl_i64 props, sl_i64 vals,
                            sl_u8 strs, sl_i64 sdir, sl_i64 koff, sl_i64 klen,
                            sl_i64 toff, sl_i64 tlen, sl_u8 buf, sl_i64 st,
                            sl_i64 opt, sl_i64 kvoff, sl_i64 kvlen, sl_i64 choff,
                            sl_i64 chlen, sl_u8 sbuf, sl_i64 pwork, sl_i64 mprops,
                            sl_i64 rref, sl_u8 out);
int64_t role_to_string(int64_t role, sl_u8 out);
int64_t role_from_string(sl_u8 s);

#define K_STRING 10
#define MS 11

int run_jinja_schema(void) {
    static const char schema[] = "{\"type\":\"string\"}";
    static int64_t meta[MS];          /* one node: a string primitive */
    static int64_t props[8], vals[8];
    static int64_t sdir[2];
    static int64_t koff[4096], klen[4096], toff[4096], tlen[4096];
    static int64_t st[16], opt[4096], kvoff[4096], kvlen[4096];
    static int64_t choff[4096], chlen[4096];
    static int64_t pwork[1 << 18], mprops[4 * 4096], rref[256];
    static uint8_t buf[1 << 20], sbuf_arr[1 << 20], out[1 << 20];
    static uint8_t role[64];
    char *argv[] = {(char *)"smoke", NULL};

    goose_init_jinja_schema(1, argv);

    meta[0] = K_STRING;               /* kind */
    meta[1] = 0;                      /* unsupported */
    meta[2] = 0;                      /* dump_sid -> strs[0..17] */
    sdir[0] = 0;
    sdir[1] = (int64_t)(sizeof schema - 1);

    sl_i64 smeta = {meta, MS}, sprops = {props, 0}, svals = {vals, 0};
    sl_u8 sstrs = {(uint8_t *)schema, (int64_t)(sizeof schema - 1)};
    sl_i64 ssdir = {sdir, 2};
    sl_i64 skoff = {koff, 4096}, sklen = {klen, 4096};
    sl_i64 stoff = {toff, 4096}, stlen = {tlen, 4096};
    sl_u8 sbufv = {buf, (int64_t)sizeof buf};
    sl_i64 sst = {st, 16}, sopt = {opt, 4096};
    sl_i64 skvoff = {kvoff, 4096}, skvlen = {kvlen, 4096};
    sl_i64 schoff = {choff, 4096}, schlen = {chlen, 4096};
    sl_u8 ssbuf = {sbuf_arr, (int64_t)sizeof sbuf_arr};
    sl_i64 spwork = {pwork, 1 << 18}, smprops = {mprops, 4 * 4096};
    sl_i64 srref = {rref, 256};
    sl_u8 sout = {out, (int64_t)sizeof out};

    int64_t n = json_schema_convert(0, smeta, sprops, svals, sstrs, ssdir,
                                    skoff, sklen, stoff, stlen, sbufv, sst, sopt,
                                    skvoff, skvlen, schoff, schlen, ssbuf,
                                    spwork, smprops, srref, sout);
    sl_u8 rbuf = {role, (int64_t)sizeof role};
    sl_u8 rname = {(uint8_t *)"user", 4};
    int64_t rn = role_to_string(role_from_string(rname), rbuf);

    printf("jinja:  json_schema_convert(\"string\") -> %lld bytes\n", (long long)n);
    if (n > 0) {
        /* first grammar rule, one line */
        int64_t cut = 0;
        while (cut < n && out[cut] != '\n' && cut < 80) cut++;
        printf("jinja:  %.*s\n", (int)cut, (char *)out);
    }
    printf("jinja:  role_to_string(role_from_string(\"user\")) -> %lld bytes: %.*s\n", (long long)rn,
           (int)rn, (char *)role);
    return (n > 0 && rn == 4 && memcmp(role, "user", 4) == 0) ? 0 : 1;
}
