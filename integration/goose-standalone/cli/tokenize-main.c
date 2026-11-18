// frankenmoe-tokenize: the standalone runtime's tokenizer entry point.
//
// The decisions around tokenization are Goose (goose/cli/tokenize-cli.goose:
// tz_source, tz_add_bos, tz_fmt_ids) and tokenization itself is the Goose engine
// (btl_load + bt_bpe). This file owns only what a process must own: argv, stdin,
// the file mapping and stdout.
//
// It lives under cli/ rather than beside host_v4.c so it is not counted as adapter
// debt: an entry point is not a bridge between Goose and a substrate, it is the
// program the user runs.
//
// Usage:
//   frankenmoe-tokenize <model.gguf> --prompt TEXT [--ids] [--no-bos]
//   frankenmoe-tokenize <model.gguf> --stdin     [--ids] [--no-bos]
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
extern int64_t gr_meta_int(sl_u8 data, sl_i64 state, sl_i64 kv, int64_t index, sl_i64 out);
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

// The CLI decisions, in Goose.
extern int64_t tz_source(int64_t stdin_flag, int64_t file_present, int64_t prompt_present);
extern int64_t tz_add_bos(int64_t model_wants_add_bos, int64_t no_bos);
extern int64_t tz_fmt_ids(sl_i64 tokens, sl_u8 out);

static sl_u8 S(void * p, int64_t n) { return (sl_u8){ (uint8_t *) p, n }; }
static sl_u8 K(const char * s) { return (sl_u8){ (uint8_t *) s, (int64_t) strlen(s) }; }

int main(int argc, char ** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s <model.gguf> (--prompt TEXT | --stdin) [--ids] [--no-bos]\n", argv[0]);
        return 2;
    }
    // First, before any Goose export: tz_source is a Goose decision and the unit
    // refuses every export until it is initialised.
    goose_init(0, NULL);
    const char * path = argv[1];
    const char * prompt = NULL;
    int stdin_flag = 0, ids_mode = 0, no_bos = 0;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--prompt") == 0 && i + 1 < argc) prompt = argv[++i];
        else if (strcmp(argv[i], "--stdin") == 0) stdin_flag = 1;
        else if (strcmp(argv[i], "--ids") == 0) ids_mode = 1;
        else if (strcmp(argv[i], "--no-bos") == 0) no_bos = 1;
        else { fprintf(stderr, "unknown argument: %s\n", argv[i]); return 2; }
    }

    // The prompt source is a Goose decision, not an if-chain here.
    const int64_t source = tz_source(stdin_flag, 0, prompt != NULL ? 1 : 0);
    if (source < 0) { fprintf(stderr, "a prompt is required\n"); return 2; }

    char * text = NULL;
    size_t text_len = 0;
    if (source == 2) {
        // No strdup: with -std=c11 it is not declared without _POSIX_C_SOURCE, and the
        // compiler then assumes it returns int - the pointer is silently truncated to
        // 32 bits. That cost a debugging round; malloc+memcpy cannot.
        text_len = strlen(prompt);
        text = malloc(text_len + 1);
        memcpy(text, prompt, text_len + 1);
    } else {
        size_t cap = 4096, len = 0;
        text = malloc(cap);
        size_t n;
        while ((n = fread(text + len, 1, cap - len, stdin)) > 0) {
            len += n;
            if (len == cap) { cap *= 2; text = realloc(text, cap); }
        }
        text_len = len;
        if (text_len > 0 && text[text_len - 1] == '\n') text_len--;
    }

    int fd = open(path, O_RDONLY);
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

    int64_t * state = calloc(8, sizeof(int64_t));
    int64_t * kv = calloc((size_t) (n_kv * 8), sizeof(int64_t));
    int64_t * tens = calloc((size_t) (n_tensors * 12), sizeof(int64_t));
    int64_t ksn = 16, tsn = 16;
    while (ksn < 2 * n_kv + 2) ksn <<= 1;
    while (tsn < 2 * n_tensors + 2) tsn <<= 1;
    int64_t * ksl = calloc((size_t) ksn, sizeof(int64_t));
    int64_t * tsl = calloc((size_t) tsn, sizeof(int64_t));

    sl_u8 data = S(base, hdr);
    sl_i64 st8 = { state, 8 }, kvv = { kv, n_kv * 8 }, kslots = { ksl, ksn };
    if (gr_parse(data, file_bytes, st8, kvv, (sl_i64){ tens, n_tensors * 12 }, kslots,
                 (sl_i64){ tsl, tsn }) != 0) {
        fprintf(stderr, "the Goose reader refused %s\n", path);
        return 1;
    }
    const int64_t ti = gr_find_kv(data, st8, kvv, kslots, K("tokenizer.ggml.tokens"));
    const int64_t mi = gr_find_kv(data, st8, kvv, kslots, K("tokenizer.ggml.merges"));
    if (ti < 0 || mi < 0) { fprintf(stderr, "no tokenizer in %s\n", path); return 1; }

    const int64_t n_tok = gr_arr_n(st8, kvv, ti);
    const int64_t n_mrg = gr_arr_n(st8, kvv, mi);
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

    if (btl_load(data, st8, kvv, kslots,
                 (sl_i64){ tok_off, n_tok }, (sl_i64){ tok_len, n_tok }, (sl_i64){ tok_slots, tcap },
                 (sl_i64){ mrg_off, n_mrg > 0 ? n_mrg : 1 }, (sl_i64){ mrg_len, n_mrg > 0 ? n_mrg : 1 },
                 (sl_i64){ mrg_slots, mcap }, (sl_i64){ pairs, 2 * maxn },
                 (sl_i64){ tok_type, n_tok }, (sl_i64){ attr, n_tok }, (sl_i64){ order, n_tok },
                 (sl_i64){ eog, n_tok }, -1, -1, -1, (sl_i64){ stats, 8 }) != 0) {
        fprintf(stderr, "the Goose engine refused the vocabulary in %s\n", path);
        return 1;
    }

    // Whether the model wants a BOS is a metadata question, so it comes from the reader.
    int64_t wants_bos = 0;
    const int64_t bi = gr_find_kv(data, st8, kvv, kslots, K("tokenizer.ggml.add_bos_token"));
    if (bi >= 0) {
        int64_t v = 0;
        if (gr_meta_int(data, st8, kvv, bi, (sl_i64){ &v, 1 }) == 0) wants_bos = v != 0;
    }
    const int64_t with_bos = tz_add_bos(wants_bos, no_bos);

    const int64_t n = (int64_t) text_len;
    int64_t cap = n + 8 + (with_bos ? 1 : 0);
    int64_t * ids = calloc((size_t) cap, sizeof(int64_t));
    int64_t * cpts = calloc((size_t) n + 1, sizeof(int64_t));
    int64_t * seg = calloc((size_t) (4 * (n + 1)), sizeof(int64_t));
    uint8_t * enc = calloc((size_t) (8 * (n + 1)), 1);
    int64_t * sym = calloc((size_t) (24 * (n + 1)), sizeof(int64_t));
    int64_t * heap = calloc((size_t) (15 * (n + 1) + 40), sizeof(int64_t));
    int64_t * hc = calloc(2, sizeof(int64_t));
    int64_t * info = calloc(8, sizeof(int64_t));

    int64_t nout = 0;
    if (with_bos) {
        // The BOS id is whatever the vocabulary says; the engine does not invent one.
        const int64_t bos = gr_find_kv(data, st8, kvv, kslots, K("tokenizer.ggml.bos_token_id"));
        if (bos >= 0) {
            int64_t v = 0;
            if (gr_meta_int(data, st8, kvv, bos, (sl_i64){ &v, 1 }) == 0) ids[nout++] = v;
        }
    }
    const int64_t got = bt_bpe(S(text, n), data,
                               (sl_i64){ tok_off, n_tok }, (sl_i64){ tok_len, n_tok },
                               (sl_i64){ tok_slots, tcap },
                               (sl_i64){ mrg_off, n_mrg > 0 ? n_mrg : 1 },
                               (sl_i64){ mrg_len, n_mrg > 0 ? n_mrg : 1 },
                               (sl_i64){ mrg_slots, mcap },
                               (sl_i64){ ids + nout, cap - nout }, 0,
                               (sl_i64){ cpts, n + 1 }, (sl_i64){ seg, 4 * (n + 1) },
                               S(enc, 8 * (n + 1)), (sl_i64){ sym, 24 * (n + 1) },
                               (sl_i64){ heap, 15 * (n + 1) + 40 }, (sl_i64){ hc, 2 },
                               (sl_i64){ info, 8 });
    if (got < 0) { fprintf(stderr, "bt_bpe refused the input\n"); return 1; }
    nout += got;

    if (ids_mode) {
        char out[65536];
        const int64_t len = tz_fmt_ids((sl_i64){ ids, nout }, S(out, (int64_t) sizeof(out)));
        if (len < 0) { fprintf(stderr, "the id list does not fit the output buffer\n"); return 1; }
        fwrite(out, 1, (size_t) len, stdout);
    } else {
        for (int64_t i = 0; i < nout; i++) printf("%s%lld", i ? " " : "", (long long) ids[i]);
        printf("\n");
    }
    return 0;
}
