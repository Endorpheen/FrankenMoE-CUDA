// One Goose unit, one process: HTTP in, real tokenisation out.
//
// This is the integration the earlier pieces were missing. The listener, the
// router, the GGUF reader, the vocabulary layout, the tokenizer and the trunk
// all live in the same composed unit; the host here supplies only buffers and
// the model file path. Every decision - which route, where the request line
// ends, where the vocabulary text lives, which ids the prompt becomes - is
// answered by Goose code in this binary.
//
// The endpoint is /tokenize: POST with the text as the body, and the answer
// carries the ids the Goose tokenizer produced. That makes an HTTP round trip
// observable end to end without a GPU and without libllama.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "unit.h"

#define HDR_CAP  (64u * 1024u * 1024u)
#define MAX_REQ  262144
#define MAX_IDS  65536

static const uint8_t * g_hdr = NULL;
static int64_t         g_hdr_len = 0;
static int64_t         g_state[8] = {0};
static int64_t *       g_kv = NULL, * g_kvsl = NULL;

static uint8_t * g_blob = NULL;
static int64_t    g_blob_len = 0;
static int64_t *  g_tok_off = NULL, * g_tok_len = NULL, * g_tok_slots = NULL;
static int64_t *  g_mrg_off = NULL, * g_mrg_len = NULL, * g_mrg_slots = NULL;
static int64_t *  g_order = NULL;
static int64_t    g_n_tok = 0, g_n_special = 0, g_n_tok_slots = 0, g_n_mrg_slots = 0;

// Scratch buffers for one tokenize call, sized for the request text.
static int64_t * g_cpts = NULL, * g_seg = NULL, * g_sym = NULL, * g_heap = NULL, * g_hc = NULL, * g_info = NULL;
static uint8_t * g_enc = NULL;

static int64_t find_key(const char * key) {
    int64_t n = 0;
    while (key[n]) { n++; }
    return gr_find_kv((sl_u8){ (unsigned char *) g_hdr, g_hdr_len }, (sl_i64){ g_state, 8 },
                      (sl_i64){ g_kv, 256 * 8 }, (sl_i64){ g_kvsl, 512 },
                      (sl_u8){ (unsigned char *) key, n });
}

static int load_vocabulary(const char * path) {
    const int fd = open(path, O_RDONLY);
    if (fd < 0) { perror("open"); return -1; }
    struct stat st;
    if (fstat(fd, &st) != 0) { perror("fstat"); return -1; }
    g_hdr_len = st.st_size < (int64_t) HDR_CAP ? (int64_t) st.st_size : (int64_t) HDR_CAP;
    void * m = mmap(NULL, (size_t) g_hdr_len, PROT_READ, MAP_PRIVATE, fd, 0);
    if (m == MAP_FAILED) { perror("mmap"); return -1; }
    g_hdr = (const uint8_t *) m;

    g_kv = calloc(256 * 8, sizeof(int64_t));
    g_kvsl = calloc(512, sizeof(int64_t));
    int64_t * tens = calloc(2048 * 12, sizeof(int64_t));
    int64_t * tensl = calloc(8192, sizeof(int64_t));

    if (gr_parse((sl_u8){ (unsigned char *) g_hdr, g_hdr_len }, (int64_t) st.st_size,
                 (sl_i64){ g_state, 8 }, (sl_i64){ g_kv, 256 * 8 }, (sl_i64){ tens, 2048 * 12 },
                 (sl_i64){ g_kvsl, 512 }, (sl_i64){ tensl, 8192 }) != 0) {
        printf("LOAD step=parse failed\n");
        return -1;
    }
    printf("LOAD parse ok keys=%lld tensors=%lld\n", (long long) g_state[2], (long long) g_state[3]);
    // Lengths are computed, never typed: a wrong length silently answers -1.
    const int64_t tok_i = find_key("tokenizer.ggml.tokens");
    const int64_t mrg_i = find_key("tokenizer.ggml.merges");
    const int64_t typ_i = find_key("tokenizer.ggml.token_type");
    if (tok_i < 0 || mrg_i < 0 || typ_i < 0) {
        printf("LOAD step=keys tok=%lld mrg=%lld typ=%lld\n", (long long) tok_i, (long long) mrg_i, (long long) typ_i);
        return -1;
    }
    printf("LOAD keys tok=%lld mrg=%lld typ=%lld\n", (long long) tok_i, (long long) mrg_i, (long long) typ_i);

    int64_t need[5] = {0};
    if (bt_prepare_need((sl_u8){ (unsigned char *) g_hdr, g_hdr_len }, (sl_i64){ g_state, 8 },
                        (sl_i64){ g_kv, 256 * 8 }, tok_i, mrg_i, (sl_i64){ need, 5 }) != 0) {
        printf("LOAD step=need failed\n");
        return -1;
    }
    printf("LOAD need bytes=%lld tok=%lld mrg=%lld slots=%lld/%lld\n", (long long) need[0],
           (long long) need[1], (long long) need[2], (long long) need[3], (long long) need[4]);
    g_n_tok = need[1];
    g_n_tok_slots = need[3];
    g_n_mrg_slots = need[4];
    g_blob_len = need[0];
    g_blob = malloc((size_t) g_blob_len);
    g_tok_off = calloc((size_t) g_n_tok, sizeof(int64_t));
    g_tok_len = calloc((size_t) g_n_tok, sizeof(int64_t));
    g_tok_slots = calloc((size_t) g_n_tok_slots, sizeof(int64_t));
    g_mrg_off = calloc((size_t) need[2], sizeof(int64_t));
    g_mrg_len = calloc((size_t) need[2], sizeof(int64_t));
    g_mrg_slots = calloc((size_t) g_n_mrg_slots, sizeof(int64_t));
    if (bt_prepare_from_gguf((sl_u8){ (unsigned char *) g_hdr, g_hdr_len }, (sl_i64){ g_state, 8 },
                             (sl_i64){ g_kv, 256 * 8 }, tok_i, mrg_i,
                             (sl_u8){ g_blob, g_blob_len },
                             (sl_i64){ g_tok_off, g_n_tok }, (sl_i64){ g_tok_len, g_n_tok },
                             (sl_i64){ g_tok_slots, g_n_tok_slots },
                             (sl_i64){ g_mrg_off, need[2] }, (sl_i64){ g_mrg_len, need[2] },
                             (sl_i64){ g_mrg_slots, g_n_mrg_slots }, (sl_i64){ need, 5 }) != 0) {
        printf("LOAD step=prepare_from_gguf failed\n");
        return -1;
    }

    int64_t * attr = calloc((size_t) g_n_tok, sizeof(int64_t));
    int64_t * ttype = calloc((size_t) g_n_tok, sizeof(int64_t));
    int64_t span[4] = {0};
    if (gr_val_span((sl_i64){ g_state, 8 }, (sl_i64){ g_kv, 256 * 8 }, typ_i, (sl_i64){ span, 4 }) != 0) {
        printf("LOAD step=token_type span failed\n");
        return -1;
    }
    const int32_t * tt = (const int32_t *) (g_hdr + span[1]);
    for (int64_t i = 0; i < g_n_tok; ++i) { ttype[i] = tt[i]; }
    if (bt_attrs((sl_u8){ g_blob, g_blob_len }, (sl_i64){ g_tok_off, g_n_tok }, (sl_i64){ g_tok_len, g_n_tok },
                 (sl_i64){ ttype, g_n_tok }, (sl_i64){ attr, g_n_tok }) != 0) {
        printf("LOAD step=attrs failed\n");
        return -1;
    }
    int64_t stats[4] = {0};
    if (bt_build((sl_u8){ g_blob, g_blob_len }, (sl_i64){ g_tok_off, g_n_tok }, (sl_i64){ g_tok_len, g_n_tok },
                 (sl_i64){ g_tok_slots, g_n_tok_slots }, (sl_i64){ g_mrg_off, need[2] },
                 (sl_i64){ g_mrg_len, need[2] }, (sl_i64){ g_mrg_slots, g_n_mrg_slots },
                 (sl_i64){ stats, 4 }) != 0) {
        printf("LOAD step=bt_build failed\n");
        return -1;
    }
    g_order = calloc((size_t) g_n_tok, sizeof(int64_t));
    g_n_special = bt_special_order((sl_u8){ g_blob, g_blob_len }, (sl_i64){ g_tok_off, g_n_tok },
                                   (sl_i64){ g_tok_len, g_n_tok }, (sl_i64){ attr, g_n_tok },
                                   (sl_i64){ g_order, g_n_tok });
    if (g_n_special < 0) { g_n_special = 0; }

    const int64_t cap = MAX_REQ + 1;
    g_cpts = calloc((size_t) cap, sizeof(int64_t));
    g_seg = calloc(4 * (size_t) cap, sizeof(int64_t));
    g_enc = calloc(8 * (size_t) cap, 1);
    g_sym = calloc(24 * (size_t) cap, sizeof(int64_t));
    g_heap = calloc(15 * (size_t) cap + 40, sizeof(int64_t));
    g_hc = calloc(2, sizeof(int64_t));
    g_info = calloc(8, sizeof(int64_t));
    return 0;
}

static int64_t tokenize(const unsigned char * text, int64_t len, int64_t * ids) {
    return bt_tokenize((sl_u8){ g_blob, g_blob_len },
                       (sl_i64){ g_tok_off, g_n_tok_slots }, (sl_i64){ g_tok_len, g_n_tok_slots },
                       (sl_i64){ g_tok_slots, g_n_tok_slots },
                       (sl_i64){ g_mrg_off, g_n_mrg_slots }, (sl_i64){ g_mrg_len, g_n_mrg_slots },
                       (sl_i64){ g_mrg_slots, g_n_mrg_slots },
                       (sl_i64){ g_order, g_n_tok }, g_n_special,
                       (sl_u8){ (unsigned char *) text, len }, (sl_i64){ ids, MAX_IDS }, 0,
                       (sl_i64){ g_cpts, MAX_REQ + 1 }, (sl_i64){ g_seg, 4 * (MAX_REQ + 1) },
                       (sl_u8){ g_enc, 8 * (MAX_REQ + 1) },
                       (sl_i64){ g_sym, 24 * (MAX_REQ + 1) }, (sl_i64){ g_heap, 15 * (MAX_REQ + 1) + 40 },
                       (sl_i64){ g_hc, 2 }, (sl_i64){ g_info, 8 });
}

static int serve(int64_t conn) {
    static unsigned char req[MAX_REQ];
    static unsigned char head[512];
    static unsigned char body[MAX_REQ];
    static unsigned char path[1024];
    static int64_t ids[MAX_IDS];

    int64_t info[4] = {0, 0, 0, 0};
    const int64_t n = ht_read_request(conn, (sl_u8){ req, MAX_REQ }, (sl_i64){ info, 4 });
    if (n <= 0) { return -1; }
    const int64_t method = info[1], poff = info[2], plen = info[3];
    if (poff < 0 || plen < 0 || poff + plen > (int64_t) sizeof(path)) { return -1; }
    memcpy(path, req + poff, (size_t) plen);

    const int64_t route = route_id(method, (sl_u8){ path, plen });
    const int64_t keep = ht_keep_alive((sl_u8){ req, n });

    int64_t blen = 0;
    int64_t status = 200;
    if (plen == 9 && memcmp(path, "/tokenize", 9) == 0 && method == 2) {
        const unsigned char * text = req + info[0];
        const int64_t tlen = n - info[0];
        const int64_t cnt = tokenize(text, tlen, ids);
        if (cnt < 0) {
            blen = (int64_t) sprintf((char *) body, "{\"error\":\"tokenize failed\"}");
            status = 500;
        } else {
            int64_t at = (int64_t) sprintf((char *) body, "{\"ids\":[");
            for (int64_t i = 0; i < cnt && at < (int64_t) sizeof(body) - 24; ++i) {
                at += (int64_t) sprintf((char *) body + at, i ? ",%lld" : "%lld", (long long) ids[i]);
            }
            at += (int64_t) sprintf((char *) body + at, "],\"count\":%lld}", (long long) cnt);
            blen = at;
        }
    } else if (route > 0) {
        blen = (int64_t) sprintf((char *) body, "{\"route\":%lld}", (long long) route);
    } else {
        status = 404;
        blen = (int64_t) sprintf((char *) body, "{\"error\":{\"code\":404}}");
    }

    const int64_t hl = ht_response_head(status, blen, keep, (sl_u8){ head, sizeof(head) });
    if (hl < 0) { return -1; }
    if (ht_write_all(conn, (sl_u8){ head, hl }) != 0) { return -1; }
    if (ht_write_all(conn, (sl_u8){ body, blen }) != 0) { return -1; }

    printf("REQ method=%lld path=%.*s route=%lld status=%lld\n",
           (long long) method, (int) plen, path, (long long) route, (long long) status);
    return keep ? 0 : 1;
}

int main(int argc, char ** argv) {
    goose_init(0, NULL);
    if (argc < 3) {
        fprintf(stderr, "usage: %s <model.gguf> <port> [max_requests]\n", argv[0]);
        return 2;
    }
    if (load_vocabulary(argv[1]) != 0) {
        printf("SERVER vocabulary load failed\n");
        return 1;
    }
    printf("SERVER vocabulary tokens=%lld special=%lld\n", (long long) g_n_tok, (long long) g_n_special);
    fflush(stdout);

    const int64_t port = atoll(argv[2]);
    const int64_t max_req = argc > 3 ? atoll(argv[3]) : 1;
    int64_t opts[1] = { 0 };
    const int64_t lfd = ht_open(port, (sl_i64){ opts, 1 });
    if (lfd < 0) {
        printf("SERVER open failed step=%lld\n", (long long) (-lfd));
        return 1;
    }
    printf("SERVER listening fd=%lld port=%lld\n", (long long) lfd, (long long) port);
    fflush(stdout);

    static unsigned char addr[16];
    int64_t alen[1] = { 16 };
    int64_t ainfo[2] = { 0, 0 };
    int64_t served = 0;
    while (served < max_req) {
        const int64_t conn = ht_accept(lfd, (sl_u8){ addr, 16 }, (sl_i64){ alen, 1 }, (sl_i64){ ainfo, 2 });
        if (conn < 0) { break; }
        served++;
        serve(conn);
        ht_close(conn);
    }
    ht_close(lfd);
    printf("SERVER done served=%lld\n", (long long) served);
    return 0;
}
