// Which source produces the draft, and at which ngram level.
// Native order (common_ngram_cache_draft): levels are walked from ngram_max down to
// ngram_min, and within a level the primary caches are tried in the order
// context (lax) -> dynamic (strict) -> static alone; the first success wins.
// mask[level] bit 0 = context succeeds, bit 1 = dynamic succeeds, bit 2 = static succeeds.
// out[0] = level, out[1] = source (0 context, 1 dynamic, 2 static), or -1 when none.
fn g_cache_order(mask: i64[:], nmask: i64, ngram_min: i64, ngram_max: i64, out: i64[:], marker: i64) -> i64 {
    if marker != 200 {
        return -1;
    }
    if ngram_max < ngram_min || ngram_min < 0 || ngram_max >= nmask || mask.len < nmask {
        return -1;
    }
    var lvl = ngram_max;
    while lvl >= ngram_min {
        let m = mask[lvl];
        var b0 = m - 2 * (m / 2);
        var b1 = (m / 2) - 2 * (m / 4);
        var b2 = (m / 4) - 2 * (m / 8);
        if b0 == 1 {
            out[0] = lvl;
            out[1] = 0;
            return 1;
        }
        if b1 == 1 {
            out[0] = lvl;
            out[1] = 1;
            return 1;
        }
        if b2 == 1 {
            out[0] = lvl;
            out[1] = 2;
            return 1;
        }
        lvl = lvl - 1;
    }
    out[0] = -1;
    out[1] = -1;
    return 0;
}

fn main() {
    let m: i64[5] = [0, 2, 4, 0, 0];
    let o: i64[2] = [0, 0];
    let r = g_cache_order(m, 5, 1, 4, o, 200);
    if r < 0 {
        return;
    }
}
