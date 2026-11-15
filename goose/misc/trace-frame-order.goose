// P10-12 S3: the order of one decode's trace frames (gap G3).
//
// Native source: runtime/core/src/engine/session.cpp at base commit 1b3f4a1a,
// the two lambdas at 1485-1530:
//
//   trace_begin(base_pos, n_tokens, phase)  — phase first, then the framing
//                                             calls, then the stamp
//   trace_flush()                           — compute close FIRST, then the
//                                             row handovers in a fixed order
//   trace_phase / trace_step                — what the I/O rows are stamped with
//
// The lambdas are ordered side effects over three sinks and one hook, so the
// portable part is the ORDER and the arguments, not the calls: the module
// returns the sequence as a plan and the host performs it. The framing exists
// because the I/O rows carry no frame of their own (a lane does not know which
// token it serves), and because at layer granularity trace_flush's own work
// would otherwise be billed to the LM head if the compute interval were not
// closed first. Both remarks are the native comments, and both are decisions
// the plan has to keep.
//
// The prefix is tr_. Events are five i64 slots: op, a0, a1, a2, a3.
//
//   op 1 SET_BATCH_PHASE        a0 = phase
//   op 2 BEGIN_DETERMINISTIC    a0 = base_pos, a1 = n_tokens, a2 = phase
//   op 3 BEGIN_TRACE_BATCH      a0 = base_pos, a1 = n_tokens, a2 = phase, a3 = turn
//   op 4 BEGIN_COMPUTE_BATCH    a0 = base_pos + n_tokens - 1, a1 = phase, a2 = turn
//   op 5 END_COMPUTE_BATCH      (no arguments)
//   op 6 END_TRACE_BATCH        (no arguments)
//   op 7 ROUTE_ROWS             a0 = row count (absent when the frame is empty)
//   op 8 COMPUTE_ROWS           a0 = row count
//   op 9 IO_TAKE_ROWS           a0 = row count the source handed over
//   op10 IO_STAMP_ROWS          a0 = row count, a1 = turn, a2 = phase, a3 = step
//   op11 IO_ROWS                a0 = row count (absent when the frame is empty)

const TR_SET_PHASE: i64 = 1;
const TR_BEGIN_DET: i64 = 2;
const TR_BEGIN_ROUTE: i64 = 3;
const TR_BEGIN_COMPUTE: i64 = 4;
const TR_END_COMPUTE: i64 = 5;
const TR_END_ROUTE: i64 = 6;
const TR_ROUTE_ROWS: i64 = 7;
const TR_COMPUTE_ROWS: i64 = 8;
const TR_IO_TAKE: i64 = 9;
const TR_IO_STAMP: i64 = 10;
const TR_IO_ROWS: i64 = 11;

fn tr_put(out: i64[:], at: i64, op: i64, a0: i64, a1: i64, a2: i64, a3: i64) -> i64 {
    if at + 4 < out.len {
        out[at] = op;
        out[at + 1] = a0;
        out[at + 2] = a1;
        out[at + 3] = a2;
        out[at + 4] = a3;
    }
    return at + 5;
}

// session.cpp:1485-1496 — trace_begin. The phase reaches the drop policy before
// any trace call, and the last position of the batch (not its first) is the
// step a prefill chunk's graph is attributed to.
export fn tr_begin(route: i64, compute: i64, base_pos: i64, n_tokens: i64, phase: i64, turn: i64,
                   out: i64[:]) -> i64 {
    var at: i64 = 0;
    at = tr_put(out, at, TR_SET_PHASE, phase, 0, 0, 0);
    at = tr_put(out, at, TR_BEGIN_DET, base_pos, n_tokens, phase, 0);
    if route != 0 {
        at = tr_put(out, at, TR_BEGIN_ROUTE, base_pos, n_tokens, phase, turn);
    }
    if compute != 0 {
        at = tr_put(out, at, TR_BEGIN_COMPUTE, base_pos + n_tokens - 1, phase, turn, 0);
    }
    return at / 5;
}

// The frame's stamp, which the I/O rows carry and the other traces do not:
// trace_phase = phase; trace_step = base_pos + n_tokens - 1.
export fn tr_stamp(base_pos: i64, n_tokens: i64, phase: i64, out: i64[:]) -> i64 {
    if out.len >= 2 {
        out[0] = phase;
        out[1] = base_pos + n_tokens - 1;
    }
    return 2;
}

// session.cpp:1497-1530 — trace_flush. The compute interval closes before
// anything else, the route rows leave before the compute rows, and the I/O
// rows are taken (then stamped, then handed over) last. An empty frame emits
// nothing at all — not an empty batch.
export fn tr_flush(route: i64, compute: i64, io: i64, route_rows: i64, compute_rows: i64, io_rows: i64,
                   phase: i64, step: i64, turn: i64, out: i64[:]) -> i64 {
    var at: i64 = 0;
    if compute != 0 {
        at = tr_put(out, at, TR_END_COMPUTE, 0, 0, 0, 0);
    }
    if route != 0 {
        at = tr_put(out, at, TR_END_ROUTE, 0, 0, 0, 0);
        if route_rows > 0 {
            at = tr_put(out, at, TR_ROUTE_ROWS, route_rows, 0, 0, 0);
        }
    }
    if compute != 0 && compute_rows > 0 {
        at = tr_put(out, at, TR_COMPUTE_ROWS, compute_rows, 0, 0, 0);
    }
    if io != 0 {
        if io_rows > 0 {
            at = tr_put(out, at, TR_IO_TAKE, io_rows, 0, 0, 0);
            at = tr_put(out, at, TR_IO_STAMP, io_rows, turn, phase, step);
            at = tr_put(out, at, TR_IO_ROWS, io_rows, 0, 0, 0);
        } else {
            at = tr_put(out, at, TR_IO_TAKE, 0, 0, 0, 0);
        }
    }
    return at / 5;
}
