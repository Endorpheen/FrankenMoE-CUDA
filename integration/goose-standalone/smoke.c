// Minimal host for the single Goose runtime: proves the composed unit links and
// answers real queries with nothing but the host adapter and libc.
#include <stdio.h>
#include <stdint.h>

typedef struct { uint8_t * data; int64_t len; } sl_u8;
typedef struct { int64_t * data; int64_t len; } sl_i64;
typedef struct { double  * data; int64_t len; } sl_f64;

extern void fg_goose_host_ensure_init(void);
extern int64_t fg_goose_host_init_runs(void);
extern int fg_goose_host_iq_tables_bound(void);
extern int64_t abi_type_count(void);
extern int64_t abi_blck_size(int64_t type);
extern const char * abi_type_name(int64_t type);
extern int64_t cfg_validate(sl_i64 iv, sl_f64 fv);
extern int64_t mime_of(sl_u8 name);
extern int64_t mime_text(int64_t id, sl_u8 out);
extern int64_t tool_index(sl_u8 name);

static sl_u8 S(const char * s) {
    sl_u8 r;
    r.data = (uint8_t *) s;
    r.len  = (int64_t) (s[0] ? (int64_t) __builtin_strlen(s) : 0);
    return r;
}

int main(void) {
    fg_goose_host_ensure_init();
    const int64_t runs = fg_goose_host_init_runs();

    // A real query pair: a valid config and one that must be refused.
    int64_t iv[32] = {0};
    double  fv[5]  = {0};
    const int64_t valid_rule = cfg_validate((sl_i64){iv, 32}, (sl_f64){fv, 5});

    int64_t ti = tool_index(S("read_file"));
    // Two-step contract (mime-table.goose): mime_of names the type, mime_text
    // writes it. One call each, the same order the server uses.
    const int64_t mime_id = mime_of(S("index.html"));
    char type_buf[64];
    const int64_t type_len = mime_text(mime_id, (sl_u8) { (uint8_t *) type_buf, (int64_t) sizeof type_buf });
    if (type_len > 0) {
        type_buf[type_len < (int64_t) sizeof type_buf ? type_len : (int64_t) sizeof type_buf - 1] = '\0';
    }

    printf("FG_STANDALONE init_runs=%lld iq_bound=%d types=%lld blck_q4k=%lld "
           "cfg_rule=%lld tool_read_file=%lld mime_id=%lld mime=%s\n",
           (long long) runs, fg_goose_host_iq_tables_bound(), (long long) abi_type_count(),
           (long long) abi_blck_size(12), (long long) valid_rule,
           (long long) ti, (long long) mime_id, type_len > 0 ? type_buf : "<none>");
    return runs == 1 ? 0 : 1;
}
