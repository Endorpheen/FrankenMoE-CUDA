// Score-path dynamic programming of the tokenizer (encode_cpts).
// Positions are filled from the end; a candidate piece at i costs scores[i + len] - score,
// and a piece replaces the current choice only when it is STRICTLY better. An unknown piece
// (score == -1) additionally bills the UTF-8 byte count of the codepoint.
// plen/pscore/pid are the candidate pieces in the table order the native loop walks;
// they all start at the position being computed.
// out_len/out_id are written at the position index; scores is the running table.
fn g_dp_step(scores: i64[:], at: i64, plen: i64[:], pscore: i64[:], pid: i64[:], npieces: i64,
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

fn main() {
    let sc: i64[4] = [1000, 1000, 0, 0];
    let pl: i64[2] = [1, 2];
    let ps: i64[2] = [5, 1];
    let pi: i64[2] = [10, 11];
    let ol: i64[4] = [0, 0, 0, 0];
    let ot: i64[4] = [0, 0, 0, 0];
    let on: i64[4] = [0, 0, 0, 0];
    let r = g_dp_step(sc, 0, pl, ps, pi, 2, ol, ot, on, 97);
    if r < -1 {
        return;
    }
}
