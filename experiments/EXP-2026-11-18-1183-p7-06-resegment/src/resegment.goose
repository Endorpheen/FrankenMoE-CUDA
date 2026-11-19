// Emission of the merged symbols (llm_tokenizer_spm_session::resegment).
// A symbol that is itself a token is emitted as is. A symbol that is not is split back into
// the two halves it was merged from, recursively, and a leaf that matches nothing falls back
// to one token per byte of its text.
// Per symbol: tok >= 0 means "this text is a token", lc/rc are the merge children (-1 for a
// leaf), off/len index into the text bytes. The output order is the order of the chain.
// Returns the number of ids written to out.
fn g_resegment(sym: i64[:], nchain: i64, ntab: i64, tok: i64[:], lc: i64[:], rc: i64[:],
               off: i64[:], len: i64[:], text: const u8[:], byteid: i64[:],
               out: i64[:], marker: i64) -> i64 {
    if marker != 204 {
        return -1;
    }
    var n: i64 = 0;
    var stack: i64[64] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    var sp: i64 = 0;
    // push the chain in reverse so it pops in order
    var i: i64 = nchain - 1;
    while i >= 0 {
        if sp < 64 {
            stack[sp] = i;
            sp = sp + 1;
        }
        i = i - 1;
    }
    while sp > 0 {
        sp = sp - 1;
        let s = stack[sp];
        if s < 0 || s >= ntab {
            continue;
        }
        if tok[s] >= 0 {
            if n < out.len {
                out[n] = tok[s];
                n = n + 1;
            }
        } else {
            if lc[s] >= 0 && rc[s] >= 0 {
                // push right first so the left half is emitted first
                if sp + 1 < 64 {
                    stack[sp] = rc[s];
                    sp = sp + 1;
                }
                if sp + 1 < 64 {
                    stack[sp] = lc[s];
                    sp = sp + 1;
                }
            } else {
                var b: i64 = 0;
                while b < len[s] {
                    if n < out.len {
                        let at = off[s] + b;
                        if at < text.len {
                            out[n] = byteid[text[at]];
                            n = n + 1;
                        }
                    }
                    b = b + 1;
                }
            }
        }
    }
    return n;
}

fn main() {
    let sym: i64[4] = [0, 1, 2, 3];
    let tok: i64[4] = [7, -1, 9, 5];
    let lc: i64[4] = [-1, 2, -1, -1];
    let rc: i64[4] = [-1, 3, -1, -1];
    let off: i64[4] = [0, 0, 1, 2];
    let len: i64[4] = [1, 3, 1, 1];
    let text: u8[3] = [104, 105, 33];
    let bid: i64[256] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    let o: i64[8] = [0, 0, 0, 0, 0, 0, 0, 0];
    let r = g_resegment(sym, 4, 4, tok, lc, rc, off, len, text, bid, o, 204);
    if r < 0 {
        return;
    }
}
