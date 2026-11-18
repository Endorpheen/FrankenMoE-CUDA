// HTTP front end: the listener and router are Goose, the trunk host is shared
// with the shipping binary (goose-host-llm.c).
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "unit.h"
#include "goose-host-llm.h"

#define MAX_REQ   262144
#define MAX_IDS   65536
#define MAX_TOKENS 256

// ---- generation over the listener ----------------------------------------

static int64_t g_served = 0;
static int64_t g_stats[5] = {0};
#define stats g_stats

static double now_s(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double) ts.tv_sec + (double) ts.tv_nsec / 1e9;
}

int64_t generation_serve(int64_t conn) {
    static unsigned char req[MAX_REQ], head[512], body[MAX_REQ * 2], path[1024];
    static int64_t ids[MAX_IDS], out_ids[MAX_TOKENS];

    int64_t info[4] = {0, 0, 0, 0};
    const int64_t n = ht_read_request(conn, (sl_u8){ req, MAX_REQ }, (sl_i64){ info, 4 });
    if (n <= 0) return -1;
    const int64_t method = info[1], poff = info[2], plen = info[3];
    if (poff < 0 || plen < 0 || poff + plen > (int64_t) sizeof(path)) return -1;
    memcpy(path, req + poff, (size_t) plen);

    const int64_t route = route_id(method, (sl_u8){ path, plen });
    const int64_t keep = ht_keep_alive((sl_u8){ req, n });
    int64_t blen = 0, status = 200;

    const int is_completion = (plen == 11 && memcmp(path, "/completion", 11) == 0 && method == 2);
    if (is_completion) {
        // Requests are independent: the KV bookkeeping starts from zero for each
        // one, so a second prompt never sees the first prompt's positions.
        hose_reset_state();
        const int64_t cnt = hose_tokenize(req + info[0], n - info[0], ids, MAX_IDS);
        if (cnt <= 0) {
            blen = (int64_t) sprintf((char *) body, "{\"error\":\"tokenize failed\"}");
            status = 400;
        } else {
            float * logits = NULL;
            int64_t n_logits = 0, produced = 0;
            int32_t * batch = malloc((size_t) cnt * 4);
            for (int64_t i = 0; i < cnt; ++i) batch[i] = (int32_t) ids[i];

            int64_t past = cnt;
            const double t0 = now_s();
            double t_prefill = 0.0;
            for (int64_t step = 0; step < MAX_TOKENS; ++step) {
                n_logits = hose_step(batch, past, &logits);
                free(batch);
                if (n_logits <= 0) { printf("STEP failed at %lld past=%lld\n", (long long) step, (long long) past); fflush(stdout); break; }
                if (step == 0) t_prefill = now_s() - t0;
                // logits are [n_vocab, n_tokens]; the next token comes from the
                // last position's row, and sm_greedy takes the count as its
                // second argument.
                const int64_t vocab = n_logits / (past > 0 ? past : 1);
                if (vocab <= 0) break;
                const float * row = logits + (int64_t) (past - 1) * vocab;
                const int64_t next = sm_greedy((sl_f32){ (float *) row, vocab }, vocab);
                free(logits);
                logits = NULL;
                if (next < 0) { printf("SAMPLE failed\n"); fflush(stdout); break; }
                out_ids[produced++] = next;
                if (hose_eog_is(next) != 0) break;
                batch = malloc(4);
                batch[0] = (int32_t) next;
                past = 1;
            }

            const double t_all = now_s() - t0;
            const double t_dec = t_all - t_prefill;
            printf("TIME prompt=%lld prefill_s=%.2f prefill_tok_s=%.1f decode_n=%lld decode_s=%.2f decode_tok_s=%.2f\n",
                   (long long) cnt, t_prefill, cnt > 0 ? (double) cnt / (t_prefill > 0 ? t_prefill : 1e-9) : 0.0,
                   (long long) (produced > 0 ? produced - 1 : 0), t_dec,
                   (produced > 1 && t_dec > 0) ? (double) (produced - 1) / t_dec : 0.0);
            fflush(stdout);

            unsigned char text[MAX_REQ];
            const int64_t tlen = produced ? hose_detokenize(out_ids, produced, text, sizeof text) : 0;
            int64_t at = (int64_t) sprintf((char *) body, "{\"ids\":[");
            for (int64_t i = 0; i < produced && at < (int64_t) sizeof(body) - 40; ++i)
                at += (int64_t) sprintf((char *) body + at, i ? ",%lld" : "%lld", (long long) out_ids[i]);
            at += (int64_t) sprintf((char *) body + at, "],\"text\":\"");
            for (int64_t i = 0; i < tlen && at < (int64_t) sizeof(body) - 12; ++i) {
                const unsigned char ch = text[i];
                if (ch == '"' || ch == '\\') { body[at++] = '\\'; body[at++] = ch; }
                else if (ch == 10) { body[at++] = '\\'; body[at++] = 'n'; }
                else if (ch == 13) { body[at++] = '\\'; body[at++] = 'r'; }
                else if (ch == 9)  { body[at++] = '\\'; body[at++] = 't'; }
                else if (ch < 32)  { at += (int64_t) sprintf((char *) body + at, "\\u%04x", ch); }
                else body[at++] = ch;
            }
            at += (int64_t) sprintf((char *) body + at, "\",\"count\":%lld}", (long long) produced);
            blen = at;
        }
    } else if (plen == 6 && memcmp(path, "/props", 6) == 0) {
        blen = (int64_t) sprintf((char *) body,
            "{\"n_vocab\":%lld,\"n_special\":%lld,\"n_eog\":%lld,\"n_layer\":%lld}",
            (long long) stats[0], (long long) stats[1], (long long) stats[2], (long long) stats[4]);
    } else if (route > 0) {
        blen = (int64_t) sprintf((char *) body, "{\"route\":%lld}", (long long) route);
    } else {
        status = 404;
        blen = (int64_t) sprintf((char *) body, "{\"error\":{\"code\":404}}");
    }

respond:
    {
    const int64_t hl = ht_response_head(status, blen, keep, (sl_u8){ head, sizeof(head) });
    if (hl < 0) return -1;
    if (ht_write_all(conn, (sl_u8){ head, hl }) != 0) return -1;
    if (ht_write_all(conn, (sl_u8){ body, blen }) != 0) return -1;
    printf("REQ method=%lld path=%.*s status=%lld\n", (long long) method, (int) plen, path, (long long) status);
    return keep ? 0 : 1;
    }
}

int main(int argc, char ** argv) {
    goose_init(0, NULL);
    if (argc < 3) {
        fprintf(stderr, "usage: %s <model.gguf> <port> [max_requests]\n", argv[0]);
        return 2;
    }
    hose_host_load(argv[1]);
    hose_stats(stats);
    printf("SERVER vocab=%lld special=%lld eog=%lld layers=%lld\n",
           (long long) stats[0], (long long) stats[1], (long long) stats[2], (long long) stats[4]);
    fflush(stdout);

    const int64_t max_req = argc > 3 ? atoll(argv[3]) : 1;
    int64_t opts[1] = { 0 };
    const int64_t lfd = ht_open(atoll(argv[2]), (sl_i64){ opts, 1 });
    if (lfd < 0) { printf("SERVER open failed step=%lld\n", (long long) (-lfd)); return 1; }
    printf("SERVER listening fd=%lld\n", (long long) lfd);
    fflush(stdout);

    static unsigned char addr[16];
    int64_t alen[1] = { 16 }, ainfo[2] = { 0, 0 };
    while (g_served < max_req) {
        const int64_t conn = ht_accept(lfd, (sl_u8){ addr, 16 }, (sl_i64){ alen, 1 }, (sl_i64){ ainfo, 2 });
        if (conn < 0) break;
        g_served++;
        generation_serve(conn);
        ht_close(conn);
    }
    ht_close(lfd);
    printf("SERVER done served=%lld\n", (long long) g_served);
    return 0;
}
