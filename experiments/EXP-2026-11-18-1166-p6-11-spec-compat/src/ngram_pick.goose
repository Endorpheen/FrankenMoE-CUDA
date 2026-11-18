// Which ngram value becomes the draft (common_ngram_map_send, complex mode).
// The most frequent value is used, but only when it is at least twice as frequent as
// all the others together; ties keep the first slot because the scan uses a strict
// greater comparison. out[0] = chosen slot, out[1] = draft length in tokens.
// Returns 1 when a draft is produced, 0 when the native code returns early.
fn g_ngram_pick(vnum: i64[:], nacc: i64[:], m: i64, nslots: i64, out: i64[:], marker: i64) -> i64 {
    if marker != 194 {
        return -1;
    }
    if nslots <= 0 || vnum.len < nslots || nacc.len < nslots {
        return -1;
    }
    var max_occur: i64 = 0;
    var slot_max: i64 = 0;
    var v: i64 = 0;
    while v < nslots {
        if vnum[v] > max_occur {
            max_occur = vnum[v];
            slot_max = v;
        }
        v = v + 1;
    }
    var sum_occur: i64 = 0;
    v = 0;
    while v < nslots {
        if v != slot_max {
            sum_occur = sum_occur + vnum[v];
        }
        v = v + 1;
    }
    if sum_occur > 0 && max_occur < 2 * sum_occur {
        return 0;
    }
    var ntok = m;
    if nacc[slot_max] < ntok {
        ntok = nacc[slot_max];
    }
    out[0] = slot_max;
    out[1] = ntok;
    return 1;
}

fn main() {
    let a: i64[4] = [5, 1, 0, 0];
    let b: i64[4] = [8, 4, 0, 0];
    let o: i64[2] = [0, 0];
    let r = g_ngram_pick(a, b, 4, 4, o, 194);
    if r < 0 {
        return;
    }
}
