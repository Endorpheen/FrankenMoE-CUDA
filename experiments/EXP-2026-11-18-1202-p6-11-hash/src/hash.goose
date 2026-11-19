// ngram-map hash and its slot (common_ngram_map_hash + the modulo that places it).
// hash = ((0 * LCG + t0) * LCG + t1) ... over 32-bit wraparound, then the slot is
// hash % key_map_size. Both parts matter: the wraparound is what makes the C uint32_t
// arithmetic reproducible, and the slot decides collisions.
fn g_ngram_hash(tokens: i64[:], start: i64, len: i64, lcg: i64, marker: i64) -> i64 {
    if marker != 207 {
        return -1;
    }
    if len < 0 || start < 0 || start + len > tokens.len || lcg <= 0 {
        return -1;
    }
    var h: i64 = 0;
    var i: i64 = 0;
    while i < len {
        // (h * lcg) mod 2^32 without overflowing: h can be up to 2^32-1 and lcg is
        // 2654435761, whose product does not fit in a signed 64-bit integer.
        let hi: i64 = h / 65536;
        let lo: i64 = h - 65536 * hi;
        var p: i64 = lo * lcg;
        p = p - 4294967296 * (p / 4294967296);
        var q: i64 = hi * lcg;
        q = q - 65536 * (q / 65536);       // only the low 16 bits of this term can reach bit 32
        q = q * 65536;
        q = q - 4294967296 * (q / 4294967296);
        h = p + q;
        h = h - 4294967296 * (h / 4294967296);
        h = h + tokens[start + i];
        h = h - 4294967296 * (h / 4294967296);
        i = i + 1;
    }
    return h;
}

fn g_ngram_slot(hash: i64, slots: i64) -> i64 {
    if slots <= 0 {
        return -1;
    }
    return hash - slots * (hash / slots);
}

fn main() {
    let t: i64[4] = [1, 2, 3, 4];
    let h = g_ngram_hash(t, 0, 4, 1000003, 207);
    let s = g_ngram_slot(h, 1021);
    if s < 0 {
        return;
    }
}
