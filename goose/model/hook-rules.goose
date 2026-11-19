// P9-04 S1: the router hook's ACTIVATION and CALL ORDER, from
// runtime/core/src/moe/router_hook.cpp.
//
// The hook is the only place where the runtime sees a llama.cpp graph. What it
// is asked to do changes per node and per run; all of that is decided by small
// integer flags and by the node's NAME. Those decisions are pure, so they are
// ported here and the native file keeps the tensor reads, the barriers and the
// source/load calls:
//
//   name matching       48  is_moe_node: the 8-byte "ffn_moe_" prefix is the
//                            gate in front of every other matcher
//                       51  parse_layer_tail: digits only to the end of the
//                            string, > 2^20 rejected (stricter than %d)
//                       66  match_layer_node: prefix + tail, for the two
//                            singular routing nodes (topk, gate matmul)
//                       83  kWeightNodes + match_weights: the weight chain is
//                            four mutually exclusive names, and the '-'
//                            after the name is what rejects weights_sum
//   arming             232  substitute_armed: a source, a lambda, decode only
//                       239  drop_armed: a source, a fraction, decode unless
//                            armed for prefill
//   isolation          1469-1528  which node is worth a barrier: the topk, the
//                            weight chain while a layer is still learning its
//                            terminal, the gate matmul only for the probe,
//                            and every node under the compute trace
//                       1489  weights_iso: stop isolating the chain once its
//                            terminal is known
//   learning/deferral   321  close_drop_layer: learn the terminal, and FORGET
//                            it when the deferral was not honoured
//                       1547  the weight-node observation that learns it and
//                            then decides
//                       1668  the topk's defer decision
//   capture            1410  which expert tensor a captured node admits
//   order              1553-1699  the topk callback's sequence: substitution,
//                            route-ahead, gather, workload, trace, drop,
//                            prediction, load-or-defer, prefetch, record
//   refusal            1588  a workload capture/replay that does not match the
//                            graph shape fails closed (abort callback)
//
// Not here: tensor reads and writes, the barrier's cost, the prediction GEMV,
// route-ahead's own decisions, load_layer, and the row-source preparation
// (which is S3). The fixture supplies node names and run flags.

const RH_KWEIGHTS: i64 = 4;
const RH_TAIL_MAX: i64 = 1048576; // 1 << 20, the native parse ceiling

// The topk callback's operations, in the native order of the file.
const RH_OP_CLOSE: i64 = 1;         // 1559 close_drop_layer()
const RH_OP_SUB: i64 = 2;           // 1563 apply_substitute
const RH_OP_RA_COLLECT: i64 = 3;    // 1572 route_ahead_collect
const RH_OP_RA_SUBMIT: i64 = 4;     // 1573 route_ahead_submit
const RH_OP_RA_APPLY: i64 = 5;      // 1574 apply_route_ahead
const RH_OP_GATHER: i64 = 6;        // 1580 col 1 the ids
const RH_OP_WL_CAPTURE: i64 = 7;    // 1590 capture_topk
const RH_OP_WL_REPLAY: i64 = 8;     // 1598 replay_topk + write back + re-gather
const RH_OP_TRACE: i64 = 9;         // 1602 flush_pending + open the record
const RH_OP_DROP_COUNT: i64 = 10;   // 1619 experts_routed_ += slots
const RH_OP_PREDICT: i64 = 11;      // 1625 predict_at_topk
const RH_OP_DEFER: i64 = 12;        // 1671 drop_.deferred = true
const RH_OP_LOAD: i64 = 13;         // 1687 load_layer at the topk
const RH_OP_PRED_AFTER: i64 = 14;   // 1692 predict_after_load
const RH_OP_SCORE: i64 = 15;        // 1699 score_layer
const RH_OP_PREFETCH: i64 = 16;     // 1705 temporal prefetch hints
const RH_OP_RECORD: i64 = 17;       // 1713 remember this routing
const RH_OP_WL_FAIL: i64 = 18;      // 1588/1595 shape mismatch: fail closed

// Flag indices of the topk callback's input vector.
const RH_F_SUB: i64 = 0;      // substitute_armed()
const RH_F_RA: i64 = 1;       // route_ahead_ > 0
const RH_F_PHASE: i64 = 2;    // batch_phase_ (0 prefill, 1 decode)
const RH_F_WL: i64 = 3;       // workload mode: 0 off, 1 capture, 2 replay
const RH_F_TRACE: i64 = 4;    // trace_on_
const RH_F_DROP: i64 = 5;     // drop_frac_ > 0
const RH_F_PREDICT: i64 = 6;  // predict_on() (probe or prefetch)
const RH_F_DEFER: i64 = 7;    // the topk's defer decision
const RH_F_IDS: i64 = 8;      // the gathered routing is non-empty
const RH_F_PREDLOG: i64 = 9;  // predict_log_
const RH_F_PREFETCH: i64 = 10; // prefetch_layers_ > 0
const RH_F_RECORD: i64 = 11;  // prefetch or probe keeps prev_ids_
const RH_F_WL_OK: i64 = 12;   // the workload's shape check passed
const RH_F_WL_MATCH: i64 = 13; // the replay found the captured routing

// The ask pass's operations, in the native order of the file.
const RH_A_CAPTURE: i64 = 1;   // 1405 the capture pass, which never isolates
const RH_A_ROW: i64 = 2;       // 1446 row-source preparation (S3 decides)
const RH_A_STASH: i64 = 3;     // 1500 the gate matmul's source pointers
const RH_A_CTRACE: i64 = 4;    // 1511 the compute trace's isolation
const RH_A_TOPK: i64 = 5;      // 1528 the topk barrier
const RH_A_WEIGHTS: i64 = 6;   // 1528 the weight chain's barrier
const RH_A_LOGITS: i64 = 7;    // 1528 the probe's gate-matmul barrier

fn rh_at(s: const u8[:], at: i64) -> i64 {
    if at < 0 || at >= s.len {
        return 0; // past the end: the native C string's NUL
    }
    return s[at];
}

// memcmp(name, lit, lit.len) == 0 over a NUL-padded name buffer.
fn rh_prefix(name: const u8[:], lit: const u8[:]) -> i64 {
    var k: i64 = 0;
    while k < lit.len {
        if rh_at(name, k) != lit[k] {
            return 0;
        }
        k += 1;
    }
    return 1;
}

// router_hook.cpp:48-50 — is_moe_node. Every routing node the hook looks for is
// named "ffn_moe_<something>"; one comparison keeps the rest of the matching
// off the hot path, because a graph has thousands of nodes and the ask pass
// sees all of them.
export fn rh_is_moe_node(name: const u8[:]) -> i64 {
    return rh_prefix(name, "ffn_moe_");
}

// router_hook.cpp:51-63 — parse_layer_tail. Digits only, to the end of the
// string: "12abc" is not layer 12, and a runaway digit run is not a layer
// index. The ceiling is the native one, checked after every digit.
fn rh_tail(name: const u8[:], pos_from: i64) -> i64 {
    if pos_from < 0 || pos_from >= name.len {
        return -1; // an empty tail is not a layer
    }
    var c: i64 = name[pos_from];
    if c < '0' || c > '9' {
        return -1;
    }
    var il: i64 = 0;
    var k: i64 = pos_from;
    while k < name.len {
        c = name[k];
        if c < '0' || c > '9' {
            return -1;
        }
        il = il * 10 + (c - '0');
        if il > RH_TAIL_MAX {
            return -1;
        }
        k += 1;
    }
    return il;
}

export fn rh_parse_tail(name: const u8[:], pos_from: i64) -> i64 {
    return rh_tail(name, pos_from);
}

// router_hook.cpp:66-70 — match_layer_node. kind 1 is the topk ("ffn_moe_topk-"),
// kind 2 the gate matmul ("ffn_moe_logits-"). The trailing '-' is what keeps
// "ffn_moe_logits_biased-<il>" — a different node — from matching.
export fn rh_match_layer_node(name: const u8[:], kind: i64) -> i64 {
    var lit: const u8[:] = "";
    if kind == 1 {
        lit = "ffn_moe_topk-";
    } else if kind == 2 {
        lit = "ffn_moe_logits-";
    } else if kind == 3 {
        lit = "ffn_moe_topk_softmax-";
    } else {
        return -1;
    }
    if rh_prefix(name, lit) == 0 {
        return -1;
    }
    return rh_tail(name, lit.len);
}

// router_hook.cpp:83-88 — kWeightNodes, in the native table order.
fn rh_kweight(v: i64) -> const u8[:] {
    if v == 0 {
        return "ffn_moe_weights";
    }
    if v == 1 {
        return "ffn_moe_weights_softmax";
    }
    if v == 2 {
        return "ffn_moe_weights_norm";
    }
    if v == 3 {
        return "ffn_moe_weights_scaled";
    }
    return "";
}

// router_hook.cpp:90-100 — match_weights. Last-wins is the caller's business;
// what is decided here is which VARIANT a name is, and the four names are
// mutually exclusive because each one must be followed by '-'. The first
// matching variant wins, in table order.
//
// `out[0]` receives the layer and is written ONLY on a match, exactly like the
// native `int &il_out`, which the caller initialises to -1.
export fn rh_match_weights(name: const u8[:], out: i64[:]) -> i64 {
    if rh_prefix(name, "ffn_moe_") == 0 {
        return -1; // the hook only asks after the prefix test
    }
    var v: i64 = 0;
    while v < RH_KWEIGHTS {
        let lit: const u8[:] = rh_kweight(v);
        if rh_prefix(name, lit) != 0 && rh_at(name, lit.len) == '-' {
            let il: i64 = rh_tail(name, lit.len + 1);
            if il >= 0 {
                if out.len > 0 {
                    out[0] = il;
                }
                return v;
            }
        }
        v += 1;
    }
    return -1;
}

// router_hook.cpp:239-241 — drop_armed. Needs a source to ask about residency,
// a non-zero threshold, and a decode batch unless the run armed prefill too.
export fn rh_drop_armed(frac: f64, has_source: i64, phase: i64, prefill: i64) -> i64 {
    if frac > 0.0 && has_source != 0 && (phase == 1 || prefill != 0) {
        return 1;
    }
    return 0;
}

// router_hook.cpp:232-234 — substitute_armed. Decode only, unconditionally: a
// prefill runs against a cold cache and is compute-bound anyway.
export fn rh_sub_armed(lambda: f64, has_source: i64, phase: i64) -> i64 {
    if lambda > 0.0 && has_source != 0 && phase == 1 {
        return 1;
    }
    return 0;
}

// router_hook.cpp:1476 — the weight chain is asked for by a traced run, and by
// the drop policy, which decides on the weights the matmul will actually apply.
// Neither is on by default: every extra ask is another barrier.
export fn rh_want_weights(trace_on: i64, drop_armed: i64) -> i64 {
    if trace_on != 0 || drop_armed != 0 {
        return 1;
    }
    return 0;
}

// router_hook.cpp:1489-1490 — weights_iso. Once a layer's terminal weight node
// is known, both consumers read that node and no other, so isolating the rest
// of the chain would buy nothing and cost a graph split plus a compute-thread
// synchronization per node. The chain is asked for again only while the layer
// is still learning it, which is the first graph of a run and any graph after
// close_drop_layer forgot a terminal that stopped appearing.
export fn rh_weights_iso(variant: i64, wl: i64, term: const i64[:]) -> i64 {
    if variant < 0 {
        return 0; // not a weight node at all
    }
    if wl < 0 || wl >= term.len {
        return 1;
    }
    if term[wl] < 0 {
        return 1; // still learning this layer's chain
    }
    if term[wl] == variant {
        return 1; // the known terminal: the one worth a barrier
    }
    return 0;
}

// router_hook.cpp:1496-1498 — is the gate matmul looked for at all? It carries
// the router's own two inputs, which is why the probe attaches there; it is
// decode-only and needs a source.
export fn rh_logits_scope(moe_node: i64, predict_or_ra: i64, has_source: i64, phase: i64) -> i64 {
    if moe_node != 0 && predict_or_ra != 0 && has_source != 0 && phase == 1 {
        return 1;
    }
    return 0;
}

// router_hook.cpp:1510-1522 — the compute trace's share of the ask decision.
// Node granularity isolates everything. Layer granularity isolates one node per
// layer transition; the cursor advances on the ASK stream, which every node
// passes through, and a layerless name (embeddings, the head, a reshape) never
// moves it. `st[0]` carries ctrace_ask_layer_ in and out.
export fn rh_ctrace_iso(ctrace_on: i64, layers: i64, nl: i64, st: i64[:]) -> i64 {
    if ctrace_on == 0 {
        return 0;
    }
    if layers == 0 {
        return 1;
    }
    if st.len < 1 {
        return 0;
    }
    if nl >= 0 && nl != st[0] {
        st[0] = nl;
        return 1;
    }
    return 0;
}

// router_hook.cpp:1528 — the ask pass's answer, a disjunction of the three
// consumers plus the trace. Route-ahead deliberately does NOT isolate the gate
// matmul: a committed prediction is made safe by the watchdog and the
// passthrough default, not by a barrier.
export fn rh_ask_iso(ctrace_iso: i64, is_topk: i64, weights_iso: i64, is_logits: i64,
                     predict_log: i64) -> i64 {
    if ctrace_iso != 0 || is_topk != 0 || weights_iso != 0 || (is_logits != 0 && predict_log != 0) {
        return 1;
    }
    return 0;
}

// router_hook.cpp:1410-1420 — capture admission for an expert-named tensor.
// The expert dim is dim-2; a tensor whose expert axis is empty is not one the
// streamer can bind, and its slot stays unbound rather than half-bound.
export fn rh_capture_take(p: i64, in_range: i64, ne2: i64) -> i64 {
    if p < 0 {
        return 0; // not an expert-named tensor
    }
    if in_range != 0 && ne2 > 0 {
        return 1;
    }
    return 0;
}

// router_hook.cpp:1668-1671 — defer the layer's load from the topk node to its
// terminal weight node. Only safe once that terminal is known: otherwise there
// is no callback left to decide in and the expert matmul would run against
// slots nothing loaded.
export fn rh_defer(drop_armed: i64, il: i64, term: const i64[:]) -> i64 {
    if drop_armed != 0 && il >= 0 && il < term.len && term[il] >= 0 {
        return 1;
    }
    return 0;
}

// router_hook.cpp:321-341 — close_drop_layer, run when the next layer's topk
// arrives. 0 = nothing pending, 1 = the terminal was learned, 2 = the deferral
// was never honoured, so the terminal is FORGOTTEN and the routing is loaded
// now. Forgetting is what keeps this self-correcting: deferring again on the
// same stale guess would repeat the fault every token.
export fn rh_close_layer(term: i64[:], lay: i64, chain_last: i64, deferred: i64) -> i64 {
    if lay < 0 {
        return 0;
    }
    var r: i64 = 0;
    if lay < term.len && term[lay] < 0 && chain_last >= 0 {
        term[lay] = chain_last;
        r = 1;
    }
    if deferred != 0 {
        if lay < term.len {
            term[lay] = -1;
        }
        r = 2;
    }
    return r;
}

// router_hook.cpp:1547-1551 — a weight node of the pending layer, offered after
// that layer's topk. The chain's last node seen is remembered as the layer's
// terminal candidate; a deferred layer decides here, and only here, because
// everything the policy needs is final at this node and nothing has consumed it
// yet. `st[0]` carries chain_last_ in and out.
export fn rh_observe_weights(ready: i64, wl: i64, variant: i64, drop_layer: i64, deferred: i64,
                             term: const i64[:], st: i64[:]) -> i64 {
    if ready == 0 || wl < 0 || drop_layer != wl || st.len < 1 {
        return 0;
    }
    st[0] = variant;
    if deferred != 0 && wl < term.len && term[wl] == variant {
        return 2; // apply_drop here
    }
    return 1;
}

// router_hook.cpp:1588-1600 — the workload capture/replay rides the topk
// barrier and is deliberately LAST among the route modifiers. Codes:
// 0 nothing, 1 captured, 2 replayed (write back and re-gather), 3 fail closed.
// A replay that does not match the capture is not a mismatch to be tolerated:
// the two arms must execute the identical storage and expert-compute workload,
// so the shape check aborts the graph through the abort callback.
export fn rh_workload(mode: i64, shape_ok: i64, matches: i64) -> i64 {
    if mode == 0 {
        return 0;
    }
    if shape_ok == 0 {
        return 3;
    }
    if mode == 1 {
        return 1;
    }
    if matches != 0 {
        return 2;
    }
    return 3;
}

fn rh_put(out: i64[:], k: i64, v: i64) -> i64 {
    if k < out.len {
        out[k] = v;
    }
    return k + 1;
}

// router_hook.cpp:1553-1699 — the topk callback's sequence, as a plan. Every
// consumer below runs on the routing that will actually run, which is why the
// order is load-bearing: substitution first (it is the last point where the ids
// are still ours to change), then route-ahead (which must collect before it
// submits, or nearly every legitimate result stale-fails), then the gather,
// then the workload, the trace, the drop counters and the prediction, and only
// then the load — either here or deferred to the weight chain.
//
// Returns the number of steps written to `out`. `f` is the flag vector above.
export fn rh_topk_plan(f: const i64[:], out: i64[:]) -> i64 {
    var k: i64 = 0;
    k = rh_put(out, k, RH_OP_CLOSE);
    if f[RH_F_SUB] != 0 {
        k = rh_put(out, k, RH_OP_SUB);
    }
    if f[RH_F_RA] != 0 && f[RH_F_PHASE] == 1 {
        k = rh_put(out, k, RH_OP_RA_COLLECT);
        k = rh_put(out, k, RH_OP_RA_SUBMIT);
        k = rh_put(out, k, RH_OP_RA_APPLY);
    }
    k = rh_put(out, k, RH_OP_GATHER);
    if f[RH_F_WL] == 1 {
        if f[RH_F_WL_OK] != 0 {
            k = rh_put(out, k, RH_OP_WL_CAPTURE);
        } else {
            k = rh_put(out, k, RH_OP_WL_FAIL);
        }
    } else if f[RH_F_WL] == 2 {
        if f[RH_F_WL_OK] != 0 && f[RH_F_WL_MATCH] != 0 {
            k = rh_put(out, k, RH_OP_WL_REPLAY);
        } else {
            k = rh_put(out, k, RH_OP_WL_FAIL);
        }
    }
    if f[RH_F_TRACE] != 0 {
        k = rh_put(out, k, RH_OP_TRACE);
    }
    if f[RH_F_DROP] != 0 {
        k = rh_put(out, k, RH_OP_DROP_COUNT);
    }
    if f[RH_F_PREDICT] != 0 {
        k = rh_put(out, k, RH_OP_PREDICT);
    }
    if f[RH_F_DEFER] != 0 {
        k = rh_put(out, k, RH_OP_DEFER);
    } else {
        if f[RH_F_IDS] != 0 {
            k = rh_put(out, k, RH_OP_LOAD);
        }
        if f[RH_F_PREDICT] != 0 {
            k = rh_put(out, k, RH_OP_PRED_AFTER);
        }
        if f[RH_F_RA] != 0 && f[RH_F_PHASE] == 1 {
            k = rh_put(out, k, RH_OP_RA_COLLECT);
        }
    }
    if f[RH_F_PREDLOG] != 0 && f[RH_F_PHASE] == 1 {
        k = rh_put(out, k, RH_OP_SCORE);
    }
    if f[RH_F_PREFETCH] != 0 {
        k = rh_put(out, k, RH_OP_PREFETCH);
    }
    if f[RH_F_RECORD] != 0 {
        k = rh_put(out, k, RH_OP_RECORD);
    }
    return k;
}

// router_hook.cpp:1405-1528 — the ask pass's sequence. The capture pass comes
// first and OWNS the pass: it returns before anything else can act, because its
// job is to survey the graph, not to isolate a node. Then the row-source
// preparation (which must run before the node does, and needs no barrier), then
// the gate matmul's pointers — free, from this pass — and finally the isolation
// answer, whose contributing arms are reported in the native order.
//
// `f`: 0 capturing, 1 row_source, 2 is_logits_in_range, 3 gl < n_layer,
//      4 ctrace_iso, 5 is_topk, 6 weights_iso, 7 predict_log.
export fn rh_ask_plan(f: const i64[:], out: i64[:]) -> i64 {
    var k: i64 = 0;
    if f[0] != 0 {
        return rh_put(out, k, RH_A_CAPTURE); // and the answer is "do not isolate"
    }
    if f[1] != 0 {
        k = rh_put(out, k, RH_A_ROW);
    }
    if f[2] != 0 && f[3] != 0 {
        k = rh_put(out, k, RH_A_STASH);
    }
    if f[4] != 0 {
        k = rh_put(out, k, RH_A_CTRACE);
    }
    if f[5] != 0 {
        k = rh_put(out, k, RH_A_TOPK);
    }
    if f[6] != 0 {
        k = rh_put(out, k, RH_A_WEIGHTS);
    }
    if f[7] != 0 {
        k = rh_put(out, k, RH_A_LOGITS);
    }
    return k;
}
