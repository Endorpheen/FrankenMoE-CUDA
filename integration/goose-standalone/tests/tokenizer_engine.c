// Tokenizer engine gate: the standalone path, with no llama.cpp vocabulary.
//
// Header -> Goose reader -> Goose vocabulary layout (bt_prepare_from_gguf)
// -> bt_build -> bt_token_to_piece for every id, in both the special and the
// non-special mode, written as length-prefixed records.
//
// The output is byte-comparable with the same dump produced through the
// accepted libllama (`pieces` in EXP-2026-11-17-925), which is the oracle: it
// answers from the C++ vocabulary. Agreement means the engine reproduces the
// vocabulary and the piece path without libllama.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "unit.h"
#include <llama.h>

#define HDR_CAP (64u * 1024u * 1024u)

static int64_t find_key(sl_u8 data, sl_i64 state, sl_i64 kv, sl_i64 kvsl, const char * key) {
    return gr_find_kv(data, state, kv, kvsl, (sl_u8){ (uint8_t *) key, (int64_t) strlen(key) });
}

// Tokenize one line through the Goose chain. bt_bpe runs the Qwen3.5
// pre-tokenizer itself and then the merges, so the whole line goes in once.
static int64_t goose_tokenize(const uint8_t * blob, int64_t blob_len,
                              const int64_t * tok_off, const int64_t * tok_len, const int64_t * tok_sl, int64_t n_tok_slots,
                              const int64_t * mrg_off, const int64_t * mrg_len, const int64_t * mrg_sl, int64_t n_mrg_slots,
                              const char * text, int64_t text_len, int64_t * ids, int64_t ids_cap) {
    int64_t * cpts = calloc((size_t) text_len + 1, sizeof(int64_t));
    int64_t * seg  = calloc(4 * ((size_t) text_len + 1), sizeof(int64_t));
    uint8_t * enc  = calloc(8 * ((size_t) text_len + 1), 1);
    int64_t * sym  = calloc(24 * ((size_t) text_len + 1), sizeof(int64_t));
    int64_t * heap = calloc(15 * ((size_t) text_len + 1) + 40, sizeof(int64_t));
    int64_t * hc   = calloc(2, sizeof(int64_t));
    int64_t * info = calloc(8, sizeof(int64_t));
    const int64_t got = bt_bpe((sl_u8){ (uint8_t *) text, text_len }, (sl_u8){ (uint8_t *) blob, blob_len },
                               (sl_i64){ (int64_t *) tok_off, n_tok_slots }, (sl_i64){ (int64_t *) tok_len, n_tok_slots },
                               (sl_i64){ (int64_t *) tok_sl, n_tok_slots },
                               (sl_i64){ (int64_t *) mrg_off, n_mrg_slots }, (sl_i64){ (int64_t *) mrg_len, n_mrg_slots },
                               (sl_i64){ (int64_t *) mrg_sl, n_mrg_slots },
                               (sl_i64){ ids, ids_cap }, 0,
                               (sl_i64){ cpts, text_len + 1 }, (sl_i64){ seg, 4 * (text_len + 1) },
                               (sl_u8){ enc, 8 * (text_len + 1) },
                               (sl_i64){ sym, 24 * (text_len + 1) }, (sl_i64){ heap, 15 * (text_len + 1) + 40 },
                               (sl_i64){ hc, 2 }, (sl_i64){ info, 8 });
    free(cpts); free(seg); free(enc); free(sym); free(heap); free(hc); free(info);
    return got;
}

int main(int argc, char ** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s <model.gguf> <out.bin> [corpus.txt]\n", argv[0]);
        return 2;
    }
    goose_init(0, NULL);

    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) { perror("open"); return 2; }
    struct stat st;
    if (fstat(fd, &st) != 0) { perror("fstat"); return 2; }
    const int64_t hdr_len = st.st_size < (int64_t) HDR_CAP ? (int64_t) st.st_size : (int64_t) HDR_CAP;
    uint8_t * hdr = mmap(NULL, (size_t) hdr_len, PROT_READ, MAP_PRIVATE, fd, 0);
    if (hdr == MAP_FAILED) { perror("mmap"); return 2; }

    int64_t state[8] = {0};
    int64_t * kv      = calloc(256 * 8, sizeof(int64_t));
    int64_t * tens    = calloc(2048 * 12, sizeof(int64_t));
    int64_t * kvsl    = calloc(512, sizeof(int64_t));
    int64_t * tensl   = calloc(8192, sizeof(int64_t));

    if (gr_parse((sl_u8){ hdr, hdr_len }, st.st_size,
                 (sl_i64){ state, 8 }, (sl_i64){ kv, 256 * 8 }, (sl_i64){ tens, 2048 * 12 },
                 (sl_i64){ kvsl, 512 }, (sl_i64){ tensl, 8192 }) != 0) {
        printf("FG_TOKENIZER_ENGINE parse failed\n");
        return 1;
    }

    const int64_t tok_i  = find_key((sl_u8){ hdr, hdr_len }, (sl_i64){ state, 8 }, (sl_i64){ kv, 256 * 8 }, (sl_i64){ kvsl, 512 }, "tokenizer.ggml.tokens");
    const int64_t mrg_i  = find_key((sl_u8){ hdr, hdr_len }, (sl_i64){ state, 8 }, (sl_i64){ kv, 256 * 8 }, (sl_i64){ kvsl, 512 }, "tokenizer.ggml.merges");
    const int64_t type_i = find_key((sl_u8){ hdr, hdr_len }, (sl_i64){ state, 8 }, (sl_i64){ kv, 256 * 8 }, (sl_i64){ kvsl, 512 }, "tokenizer.ggml.token_type");
    if (tok_i < 0 || mrg_i < 0 || type_i < 0) {
        printf("FG_TOKENIZER_ENGINE missing arrays tok=%lld mrg=%lld type=%lld\n",
               (long long) tok_i, (long long) mrg_i, (long long) type_i);
        return 1;
    }

    int64_t need[5] = {0};
    if (bt_prepare_need((sl_u8){ hdr, hdr_len }, (sl_i64){ state, 8 }, (sl_i64){ kv, 256 * 8 }, tok_i, mrg_i,
                        (sl_i64){ need, 5 }) != 0) {
        printf("FG_TOKENIZER_ENGINE prepare_need failed\n");
        return 1;
    }
    const int64_t n_tok = need[1], n_mrg = need[2];

    uint8_t * blob    = malloc((size_t) (need[0] + 1));
    int64_t * tok_off = calloc((size_t) n_tok, sizeof(int64_t));
    int64_t * tok_len = calloc((size_t) n_tok, sizeof(int64_t));
    int64_t * tok_sl  = calloc((size_t) need[3], sizeof(int64_t));
    int64_t * mrg_off = calloc((size_t) n_mrg, sizeof(int64_t));
    int64_t * mrg_len = calloc((size_t) n_mrg, sizeof(int64_t));
    int64_t * mrg_sl  = calloc((size_t) need[4], sizeof(int64_t));
    int64_t * attr    = calloc((size_t) n_tok, sizeof(int64_t));
    int64_t * toktype = calloc((size_t) n_tok, sizeof(int64_t));

    if (bt_prepare_from_gguf((sl_u8){ hdr, hdr_len }, (sl_i64){ state, 8 }, (sl_i64){ kv, 256 * 8 }, tok_i, mrg_i,
                             (sl_u8){ blob, need[0] }, (sl_i64){ tok_off, n_tok }, (sl_i64){ tok_len, n_tok },
                             (sl_i64){ tok_sl, need[3] }, (sl_i64){ mrg_off, n_mrg }, (sl_i64){ mrg_len, n_mrg },
                             (sl_i64){ mrg_sl, need[4] }, (sl_i64){ need, 5 }) != 0) {
        printf("FG_TOKENIZER_ENGINE prepare_from_gguf failed\n");
        return 1;
    }

    // token types: the reader gives the payload span of the integer array
    int64_t span[4] = {0};
    if (gr_val_span((sl_i64){ state, 8 }, (sl_i64){ kv, 256 * 8 }, type_i, (sl_i64){ span, 4 }) != 0 ||
        span[2] < n_tok * 4) {
        printf("FG_TOKENIZER_ENGINE token_type span failed\n");
        return 1;
    }
    const int32_t * tt = (const int32_t *) (hdr + span[1]);
    for (int64_t i = 0; i < n_tok; ++i) {
        toktype[i] = tt[i];
    }
    if (bt_attrs((sl_u8){ blob, need[0] }, (sl_i64){ tok_off, n_tok }, (sl_i64){ tok_len, n_tok },
                 (sl_i64){ toktype, n_tok }, (sl_i64){ attr, n_tok }) != 0) {
        printf("FG_TOKENIZER_ENGINE attrs failed\n");
        return 1;
    }

    int64_t stats[4] = {0};
    const int64_t build_rc = bt_build((sl_u8){ blob, need[0] }, (sl_i64){ tok_off, n_tok }, (sl_i64){ tok_len, n_tok },
                                      (sl_i64){ tok_sl, need[3] }, (sl_i64){ mrg_off, n_mrg }, (sl_i64){ mrg_len, n_mrg },
                                      (sl_i64){ mrg_sl, need[4] }, (sl_i64){ stats, 4 });
    if (build_rc != 0) {
        printf("FG_TOKENIZER_ENGINE bt_build rc=%lld\n", (long long) build_rc);
        return 1;
    }

    if (argc >= 4) {
        // Comparison mode: same lines through the Goose chain and through
        // libllama's vocabulary. Only the non-special path exists in Goose so
        // far, so that is what is compared.
        llama_backend_init();
        struct llama_model_params mp = llama_model_default_params();
        mp.vocab_only = true;
        mp.n_gpu_layers = 0;
        struct llama_model * lm = llama_model_load_from_file(argv[1], mp);
        if (!lm) { printf("FG_TOKENIZER_ENGINE oracle load failed\n"); return 1; }
        const struct llama_vocab * v = llama_model_get_vocab(lm);
        FILE * in = fopen(argv[3], "rb");
        if (!in) { perror("fopen corpus"); return 2; }

        static char line[1 << 16];
        llama_token ref[1 << 16];
        int64_t * gids = calloc(1 << 16, sizeof(int64_t));
        int64_t cases = 0, ref_total = 0, goose_total = 0, bad = 0;
        while (fgets(line, sizeof(line), in)) {
            size_t len = strlen(line);
            while (len && (line[len - 1] == '\n' || line[len - 1] == '\r')) { line[--len] = 0; }
            if (len == 0) { continue; }
            const int32_t n_ref = llama_tokenize(v, line, (int32_t) len, ref, (int32_t) (1 << 16), false, false);
            const int64_t n_g = goose_tokenize(blob, need[0], tok_off, tok_len, tok_sl, need[3],
                                               mrg_off, mrg_len, mrg_sl, need[4], line, (int64_t) len,
                                               gids, 1 << 16);
            cases++;
            if (n_ref < 0 || n_g < 0) { bad++; printf("FG_TOKENIZER_ENGINE line %lld refused ref=%d goose=%lld\n", (long long) cases, (int) n_ref, (long long) n_g); continue; }
            ref_total += n_ref;
            goose_total += n_g;
            if (n_ref != n_g) { bad++; continue; }
            for (int32_t i = 0; i < n_ref; ++i) {
                if ((int64_t) ref[i] != gids[i]) { bad++; break; }
            }
        }
        fclose(in);
        llama_model_free(lm);
        printf("FG_TOKENIZER_ENGINE tokenize cases=%lld ref_ids=%lld goose_ids=%lld mismatches=%lld\n",
               (long long) cases, (long long) ref_total, (long long) goose_total, (long long) bad);
        return bad == 0 ? 0 : 1;
    }

    FILE * out = fopen(argv[2], "wb");
    if (!out) { perror("fopen"); return 2; }
    uint8_t buf[4096];
    int64_t total = 0;
    int bad = 0;
    for (int64_t id = 0; id < n_tok; ++id) {
        for (int special = 0; special <= 1; ++special) {
            const int64_t n = bt_token_to_piece((sl_u8){ blob, need[0] }, (sl_i64){ tok_off, n_tok },
                                                (sl_i64){ tok_len, n_tok }, (sl_i64){ attr, n_tok },
                                                id, (sl_u8){ buf, sizeof(buf) }, 0, 0, special, 0);
            if (n < 0) { bad++; continue; }
            const int32_t len = (int32_t) n;
            fwrite(&len, sizeof len, 1, out);
            if (n > 0) { fwrite(buf, 1, (size_t) n, out); }
            total += n;
        }
    }
    fclose(out);
    printf("FG_TOKENIZER_ENGINE tokens=%lld merges=%lld blob=%lld bytes=%lld refused=%d build_rc=%lld\n",
           (long long) n_tok, (long long) n_mrg, (long long) need[0], (long long) total, bad, (long long) build_rc);
    return bad == 0 ? 0 : 1;
}
