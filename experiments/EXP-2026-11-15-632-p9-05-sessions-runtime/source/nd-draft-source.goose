// P9-05 S3: the draft source as the session admits and sizes it.
//
// Native source: runtime/core/src/engine/session.cpp
//   mparams.load_mtp        session.cpp:728   nextn tensors are skipped unless asked for
//   the MTP head admission  session.cpp:765-770
//   the streamed span       session.cpp:802   trunk + head when the source is MTP
//   n_rs_seq                session.cpp:843   rollback snapshots for any speculation
//   the draft ctx width     session.cpp:909-931
//   the verify batch        session.cpp:978-986
//   the RunInfo report      session.cpp:1169-1172
//
// Boundary against the two accepted neighbours, stated so nothing is ported twice:
//   - goose/misc/spec-decode-decisions.goose (EXP619, sdp_) owns the per-step
//     policy: ngram_draft itself, accept, verify width, rollback, MTP p_min width.
//     This module never looks at a generated token.
//   - goose/cli/config-validate.goose (EXP620, cfg_) owns flag admission: ranges,
//     source-specific knobs, workload/spec exclusion, n_ubatch vs draft_max+1.
//     This module assumes an already validated SpecConfig and decides what the
//     session builds with it.
//
// Source codes mirror SpecConfig::DraftSource: 0 none/off, 1 mtp, 2 ngram.

import std;

const ND_OFF: i64 = 0;
const ND_MTP: i64 = 1;
const ND_NGRAM: i64 = 2;

const ND_MTP_DRAFT_UBATCH: i64 = 32; // session.cpp:139 mtp_draft_ubatch

// session.cpp:1169 — the RunInfo label: "off", "mtp" or "ngram". The order of
// the native conditional matters: MTP first, then n-gram, else off.
fn nd_name_bytes(src: i64) -> const u8[:] {
    if src == ND_MTP {
        return "mtp";
    }
    if src == ND_NGRAM {
        return "ngram";
    }
    return "off";
}

export fn nd_source_name(src: i64, out: u8[:]) -> i64 {
    let s = nd_name_bytes(src);
    var i: i64 = 0;
    while i < s.len {
        if i < out.len {
            out[i] = s[i];
        }
        i = i + 1;
    }
    return s.len;
}

// SpecConfig::enabled() — a source is a source only when it is not none.
export fn nd_enabled(src: i64) -> i64 {
    if src == ND_OFF {
        return 0;
    }
    return 1;
}

// session.cpp:728 — mparams.load_mtp = cfg.spec.is_mtp(). The n-gram source
// reads text, not weights, so it never asks the loader for the nextn block.
export fn nd_load_mtp(src: i64) -> i64 {
    if src == ND_MTP {
        return 1;
    }
    return 0;
}

// session.cpp:765-770 — the head exists or the session refuses to open. Only
// MTP can fail here: n_layer_nextn is 0 on most ggufs and n-gram drafts anyway.
// 1 = admitted, 0 = refuse ("--mtp needs a model with a trained MTP head").
export fn nd_mtp_admit(src: i64, n_layer_nextn: i64) -> i64 {
    if src == ND_MTP && n_layer_nextn <= 0 {
        return 0;
    }
    return 1;
}

// session.cpp:802 — the streamer's layer bound. The MTP block sits at layer
// indices [n_layer, n_layer + n_layer_nextn) and routes experts of its own, so
// the streamed span must cover it; left at the trunk they stay mmap-resident.
export fn nd_stream_layers(n_layer: i64, src: i64, n_layer_nextn: i64) -> i64 {
    if src == ND_MTP {
        return n_layer + n_layer_nextn;
    }
    return n_layer;
}

// session.cpp:843 — cparams.n_rs_seq = draft_max when speculation is on, so a
// rejected draft rewinds by restoring state instead of replaying the sequence.
// When it is off the context keeps its own default, so the caller supplies it:
// this function does not invent a value the native code never writes.
export fn nd_rs_seq(src: i64, draft_max: i64, default_rs_seq: i64) -> i64 {
    if nd_enabled(src) != 0 {
        return draft_max;
    }
    return default_rs_seq;
}

// session.cpp:926-927 — the draft context's widest graph: room for the widest
// decode (1 + draft_max) but never less than mtp_draft_ubatch, and never wider
// than the target context it was copied from:
//   dparams.n_ubatch = max(draft_max + 1, 32); if > cparams.n_ubatch: clamp.
export fn nd_draft_ubatch(draft_max: i64, tgt_ubatch: i64) -> i64 {
    var w: i64 = draft_max + 1;
    if w < ND_MTP_DRAFT_UBATCH {
        w = ND_MTP_DRAFT_UBATCH;
    }
    if w > tgt_ubatch {
        return tgt_ubatch;
    }
    return w;
}

// session.cpp:983 — one batch, wide enough for the larger of its two roles:
// a whole prefill chunk, or 1 + draft_max positions of verify.
export fn nd_verify_batch(n_batch: i64, draft_max: i64) -> i64 {
    let verify = draft_max + 1;
    if n_batch > verify {
        return n_batch;
    }
    return verify;
}

// session.cpp:1170 — ri.spec_draft_max: reported only when a source is on.
export fn nd_ri_draft_max(src: i64, draft_max: i64) -> i64 {
    if nd_enabled(src) == 0 {
        return 0;
    }
    return draft_max;
}

// session.cpp:1171 — ri.mtp_p_min: an MTP knob, reported as 0 for the other
// sources rather than as the value the flag happened to carry.
export fn nd_ri_p_min(src: i64, p_min: f64) -> f64 {
    if src != ND_MTP {
        return 0.0;
    }
    return p_min;
}

// session.cpp:1172 — ri.ngram_min_match: the n-gram knob, 0 for the other
// sources. ngram_max_match is not reported at all, so it is not ported.
export fn nd_ri_ngram_min(src: i64, min_match: i64) -> i64 {
    if src != ND_NGRAM {
        return 0;
    }
    return min_match;
}
