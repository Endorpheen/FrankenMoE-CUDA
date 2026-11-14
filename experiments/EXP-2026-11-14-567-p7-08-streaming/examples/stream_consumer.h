// Host-side consumer for chat_stream_step: the caller contract in code.
//
// The caller owns every buffer, supplies the parse facts, and translates the
// step status into one action it can act on. See docs/stream-contract.md.
#ifndef STREAM_CONSUMER_H
#define STREAM_CONSUMER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SC_PUBLISHED        = 0, // 0  : send the message
    SC_KEPT             = 1, // 1  : nothing new
    SC_NEED_INPUT_SPACE = 2, // -1 : grow the input buffer, retry the SAME chunk
    SC_PROTOCOL         = 3, // -2 : caller bug, never retry
    SC_UNPARSED         = 4, // -3 : fail the request, send nothing
    SC_NEED_WORK_SPACE  = 5, // -4 : grow work buffers, retry with an EMPTY chunk
    SC_BAD_STATE        = 6, // -5 : internal bug, rebuild the session
    SC_REJECTED         = 7, // -6 : do not send the previous message as complete
    SC_UNKNOWN          = 8  // anything else, treat as SC_BAD_STATE
} sc_action;

// One slice of the flat ABI: data plus length.
typedef struct { const int64_t *p; int64_t n; } sc_islice;

// One step: the chunk plus the parse of the accumulated text after it.
typedef struct {
    const uint8_t *chunk;
    int64_t        chunk_len;
    int            is_final;
    int64_t        ptype; // 0 FAIL, 1 OK, 2 NEED_MORE
    int64_t        pend;  // result.end
    sc_islice      roots;
    sc_islice      a_tag;
    sc_islice      a_toff;
    sc_islice      a_tlen;
    sc_islice      a_cs;
    sc_islice      a_cc;
    sc_islice      achild;
    sc_islice      a_is;
    sc_islice      a_ic;
    sc_islice      inv;
    sc_islice      tid;
} sc_step;

typedef struct {
    uint8_t *inbuf;
    int64_t  in_cap;
    uint8_t *wcont;
    uint8_t *wreas;
    uint8_t *wtcb;
    int64_t  work_cap;
    uint8_t *rcont;
    uint8_t *rreas;
    uint8_t *rtcb;
    int64_t  res_cap;
    int64_t *mst;
    int64_t *mout;
    int64_t *rtab;
    int64_t *out;
    int64_t  st[8];
    int64_t  published; // number of status-0 answers
    int64_t  rejected;  // number of -6 answers
} sc_stream;

int  sc_init(sc_stream *s, int64_t in_cap, int64_t work_cap, int64_t res_cap);
void sc_free(sc_stream *s);

// Grow the input buffer (doubling), keeping the bytes [0, st[1]).
int sc_grow_input(sc_stream *s);
// Grow the work buffers (doubling). The result buffers are never grown here.
int sc_grow_work(sc_stream *s);

sc_action sc_feed(sc_stream *s, const sc_step *in);

// "The message may be sent as a final answer": no failure was ever recorded.
int     sc_ok(const sc_stream *s);
int     sc_done(const sc_stream *s);
int64_t sc_last_error(const sc_stream *s);
// Diagnostics and tests only: the raw state, e.g. to corrupt it on purpose.
int64_t *sc_state(sc_stream *s);

typedef struct {
    const uint8_t *content;
    int64_t        content_len;
    const uint8_t *reasoning;
    int64_t        reasoning_len;
    int64_t        ncalls;
} sc_msg_view;

void sc_message(const sc_stream *s, sc_msg_view *v);
// field: 0 name, 1 arguments, 2 id.
const uint8_t *sc_call_field(const sc_stream *s, int64_t i, int field, int64_t *len);

#ifdef __cplusplus
}
#endif

#endif
