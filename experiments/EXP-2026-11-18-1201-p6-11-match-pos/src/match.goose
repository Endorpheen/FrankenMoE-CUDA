// Where the ngram-map key is looked up (common_ngram_map_send, the scan that runs when the
// hash shortcut found nothing).
// Precedence, exactly as the native code orders it:
//   1. the lower range  (size_last_begin - n - m - 1) down to key_map_last_idx + 1,
//   2. then the upper range (cur_len - n - m - 1) down to size_last_begin + 1.
// Both scan downward, so within a range the LARGEST matching index wins, but a match in the
// lower range beats any match in the upper one - which is why this is not "the last occurrence".
// Returns the position, or -1 when the key does not occur in either range.
fn g_ngram_match_pos(inp: i64[:], key: i64[:], n: i64, m: i64, cur_len: i64,
                     size_last_begin: i64, key_map_last_idx: i64, marker: i64) -> i64 {
    if marker != 206 {
        return -1;
    }
    if n <= 0 || m < 0 || key.len < n {
        return -1;
    }
    var lo: i64 = key_map_last_idx + 1;
    // lower range first
    if size_last_begin > n + m + 1 {
        var j: i64 = size_last_begin - n - m - 1;
        while j > lo - 1 {
            if j >= 0 && j + n <= inp.len {
                var k: i64 = 0;
                var ok: i64 = 1;
                while k < n {
                    if inp[j + k] != key[k] {
                        ok = 0;
                    }
                    k = k + 1;
                }
                if ok == 1 {
                    return j;
                }
            }
            j = j - 1;
        }
    }
    // then the upper range
    var u: i64 = cur_len - n - m - 1;
    while u > size_last_begin && u > key_map_last_idx {
        if u >= 0 && u + n <= inp.len {
            var k: i64 = 0;
            var ok: i64 = 1;
            while k < n {
                if inp[u + k] != key[k] {
                    ok = 0;
                }
                k = k + 1;
            }
            if ok == 1 {
                return u;
            }
        }
        u = u - 1;
    }
    return -1;
}

fn main() {
    let inp: i64[8] = [1, 2, 3, 1, 2, 3, 4, 5];
    let key: i64[2] = [1, 2];
    let r = g_ngram_match_pos(inp, key, 2, 2, 8, 8, 0, 206);
    if r < -2 {
        return;
    }
}
