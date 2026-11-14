// Host-side consumer for chat_stream_step: the caller contract in code.
//
// Rules implemented here, one per status (docs/stream-contract.md):
//   0  -> SC_PUBLISHED        the caller sends the new message
//   1  -> SC_KEPT             nothing to send
//  -1  -> SC_NEED_INPUT_SPACE the chunk was NOT appended; retry it after growth
//  -2  -> SC_PROTOCOL         caller bug; never retry
//  -3  -> SC_UNPARSED         final parse failure; send nothing
//  -4  -> SC_NEED_WORK_SPACE  the chunk IS appended; retry with an EMPTY chunk
//  -5  -> SC_BAD_STATE        the published state is not a valid message
//  -6  -> SC_REJECTED         never send the kept message as a completed answer
// Two slots matter: st[5] is this step, st[7] is the last error ever. A final
// answer is only safe when st[7] == 0, which sc_ok() reports.
#include "stream_consumer.h"
#include "gen.h"

#include <stdlib.h>
#include <string.h>

int sc_init(sc_stream *s, int64_t in_cap, int64_t work_cap, int64_t res_cap) {
    memset(s, 0, sizeof *s);
    s->in_cap   = in_cap;
    s->work_cap = work_cap;
    s->res_cap  = res_cap;
    s->inbuf = calloc((size_t) in_cap, 1);
    s->wcont = calloc((size_t) work_cap, 1);
    s->wreas = calloc((size_t) work_cap, 1);
    s->wtcb  = calloc((size_t) work_cap, 1);
    s->rcont = calloc((size_t) res_cap, 1);
    s->rreas = calloc((size_t) res_cap, 1);
    s->rtcb  = calloc((size_t) res_cap, 1);
    s->mst   = calloc(24, sizeof(int64_t));
    s->mout  = calloc(3 + 6 * 64, sizeof(int64_t));
    s->rtab  = calloc(6 * 64, sizeof(int64_t));
    s->out   = calloc(3 + 6 * 64, sizeof(int64_t));
    if (!s->inbuf || !s->wcont || !s->wreas || !s->wtcb || !s->rcont || !s->rreas ||
        !s->rtcb || !s->mst || !s->mout || !s->rtab || !s->out) {
        return -1;
    }
    sl_i64 st_s = {s->st, 8};
    chat_stream_open(st_s, 0);
    return 0;
}

void sc_free(sc_stream *s) {
    free(s->inbuf);
    free(s->wcont);
    free(s->wreas);
    free(s->wtcb);
    free(s->rcont);
    free(s->rreas);
    free(s->rtcb);
    free(s->mst);
    free(s->mout);
    free(s->rtab);
    free(s->out);
    memset(s, 0, sizeof *s);
}

int sc_grow_input(sc_stream *s) {
    const int64_t cap = s->in_cap * 2;
    uint8_t *p = calloc((size_t) cap, 1);
    if (!p) {
        return -1;
    }
    // The accumulated bytes live in [0, st[1]); they must survive the move.
    if (s->st[1] > 0) {
        memcpy(p, s->inbuf, (size_t) s->st[1]);
    }
    free(s->inbuf);
    s->inbuf  = p;
    s->in_cap = cap;
    return 0;
}

int sc_grow_work(sc_stream *s) {
    const int64_t cap = s->work_cap * 2;
    uint8_t *c = calloc((size_t) cap, 1);
    uint8_t *r = calloc((size_t) cap, 1);
    uint8_t *t = calloc((size_t) cap, 1);
    if (!c || !r || !t) {
        free(c);
        free(r);
        free(t);
        return -1;
    }
    free(s->wcont);
    free(s->wreas);
    free(s->wtcb);
    s->wcont    = c;
    s->wreas    = r;
    s->wtcb     = t;
    s->work_cap = cap;
    return 0;
}

sc_action sc_feed(sc_stream *s, const sc_step *in) {
    sl_i64 st_s = {s->st, 8};
    sl_u8 in_s = {s->inbuf, s->in_cap};
    sl_u8 chunk_s = {(uint8_t *) in->chunk, in->chunk_len};
    sl_i64 roots_s = {(int64_t *) in->roots.p, in->roots.n};
    sl_i64 a_tag_s = {(int64_t *) in->a_tag.p, in->a_tag.n};
    sl_i64 a_toff_s = {(int64_t *) in->a_toff.p, in->a_toff.n};
    sl_i64 a_tlen_s = {(int64_t *) in->a_tlen.p, in->a_tlen.n};
    sl_i64 a_cs_s = {(int64_t *) in->a_cs.p, in->a_cs.n};
    sl_i64 a_cc_s = {(int64_t *) in->a_cc.p, in->a_cc.n};
    sl_i64 achild_s = {(int64_t *) in->achild.p, in->achild.n};
    sl_i64 a_is_s = {(int64_t *) in->a_is.p, in->a_is.n};
    sl_i64 a_ic_s = {(int64_t *) in->a_ic.p, in->a_ic.n};
    sl_i64 inv_s = {(int64_t *) in->inv.p, in->inv.n};
    sl_i64 tid_s = {(int64_t *) in->tid.p, in->tid.n};
    sl_i64 mst_s = {s->mst, 24};
    sl_i64 mout_s = {s->mout, 3 + 6 * 64};
    sl_u8 wcont_s = {s->wcont, s->work_cap};
    sl_u8 wreas_s = {s->wreas, s->work_cap};
    sl_u8 wtcb_s = {s->wtcb, s->work_cap};
    sl_u8 rcont_s = {s->rcont, s->res_cap};
    sl_u8 rreas_s = {s->rreas, s->res_cap};
    sl_u8 rtcb_s = {s->rtcb, s->res_cap};
    sl_i64 rtab_s = {s->rtab, 6 * 64};
    sl_i64 out_s = {s->out, 3 + 6 * 64};

    const int64_t status = chat_stream_step(
        st_s, in_s, chunk_s, in->is_final ? 1 : 0, in->ptype, in->pend, roots_s,
        a_tag_s, a_toff_s, a_tlen_s, a_cs_s, a_cc_s, achild_s, a_is_s, a_ic_s, inv_s, tid_s,
        mst_s, wcont_s, wreas_s, wtcb_s, mout_s, rcont_s, rreas_s, rtcb_s, rtab_s, out_s);

    switch (status) {
        case 0:
            s->published++;
            return SC_PUBLISHED;
        case 1:
            return SC_KEPT;
        case -1:
            // Not appended: the caller grows the input and repeats the chunk.
            return SC_NEED_INPUT_SPACE;
        case -2:
            return SC_PROTOCOL;
        case -3:
            // Final parse failure: nothing may be sent for this request.
            return SC_UNPARSED;
        case -4:
            // Already appended: the retry must use an empty chunk.
            return SC_NEED_WORK_SPACE;
        case -5:
            return SC_BAD_STATE;
        case -6:
            // The kept message is stale; it must not be sent as a completed
            // answer. The session stays usable, the status recurs while the
            // tail stays non-monotone.
            s->rejected++;
            return SC_REJECTED;
        default:
            return SC_UNKNOWN;
    }
}

int sc_ok(const sc_stream *s) {
    return s->st[7] == 0;
}

int sc_done(const sc_stream *s) {
    return s->st[0] != 0;
}

int64_t sc_last_error(const sc_stream *s) {
    return s->st[7];
}

int64_t *sc_state(sc_stream *s) {
    return s->st;
}

void sc_message(const sc_stream *s, sc_msg_view *v) {
    v->content       = s->rcont;
    v->content_len   = s->st[2];
    v->reasoning     = s->rreas;
    v->reasoning_len = s->st[3];
    v->ncalls        = s->st[4];
}

const uint8_t *sc_call_field(const sc_stream *s, int64_t i, int field, int64_t *len) {
    if (i < 0 || i >= s->st[4]) {
        *len = 0;
        return NULL;
    }
    const int64_t base = i * 6 + 2 * field;
    *len = s->rtab[base + 1];
    return s->rtcb + s->rtab[base];
}
