// Tokenizer decisions in Goose (APX: tokenizer).
//
// From unicode.cpp:16 and llama-vocab.cpp:118-230 (the BPE tokenizer). Tokenization runs on the
// Apex path twice per request -- once for the prompt, once per generated token when detokenizing.
extern "fg202t_fail" fn fail(code: i64);
extern "fg202t_st_i64" fn st_i64(base: u64, byte: i64, v: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}
// utf8_len: unicode.cpp:16. The lead byte's top four bits index a table of 1,1,1,1,1,1,1,1,
// 1,1,1,1,2,2,3,4. Written as compares rather than a table so it stays arithmetic.
//   src is the unsigned byte value.

fn utf8_len(src: i64) -> i64 {
    ensure(src >= 0 && src <= 255 as i64, 10);
    let hi: i64 = src / 16 as i64;

    if hi <= 11 as i64 {
        return 1 as i64;
    }

    if hi <= 13 as i64 {
        return 2 as i64;
    }

    if hi == 14 as i64 {
        return 3 as i64;
    }
    return 4 as i64;
}
// bigram_valid: llama-vocab.cpp:147-151. A queued merge is skipped when either side was already
// merged away, or when the recorded size no longer matches.

fn bigram_valid(left_n: i64, right_n: i64, bigram_size: i64) -> i64 {
    ensure(left_n >= 0 && right_n >= 0 && bigram_size >= 0, 20);

    if left_n == 0 as i64 {
        return 0 as i64;
    }

    if right_n == 0 as i64 {
        return 0 as i64;
    }

    if left_n + right_n != bigram_size {
        return 0 as i64;
    }
    return 1 as i64;
}
// try_add_allowed: llama-vocab.cpp:203-216. A pair is queued only when both indices exist, the
// concatenation is a real token, and the id is inside the vocabulary.

fn try_add_allowed(left: i64, right: i64, token: i64, null_token: i64, n_tokens: i64) -> i64 {
    ensure(n_tokens >= 0, 30);

    if left == (0 as i64) - 1 as i64 {
        return 0 as i64;
    }

    if right == (0 as i64) - 1 as i64 {
        return 0 as i64;
    }

    if token == null_token {
        return 0 as i64;
    }

    if token >= n_tokens {
        return 0 as i64;
    }
    return 1 as i64;
}
// resegment_kind: llama-vocab.cpp:177-199.
//   0 = the text is a token, emit it
//   1 = it was built from a merge, recurse into the recorded pair
//   2 = neither, emit the bytes

fn resegment_kind(is_token: i64, has_merge: i64) -> i64 {
    if is_token != 0 {
        return 0 as i64;
    }

    if has_merge != 0 {
        return 1 as i64;
    }
    return 2 as i64;
}
// clamp_symbol_len: line 126, `min(utf8_len, text.size() - offs)` -- the last symbol may be a
// truncated sequence.

fn clamp_symbol_len(len: i64, remaining: i64) -> i64 {
    ensure(len > 0 && remaining > 0, 40);

    if len < remaining {
        return len;
    }
    return remaining;
}
// GPT-2 BPE (`llm_tokenizer_bpe_session`), the tokenizer Qwen actually uses.
//
// bpe_add_allowed: llama-vocab.cpp:784-797. A pair enters the work queue only when both
// neighbours exist and a rank exists for the concatenation.

fn bpe_add_allowed(left: i64, right: i64, rank: i64) -> i64 {
    if left == (0 as i64) - 1 as i64 {
        return 0 as i64;
    }

    if right == (0 as i64) - 1 as i64 {
        return 0 as i64;
    }

    if rank < (0 as i64) {
        return 0 as i64;
    }
    return 1 as i64;
}
// bpe_merge_valid: lines 715-724. A queued merge is dropped when a side was already merged away,
// or when the recorded text no longer equals the two current sides concatenated.
//   concat_matches is the result of `left_token + right_token == bigram.text`, computed by the
//   caller because Goose has no string type yet.

fn bpe_merge_valid(left_n: i64, right_n: i64, concat_matches: i64) -> i64 {
    if left_n == 0 as i64 {
        return 0 as i64;
    }

    if right_n == 0 as i64 {
        return 0 as i64;
    }

    if concat_matches == 0 as i64 {
        return 0 as i64;
    }
    return 1 as i64;
}
// bpe_ignore_merges: line 686, `ignore_merges && text_to_token(word) != NULL`.

fn bpe_ignore_merges(ignore: i64, has_token: i64) -> i64 {
    if ignore != 0 as i64 && has_token != 0 as i64 {
        return 1 as i64;
    }
    return 0 as i64;
}

// Export wrappers (same rule as elsewhere in the tree: a body that the unit also
// calls internally cannot carry the export itself, so the export is a forward).
// The standalone gate calls all eight decisions by name.
export fn tok_utf8_len(src: i64) -> i64 { return utf8_len(src); }
export fn tok_bigram_valid(left_n: i64, right_n: i64, bigram_size: i64) -> i64 { return bigram_valid(left_n, right_n, bigram_size); }
export fn tok_try_add_allowed(left: i64, right: i64, token: i64, null_token: i64, n_tokens: i64) -> i64 { return try_add_allowed(left, right, token, null_token, n_tokens); }
export fn tok_resegment_kind(is_token: i64, has_merge: i64) -> i64 { return resegment_kind(is_token, has_merge); }
export fn tok_clamp_symbol_len(len: i64, remaining: i64) -> i64 { return clamp_symbol_len(len, remaining); }
export fn tok_bpe_add_allowed(left: i64, right: i64, rank: i64) -> i64 { return bpe_add_allowed(left, right, rank); }
export fn tok_bpe_merge_valid(left_n: i64, right_n: i64, concat_matches: i64) -> i64 { return bpe_merge_valid(left_n, right_n, concat_matches); }
export fn tok_bpe_ignore_merges(ignore: i64, has_token: i64) -> i64 { return bpe_ignore_merges(ignore, has_token); }

// Renamed so a composed unit keeps exactly one global `main`.
fn tokenizer_demo_main() {
    utf8_len(0 as i64);
    bigram_valid(0 as i64, 0 as i64, 0 as i64);
    try_add_allowed(0 as i64, 0 as i64, 0 as i64, 0 as i64, 0 as i64);
    resegment_kind(0 as i64, 0 as i64);
    clamp_symbol_len(1 as i64, 1 as i64);
    bpe_add_allowed(0 as i64, 0 as i64, 0 as i64);
    bpe_merge_valid(1 as i64, 1 as i64, 1 as i64);
    bpe_ignore_merges(0 as i64, 0 as i64);
}

