// Standalone load-and-tokenise path: the caller the status report says is missing.
//
// Reads a GGUF with mmap, lets the Goose reader parse it (gr_parse), builds the BPE
// tables from the reader's own array counts (btl_load), then tokenises a corpus with
// bt_bpe. Nothing in the loop is C++.
//
// The expected ids are RECORDED from the accepted libllama, not computed here:
//
//   TEXT "abc"      -> 1 ids: 257
//   TEXT "abc ab a" -> 5 ids: 257 32 256 32 97
//   TEXT "a b c"    -> 5 ids: 97 32 98 32 99
//   TEXT "aab"      -> 2 ids: 97 256
//   TEXT "  x"      -> 3 ids: 32 32 120
//   TEXT ""         -> 0 ids
//
// produced by llama_tokenize(add_special=false, parse_special=true) on the same file
// with the accepted library. Recording them keeps libllama out of this gate's link,
// so the standalone binary needs no llama library; the provenance is the command in
// the experiment README.
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct { uint8_t * data; int64_t len; } sl_u8;
typedef struct { int64_t * data; int64_t len; } sl_i64;

extern void    goose_init(int argc, char ** argv);
extern int64_t gr_parse(sl_u8 data, int64_t file_bytes, sl_i64 state, sl_i64 kv, sl_i64 tensors,
                        sl_i64 kv_slots, sl_i64 tensor_slots);
extern int64_t gr_find_kv(sl_u8 data, sl_i64 state, sl_i64 kv, sl_i64 slots, sl_u8 key);
extern int64_t gr_arr_n(sl_i64 state, sl_i64 kv, int64_t index);
extern int64_t btl_load(sl_u8 gguf, sl_i64 state, sl_i64 kv, sl_i64 kvslots,
                        sl_i64 tok_off, sl_i64 tok_len, sl_i64 tok_slots,
                        sl_i64 mrg_off, sl_i64 mrg_len, sl_i64 mrg_slots,
                        sl_i64 pairs, sl_i64 tok_type, sl_i64 attr, sl_i64 order, sl_i64 eog,
                        int64_t eos_id, int64_t eot_id, int64_t eom_id, sl_i64 stats);
extern int64_t bt_bpe(sl_u8 text, sl_u8 gguf,
                      sl_i64 tok_off, sl_i64 tok_len, sl_i64 tok_slots,
                      sl_i64 mrg_off, sl_i64 mrg_len, sl_i64 mrg_slots,
                      sl_i64 ids, int64_t nout,
                      sl_i64 cpts, sl_i64 seg, sl_u8 enc,
                      sl_i64 sym, sl_i64 heap, sl_i64 hc, sl_i64 info);

typedef struct { const char * text; int64_t ids[8]; int n; } corpus_case;

static const corpus_case CASES[] = {
    { "abc",      { 257, 32, 256, 32,  97 }, 1 },
    { "abc ab a", { 257, 32, 256, 32,  97 }, 5 },
    { "a b c",    {  97, 32,  98, 32,  99 }, 5 },
    { "aab",      {  97, 256 },              2 },
    { "  x",      {  32, 32, 120 },          3 },
    { "",         { 0 },                     0 },
};

static int checks = 0, fails = 0;
static void check(int cond, const char * what) {
    checks++;
    if (!cond) { fails++; printf("FAIL %s\n", what); }
}

static sl_u8 S(void * p, int64_t n) { return (sl_u8){ (uint8_t *) p, n }; }
static sl_u8 K(const char * s) { return (sl_u8){ (uint8_t *) s, (int64_t) strlen(s) }; }

int main(int argc, char ** argv) {
    if (argc < 2) { printf("usage: tokenize_load <vocab.gguf>\n"); return 2; }
    goose_init(0, NULL);

    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) { perror("open"); return 2; }
    struct stat st;
    if (fstat(fd, &st) != 0) { perror("fstat"); return 2; }
    const int64_t file_bytes = st.st_size;
    const int64_t hdr = file_bytes < 67108864 ? file_bytes : 67108864;
    void * base = mmap(NULL, (size_t) hdr, PROT_READ, MAP_PRIVATE, fd, 0);
    if (base == MAP_FAILED) { perror("mmap"); return 2; }

    int64_t n_kv = 0, n_tensors = 0;
    memcpy(&n_tensors, (const uint8_t *) base + 8, 8);
    memcpy(&n_kv,      (const uint8_t *) base + 16, 8);
    printf("container: n_kv=%lld n_tensors=%lld window=%lld\n",
           (long long) n_kv, (long long) n_tensors, (long long) hdr);

    int64_t * state = calloc(8, sizeof(int64_t));
    int64_t * kv = calloc((size_t) (n_kv * 8), sizeof(int64_t));
    int64_t * tens = calloc((size_t) (n_tensors * 12), sizeof(int64_t));
    int64_t ksn = 16, tsn = 16;
    while (ksn < 2 * n_kv + 2) ksn <<= 1;
    while (tsn < 2 * n_tensors + 2) tsn <<= 1;
    int64_t * ksl = calloc((size_t) ksn, sizeof(int64_t));
    int64_t * tsl = calloc((size_t) tsn, sizeof(int64_t));

    sl_u8 data = S(base, hdr);
    sl_i64 st8 = { state, 8 }, kvv = { kv, n_kv * 8 };
    sl_i64 kslots = { ksl, ksn };

    const int64_t rc = gr_parse(data, file_bytes, st8, kvv, (sl_i64){ tens, n_tensors * 12 },
                                kslots, (sl_i64){ tsl, tsn });
    check(rc == 0, "gr_parse on the fixture");
    if (rc != 0) { printf("gr_parse rc=%lld\n", (long long) rc); return 1; }

    const int64_t ti = gr_find_kv(data, st8, kvv, kslots, K("tokenizer.ggml.tokens"));
    const int64_t mi = gr_find_kv(data, st8, kvv, kslots, K("tokenizer.ggml.merges"));
    check(ti >= 0 && mi >= 0, "both tokenizer keys found through the reader");
    if (ti < 0 || mi < 0) return 1;

    const int64_t n_tok = gr_arr_n(st8, kvv, ti);
    const int64_t n_mrg = gr_arr_n(st8, kvv, mi);
    printf("reader: tokens=%lld merges=%lld\n", (long long) n_tok, (long long) n_mrg);

    int64_t tcap = 16, mcap = 16;
    while (tcap < 2 * n_tok + 2) tcap <<= 1;
    while (mcap < 2 * (n_mrg > 0 ? n_mrg : 1) + 2) mcap <<= 1;
    const int64_t maxn = n_tok > n_mrg ? n_tok : n_mrg;

    int64_t * tok_off = calloc((size_t) n_tok, sizeof(int64_t));
    int64_t * tok_len = calloc((size_t) n_tok, sizeof(int64_t));
    int64_t * mrg_off = calloc((size_t) (n_mrg > 0 ? n_mrg : 1), sizeof(int64_t));
    int64_t * mrg_len = calloc((size_t) (n_mrg > 0 ? n_mrg : 1), sizeof(int64_t));
    int64_t * tok_slots = calloc((size_t) tcap, sizeof(int64_t));
    int64_t * mrg_slots = calloc((size_t) mcap, sizeof(int64_t));
    int64_t * pairs = calloc((size_t) (2 * maxn), sizeof(int64_t));
    int64_t * tok_type = calloc((size_t) n_tok, sizeof(int64_t));
    int64_t * attr = calloc((size_t) n_tok, sizeof(int64_t));
    int64_t * order = calloc((size_t) n_tok, sizeof(int64_t));
    int64_t * eog = calloc((size_t) n_tok, sizeof(int64_t));
    int64_t * stats = calloc(8, sizeof(int64_t));

    const int64_t lr = btl_load(data, st8, kvv, kslots,
                                (sl_i64){ tok_off, n_tok }, (sl_i64){ tok_len, n_tok },
                                (sl_i64){ tok_slots, tcap },
                                (sl_i64){ mrg_off, n_mrg > 0 ? n_mrg : 1 },
                                (sl_i64){ mrg_len, n_mrg > 0 ? n_mrg : 1 },
                                (sl_i64){ mrg_slots, mcap },
                                (sl_i64){ pairs, 2 * maxn }, (sl_i64){ tok_type, n_tok },
                                (sl_i64){ attr, n_tok }, (sl_i64){ order, n_tok },
                                (sl_i64){ eog, n_tok }, -1, -1, -1, (sl_i64){ stats, 8 });
    check(lr == 0, "btl_load built the tables");
    printf("tables: rc=%lld tokens=%lld merges=%lld dups=%lld/%lld specials=%lld eog=%lld\n",
           (long long) lr, (long long) stats[0], (long long) stats[1],
           (long long) stats[2], (long long) stats[3], (long long) stats[4], (long long) stats[5]);

    // Tokenise the corpus through the engine, on the tables the reader produced.
    for (size_t c = 0; c < sizeof(CASES) / sizeof(CASES[0]); c++) {
        const char * text = CASES[c].text;
        const int64_t n = (int64_t) strlen(text);
        int64_t * ids = calloc(64, sizeof(int64_t));
        int64_t * cpts = calloc((size_t) n + 1, sizeof(int64_t));
        int64_t * seg = calloc((size_t) (4 * (n + 1)), sizeof(int64_t));
        uint8_t * enc = calloc((size_t) (8 * (n + 1)), 1);
        int64_t * sym = calloc((size_t) (24 * (n + 1)), sizeof(int64_t));
        int64_t * heap = calloc((size_t) (15 * (n + 1) + 40), sizeof(int64_t));
        int64_t * hc = calloc(2, sizeof(int64_t));
        int64_t * info = calloc(8, sizeof(int64_t));

        const int64_t got = bt_bpe(S((void *) text, n), data,
                                   (sl_i64){ tok_off, n_tok }, (sl_i64){ tok_len, n_tok },
                                   (sl_i64){ tok_slots, tcap },
                                   (sl_i64){ mrg_off, n_mrg > 0 ? n_mrg : 1 },
                                   (sl_i64){ mrg_len, n_mrg > 0 ? n_mrg : 1 },
                                   (sl_i64){ mrg_slots, mcap },
                                   (sl_i64){ ids, 64 }, 0,
                                   (sl_i64){ cpts, n + 1 }, (sl_i64){ seg, 4 * (n + 1) },
                                   S(enc, 8 * (n + 1)),
                                   (sl_i64){ sym, 24 * (n + 1) },
                                   (sl_i64){ heap, 15 * (n + 1) + 40 },
                                   (sl_i64){ hc, 2 }, (sl_i64){ info, 8 });

        check(got == CASES[c].n, "id count matches the oracle");
        if (got != CASES[c].n) {
            printf("FAIL \"%s\": got %lld ids, oracle %d\n", text, (long long) got, CASES[c].n);
        } else {
            for (int i = 0; i < CASES[c].n; i++) {
                if (ids[i] != CASES[c].ids[i]) {
                    printf("FAIL \"%s\" id %d: %lld, oracle %lld\n", text, i,
                           (long long) ids[i], (long long) CASES[c].ids[i]);
                    fails++;
                }
                checks++;
            }
        }
        free(ids); free(cpts); free(seg); free(enc); free(sym); free(heap); free(hc); free(info);
    }

    printf("%s checks=%d failures=%d\n", fails == 0 ? "PASS" : "BAD", checks, fails);
    return fails ? 1 : 0;
}
