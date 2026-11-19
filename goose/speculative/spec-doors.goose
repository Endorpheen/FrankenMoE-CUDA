// Exported doors for the speculative decisions, so a host can call them by name.
// Each door forwards to the ported decision in spec-decisions.goose and adds nothing:
// the point of a door is a stable name, not a second implementation.

export fn fg_sp_compatible(vtype_t: i64, add_bos_t: i64, bos_t: i64, add_eos_t: i64, eos_t: i64,
                           nvocab_t: i64, vtype_d: i64, add_bos_d: i64, bos_d: i64,
                           add_eos_d: i64, eos_d: i64, nvocab_d: i64, max_diff: i64) -> i64 {
    return sp_compatible(vtype_t, add_bos_t, bos_t, add_eos_t, eos_t, nvocab_t,
                         vtype_d, add_bos_d, bos_d, add_eos_d, eos_d, nvocab_d, max_diff, 190);
}

export fn fg_sp_prefix(draft: i64[:], target: i64[:], k: i64, out: i64[:]) -> i64 {
    return sp_prefix(draft, target, k, out, 191);
}

export fn fg_sp_ngram_accept(n_accepted: i64, n_draft_last: i64, n_low: i64, out: i64[:]) -> i64 {
    return sp_ngram_accept(n_accepted, n_draft_last, n_low, out, 192);
}


// n_max for the host check: types, sizes and widths are separate slices, and the widths are read
// only for the types that use them.
export fn fg_sp_n_max_call(types: i64[:], size_m: i64[:], draft_nmax: i64[:], ntypes: i64) -> i64 {
    return sp_n_max(types, size_m, draft_nmax, ntypes, 195);
}

export fn fg_sp_synth_accept(prob: f64, u: f64, is_replay: i64, is_eog: i64) -> i64 {
    return sp_synth_accept(prob, u, is_replay, is_eog, 208);
}

export fn fg_sp_reject_accept(p_x: f64, q_x: f64, u: f64) -> i64 {
    return sp_reject_accept(p_x, q_x, u, 209);
}
