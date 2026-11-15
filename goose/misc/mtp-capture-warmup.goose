// P9-08 G4: the capture warm-up and the harvested expert table (gap G4).
//
// Native source: runtime/core/src/engine/session.cpp at base commit 050354e9,
// the streaming block of Session::open():
//
//   992-1008   the warm-up: begin_capture, one decode, end_capture
//   1010-1037  the harvest: gguf offsets, the captured layers, every bound
//              layer's expert tensors, the expert-count cross-check
//   1129-1130  the KV wipe that discards the warm-up
//
// This is the order that makes streaming possible at all: the eval callback can
// only hand out expert tensor pointers for a graph it has already seen, so one
// mmap-resident decode is run before anything is streamed. Any token builds the
// same expert structure (it is prompt-independent), so the native code uses BOS
// and falls back to 0 when the vocabulary has no BOS. The MTP case is not a
// variation of the dense one, it is a second pass: the head's graph exists only
// on the draft context and only process() issues it, so without the extra
// decode the head would stay unbound and silently mmap-resident — the one thing
// streaming exists to prevent on a model that does not fit.
//
// The intermediate content (which tensor names the graph touched, the gguf
// offset maps, the streamer's own buffers) is substrate and stays native. The
// portable part is the ORDER, the gate conditions and the audit rules:
//
//   * the warm-up event sequence, including the MTP-only pair;
//   * which failure closes the open (decode, draft process, offsets, missing
//     tensor, missing offset, inconsistent expert count, no bound layer);
//   * the harvest walk: an unbound layer is skipped whole, an architecture-unused
//     projection slot is skipped, a bound slot without a tensor is fatal, a slot
//     without a gguf offset is fatal, the first tensor's ne[2] is adopted as the
//     expert count and every later one must equal it;
//   * which captured weights enter the streamer's dense list and which are
//     dropped (experts are streamed, graph inputs and KV are not gguf tensors,
//     device-resident weights stay where they are), and which of those are also
//     row-streamed;
//   * the KV wipe after the warm-up: the target always, the draft only when it
//     exists.
//
// The prefix is mw_. Events are (op, a0, a1) triples for the warm-up and
// (op, layer, slot, x, y) five-tuples for the harvest; both plans say which
// failure closes the open instead of carrying the message text, which stays the
// carrier's.

// ── warm-up ────────────────────────────────────────────────────────────────
const MW_BEGIN_CAPTURE: i64 = 1;
const MW_FILL_VERIFY: i64 = 2;  // batch_fill(mtp_batch, [tok], 1, pos0=0, all_logits=true): a0 pos0, a1 all_logits
const MW_DECODE_VERIFY: i64 = 3; // llama_decode(ctx, mtp_batch): a0 = positions given (1)
const MW_SPEC_PROCESS: i64 = 4;  // common_speculative_process(mtp, mtp_batch)
const MW_DECODE_ONE: i64 = 5;    // llama_decode(ctx, llama_batch_get_one([tok], 1))
const MW_END_CAPTURE: i64 = 6;
const MW_CLEAR_TARGET: i64 = 7;
const MW_CLEAR_DRAFT: i64 = 8;

// Failures of the warm-up, in the order the native code can reach them.
const MW_FAIL_NONE: i64 = 0;
const MW_FAIL_DECODE: i64 = 1;
const MW_FAIL_PROCESS: i64 = 2;

// session.cpp:993-994 — BOS, or 0 when the vocabulary has none. A negative id
// is never a decode input; 0 is a valid token in every vocabulary this engine
// has been run on.
export fn mw_warm_token(bos: i64) -> i64 {
    if bos < 0 {
        return 0;
    }
    return bos;
}

fn mw_put3(out: i64[:], at: i64, op: i64, a0: i64, a1: i64) -> i64 {
    if at + 2 < out.len {
        out[at] = op;
        out[at + 1] = a0;
        out[at + 2] = a1;
    }
    return at + 3;
}

// session.cpp:992-1008 + 1129-1130 — the warm-up sequence and the wipe after it.
// moe is cfg.moe.enabled (the whole block), mtp is cfg.spec.is_mtp(), has_draft
// is im.ctx_dft != nullptr (true for every MTP session, false otherwise).
//
// The MTP pass is a second decode on the SAME batch: the target builds the
// trust graph with all_logits on, then the draft context processes the very same
// batch. Capturing one and not the other leaves half the model unbound.
fn mw_warm_plan_impl(moe: i64, mtp: i64, has_draft: i64, out: i64[:]) -> i64 {
    var at: i64 = 0;
    if moe == 0 {
        return 0;
    }
    at = mw_put3(out, at, MW_BEGIN_CAPTURE, 0, 0);
    if mtp != 0 {
        at = mw_put3(out, at, MW_FILL_VERIFY, 0, 1);
        at = mw_put3(out, at, MW_DECODE_VERIFY, 1, 0);
        at = mw_put3(out, at, MW_SPEC_PROCESS, 0, 0);
    } else {
        at = mw_put3(out, at, MW_DECODE_ONE, 1, 0);
    }
    at = mw_put3(out, at, MW_END_CAPTURE, 0, 0);
    at = mw_put3(out, at, MW_CLEAR_TARGET, 0, 0);
    if has_draft != 0 {
        at = mw_put3(out, at, MW_CLEAR_DRAFT, 0, 0);
    }
    return at / 3;
}

export fn mw_warm_plan(moe: i64, mtp: i64, has_draft: i64, out: i64[:]) -> i64 {
    return mw_warm_plan_impl(moe, mtp, has_draft, out);
}

// Which failure closes the open. A failed MTP decode is reported before the
// process call is even attempted (the native `return fail(...)` is inside the
// same if), and the draft process is only checked on the MTP path: on the dense
// path process_ok is 1 by definition and must not turn into a failure.
export fn mw_warm_fail(decode_rc: i64, process_ok: i64, mtp: i64) -> i64 {
    if decode_rc != 0 {
        return MW_FAIL_DECODE;
    }
    if mtp != 0 && process_ok == 0 {
        return MW_FAIL_PROCESS;
    }
    return MW_FAIL_NONE;
}

// ── harvest ────────────────────────────────────────────────────────────────
const MW_SKIP_LAYER: i64 = 0;
const MW_VISIT: i64 = 1;
const MW_FAIL_NO_TENSOR: i64 = 2;
const MW_FAIL_NO_OFFSET: i64 = 3;
const MW_ADOPT_COUNT: i64 = 4;
const MW_CHECK_COUNT: i64 = 5;
const MW_FAIL_INCONSISTENT: i64 = 6;
const MW_TOTAL_BOUND: i64 = 7;
const MW_TOTAL_EXPERTS: i64 = 8;

const MW_BAD_SLOT: i64 = -2;

// session.cpp:1013-1037 — one projection slot of one bound layer.
// Returns MW_SKIP_LAYER for an unbound layer, MW_NONE_SLOT for an unused slot,
// then the failure or the visit. The native order inside the slot is: unused
// check, tensor check, offset lookup, assign, count cross-check.
export fn mw_slot_step(bound: i64, used: i64, has_tensor: i64, has_off: i64) -> i64 {
    if bound == 0 {
        return MW_SKIP_LAYER;
    }
    if used == 0 {
        return MW_BAD_SLOT;
    }
    if has_tensor == 0 {
        return MW_FAIL_NO_TENSOR;
    }
    if has_off == 0 {
        return MW_FAIL_NO_OFFSET;
    }
    return MW_VISIT;
}

// session.cpp:1044-1049 — the expert count is adopted from the first tensor and
// pinned by every following one.
export fn mw_count_step(cur: i64, ne2: i64) -> i64 {
    if cur == 0 {
        return MW_ADOPT_COUNT;
    }
    if ne2 == cur {
        return MW_CHECK_COUNT;
    }
    return MW_FAIL_INCONSISTENT;
}

// session.cpp:1037 — one bound layer is the minimum: a capture that bound
// nothing means the graph had no expert tensors, i.e. the model is not MoE (or
// the hook was never reached).
export fn mw_bound_gate(n_bound: i64) -> i64 {
    if n_bound > 0 {
        return 1;
    }
    return 0;
}

// session.cpp:1011-1012 — no gguf offsets, no streaming: the harvest cannot map
// a tensor to a file range without them.
export fn mw_offsets_gate(offs_ok: i64) -> i64 {
    if offs_ok != 0 {
        return 1;
    }
    return 0;
}

// The harvest as one plan, so the carrier walks it instead of nesting two loops.
//
// Inputs are flat per-layer and per-layer-slot arrays (index = layer*nslot + slot):
//   bound[nl], used[nslot], has_t[nl*nslot], off[nl*nslot], ne2[nl*nslot].
// Output is (op, layer, slot, x, y) five-tuples, in native order:
//   MW_SKIP_LAYER (l, 0)          unbound layer
//   MW_VISIT (l, p, offset, ne2)  slot harvested and counted
//   MW_FAIL_NO_TENSOR (l, p)      bound, used, tensor missing
//   MW_FAIL_NO_OFFSET (l, p)      tensor present, no gguf offset
//   MW_ADOPT_COUNT (l, p, ne2)    first tensor set the count
//   MW_CHECK_COUNT (l, p, ne2)    later tensor matched it
//   MW_FAIL_INCONSISTENT (l, p, ne2, expected)
//   MW_TOTAL_BOUND (n_bound) / MW_TOTAL_EXPERTS (n_expert) close the plan.
// A failure op is the last event: the native walk returns there.
fn mw_walk_impl(nl: i64, nslot: i64, bound: const i64[:], used: const i64[:],
               has_t: const i64[:], off: const i64[:], ne2: const i64[:], out: i64[:]) -> i64 {
    var at: i64 = 0;
    var n_bound: i64 = 0;
    var n_expert: i64 = 0;
    var l: i64 = 0;
    while l < nl {
        if bound[l] == 0 {
            if at + 4 < out.len {
                out[at] = MW_SKIP_LAYER;
                out[at + 1] = l;
                out[at + 2] = 0;
                out[at + 3] = 0;
                out[at + 4] = 0;
            }
            at = at + 5;
            l = l + 1;
            continue;
        }
        n_bound = n_bound + 1;
        var p: i64 = 0;
        while p < nslot {
            if used[p] == 0 {
                p = p + 1;
                continue;
            }
            let idx: i64 = l * nslot + p;
            var has: i64 = 0;
            var o: i64 = 0;
            var e: i64 = 0;
            if idx < has_t.len {
                has = has_t[idx];
            }
            if idx < off.len {
                o = off[idx];
            }
            if idx < ne2.len {
                e = ne2[idx];
            }
            var op: i64 = MW_VISIT;
            if has == 0 {
                op = MW_FAIL_NO_TENSOR;
            } else if o == 0 {
                op = MW_FAIL_NO_OFFSET;
            }
            var x: i64 = 0;
            var y: i64 = 0;
            if op == MW_VISIT {
                let c: i64 = mw_count_step(n_expert, e);
                if c == MW_ADOPT_COUNT {
                    n_expert = e;
                    op = MW_ADOPT_COUNT;
                } else if c == MW_CHECK_COUNT {
                    op = MW_CHECK_COUNT;
                } else {
                    op = MW_FAIL_INCONSISTENT;
                }
                x = o;
                y = e;
                if op == MW_FAIL_INCONSISTENT {
                    x = e;
                    y = n_expert;
                }
            }
            if at + 4 < out.len {
                out[at] = op;
                out[at + 1] = l;
                out[at + 2] = p;
                out[at + 3] = x;
                out[at + 4] = y;
            }
            at = at + 5;
            if op == MW_FAIL_NO_TENSOR || op == MW_FAIL_NO_OFFSET || op == MW_FAIL_INCONSISTENT {
                return at / 5;
            }
            p = p + 1;
        }
        l = l + 1;
    }
    if at + 4 < out.len {
        out[at] = MW_TOTAL_BOUND;
        out[at + 1] = n_bound;
        out[at + 2] = 0;
        out[at + 3] = 0;
        out[at + 4] = 0;
    }
    at = at + 5;
    if at + 4 < out.len {
        out[at] = MW_TOTAL_EXPERTS;
        out[at + 1] = n_expert;
        out[at + 2] = 0;
        out[at + 3] = 0;
        out[at + 4] = 0;
    }
    at = at + 5;
    return at / 5;
}

export fn mw_walk(nl: i64, nslot: i64, bound: const i64[:], used: const i64[:],
                  has_t: const i64[:], off: const i64[:], ne2: const i64[:], out: i64[:]) -> i64 {
    return mw_walk_impl(nl, nslot, bound, used, has_t, off, ne2, out);
}

// ── the dense list the harvest hands the streamer ──────────────────────────
const MW_DROP: i64 = -1;
const MW_DENSE: i64 = 0;
const MW_DENSE_ROW: i64 = 1;

// session.cpp:1058-1080 — which captured weight leaves go into the streamer's
// dense list. Four native `continue`s decide it, in this order: an expert tensor
// is streamed, not held; a leaf without a tensor or buffer is a graph input or
// KV; a device-resident weight is not host memory to rebind; a leaf without a
// gguf offset/size is not a file tensor. The survivors are dense, and the ones
// the hook row-gathered are also row-streamed.
export fn mw_dense_bucket(is_expert: i64, has_tensor: i64, has_buffer: i64, dev_non_cpu: i64,
                          has_off: i64, has_size: i64, is_row: i64) -> i64 {
    if is_expert != 0 {
        return MW_DROP;
    }
    if has_tensor == 0 || has_buffer == 0 {
        return MW_DROP;
    }
    if dev_non_cpu != 0 {
        return MW_DROP;
    }
    if has_off == 0 || has_size == 0 {
        return MW_DROP;
    }
    if is_row != 0 {
        return MW_DENSE_ROW;
    }
    return MW_DENSE;
}

// session.cpp:1082 — the row window, in bytes, from a MiB flag. A negative
// --row-stream-mb is clamped to zero rather than wrapping into a huge budget.
export fn mw_row_budget(mb: i64) -> i64 {
    var v: i64 = mb;
    if v < 0 {
        v = 0;
    }
    return v * 1024 * 1024;
}
