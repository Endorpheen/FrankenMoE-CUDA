// Exported door for the tokenizer decisions, so a host can call them by name.
// The door adds nothing to the decision: a stable name is the whole point.

export fn fg_tk_norm(input: u8[:], out: u8[:], escape_space: i64, prepend_space: i64,
                     merge_spaces: i64, suffix_space: i64) -> i64 {
    return tk_norm(input, out, escape_space, prepend_space, merge_spaces, suffix_space, 202);
}

// BPE merge order: lower rank first, ties on the smaller left index. The host compares the pair it
// just popped with the queue's new top, which must satisfy this relation for every pop.
export fn fg_tk_bpe_before(rank_a: i64, left_a: i64, rank_b: i64, left_b: i64) -> i64 {
    return tk_bpe_before(rank_a, left_a, rank_b, left_b);
}
