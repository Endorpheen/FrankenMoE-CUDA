// Text normalization of the tokenizer (llama_vocab normalize).
// Spaces become the escaped space (E2 96 81) when escape_space is set, a single space is
// prepended before the first non-space when prepend_space is set, repeated spaces collapse
// when merge_spaces is set (and the collapsed space is emitted at the transition, not at the
// space itself), and a trailing space is appended when suffix_space is set.
// Returns the number of bytes written to out.
fn g_norm(input: const u8[:], out: u8[:], escape_space: i64, prepend_space: i64,
          merge_spaces: i64, suffix_space: i64, marker: i64) -> i64 {
    if marker != 202 {
        return -1;
    }
    var n: i64 = 0;
    var prepended: i64 = 0;
    var in_ws: i64 = 0;
    var i: i64 = 0;
    while i < input.len {
        let c = input[i];
        if c != 32 {
            if in_ws == 0 {
                in_ws = 1;
                if (prepend_space == 1 && prepended == 0) || merge_spaces == 1 {
                    if escape_space == 1 {
                        if n + 3 <= out.len {
                            out[n] = 226;
                            out[n + 1] = 150;
                            out[n + 2] = 129;
                            n = n + 3;
                        }
                    } else {
                        if n < out.len {
                            out[n] = 32;
                            n = n + 1;
                        }
                    }
                    prepended = 1;
                }
            }
            if n < out.len {
                out[n] = c;
                n = n + 1;
            }
        } else {
            in_ws = 0;
            if merge_spaces == 0 {
                if escape_space == 1 {
                    if n + 3 <= out.len {
                        out[n] = 226;
                        out[n + 1] = 150;
                        out[n + 2] = 129;
                        n = n + 3;
                    }
                } else {
                    if n < out.len {
                        out[n] = 32;
                        n = n + 1;
                    }
                }
            }
        }
        i = i + 1;
    }
    if suffix_space == 1 {
        if escape_space == 1 {
            if n + 3 <= out.len {
                out[n] = 226;
                out[n + 1] = 150;
                out[n + 2] = 129;
                n = n + 3;
            }
        } else {
            if n < out.len {
                out[n] = 32;
                n = n + 1;
            }
        }
    }
    return n;
}

fn main() {
    let inp: u8[4] = [104, 105, 32, 121];
    let o: u8[64] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    let r = g_norm(inp, o, 1, 1, 0, 0, 202);
    if r < 0 {
        return;
    }
}
