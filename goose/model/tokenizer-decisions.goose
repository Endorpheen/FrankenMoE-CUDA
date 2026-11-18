// Tokenizer decisions ported from src/llama-vocab.cpp, composed from the experiments that gated them:
// text normalization, the two merge orders, UTF-8 accounting, the score-path step, the WPM
// match search, the SPM merge order and the final symbol emission.
// Each function keeps the name of its gate; the prefix tk_ marks the area.

fn tk_norm(input: const u8[:], out: u8[:], escape_space: i64, prepend_space: i64,
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

fn tk_spm_before(sa: f64, la: i64, sb: f64, lb: i64) -> i64 {
    if sa > sb {
        return 1;
    }
    if sa < sb {
        return 0;
    }
    if la < lb {
        return 1;
    }
    return 0;
}

fn tk_bpe_before(ra: i64, la: i64, rb: i64, lb: i64) -> i64 {
    if ra < rb {
        return 1;
    }
    if ra > rb {
        return 0;
    }
    if la < lb {
        return 1;
    }
    return 0;
}

fn tk_utf8_len(c: i64) -> i64 {
    var s: i64 = 1;
    if c >= 128 {
        s = s + 1;
    }
    if c >= 2048 {
        s = s + 1;
    }
    if c >= 65536 {
        s = s + 1;
    }
    return s;
}

// The byte at position i of the encoding of c, exactly as the native fallback builds it:
// a lead or continuation marker OR-ed with the payload bits taken at the right shift.
// Dropping the payload was a real defect in the first version of this port.
fn tk_utf8_byte(c: i64, i: i64) -> i64 {
    let s = tk_utf8_len(c);
    if s == 1 {
        return c;
    }
    var base: i64 = 128;
    if i == 0 {
        var v: i64 = 3840;
        var k: i64 = 0;
        while k < s {
            v = v / 2;
            k = k + 1;
        }
        base = v - 256 * (v / 256);
    }
    var shift: i64 = (s - i - 1) * 6;
    var payload: i64 = c;
    var d: i64 = 0;
    while d < shift {
        payload = payload / 2;
        d = d + 1;
    }
    payload = payload - 64 * (payload / 64);
    // base and payload occupy disjoint bits (base above 0x7F, payload below 0x40),
    // so the OR the native code performs is exactly this sum
    return base + payload;
}

fn tk_dp_step(scores: i64[:], at: i64, plen: i64[:], pscore: i64[:], pid: i64[:], npieces: i64,
             out_len: i64[:], out_tok: i64[:], out_num: i64[:], c: i64) -> i64 {
    var best: i64 = scores[at];
    var chosen: i64 = -1;
    var i: i64 = 0;
    while i < npieces {
        let score = pscore[i];
        let L = plen[i];
        if score > -2 && at + L <= scores.len {
            var s = scores[at + L] - score;
            if s < best {
                best = s;
                chosen = i;
                out_len[at] = L;
                out_tok[at] = pid[i];
                var num = out_num[at + L] + 1;
                if score == -1 {
                    var extra: i64 = 0;
                    if c >= 128 { extra = extra + 1; }
                    if c >= 2048 { extra = extra + 1; }
                    if c >= 65536 { extra = extra + 1; }
                    num = num + extra;
                }
                out_num[at] = num;
            }
        }
        if score == -1 {
            break;
        }
        i = i + 1;
    }
    scores[at] = best;
    return chosen;
}

fn tk_wpm_pick(n: i64, i: i64, maxlen: i64, okmask: i64, marker: i64) -> i64 {
    if marker != 201 {
        return -1;
    }
    if n <= 0 || i < 0 || i >= n || maxlen < 0 {
        return -1;
    }
    var j: i64 = i + maxlen + 1;
    if j > n {
        j = n;
    }
    while j > i {
        var val: i64 = okmask;
        var idx: i64 = 0;
        while idx < j {
            val = val / 2;
            idx = idx + 1;
        }
        // val - 2*(val/2) is bit j of the mask
        if val - 2 * (val / 2) == 1 {
            return j - i;
        }
        j = j - 1;
    }
    return -1;
}

fn tk_spm_merge(n: i64, pl: i64[:], pr: i64[:], pscore: f64[:], psize: i64[:], np: i64,
               next: i64[:], prev: i64[:], symn: i64[:], out: i64[:], marker: i64) -> i64 {
    if marker != 203 {
        return -1;
    }
    if n <= 0 || n > next.len || n > prev.len || n > symn.len {
        return -1;
    }
    var i: i64 = 0;
    while i < n {
        next[i] = i + 1;
        prev[i] = i - 1;
        symn[i] = 1;
        i = i + 1;
    }
    var nmerged: i64 = 0;
    var done: i64 = 0;
    while done == 0 && nmerged < out.len {
        var best: i64 = -1;
        var bleft: i64 = 0;
        var k: i64 = 0;
        while k < np {
            let l = pl[k];
            let r = pr[k];
            // the size check is not redundant with adjacency: a left symbol that has
            // already absorbed its neighbour is still adjacent, but its text has grown,
            // so the pair was queued for a different text and the native code skips it
            if l >= 0 && l < n && r >= 0 && r < n && symn[l] > 0 && symn[r] > 0 &&
                    next[l] == r && symn[l] + symn[r] == psize[k] {
                if best < 0 || pscore[k] > pscore[best] || (pscore[k] == pscore[best] && l < bleft) {
                    best = k;
                    bleft = l;
                }
            }
            k = k + 1;
        }
        if best < 0 {
            done = 1;
        } else {
            let l = pl[best];
            let r = pr[best];
            symn[l] = symn[l] + symn[r];
            symn[r] = 0;
            next[l] = next[r];
            var nx = next[r];
            if nx < n {
                prev[nx] = l;
            }
            out[nmerged] = best;
            nmerged = nmerged + 1;
        }
    }
    return nmerged;
}

fn tk_resegment(sym: i64[:], nchain: i64, ntab: i64, tok: i64[:], lc: i64[:], rc: i64[:],
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

