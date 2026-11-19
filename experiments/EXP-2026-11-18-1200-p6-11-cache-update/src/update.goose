// Insertion policy of the ngram cache (common_ngram_cache_update).
// For each ngram size from min to max, every position from max(inp_size - nnew, ngram_size)
// to inp_size-1 contributes the key (the ngram_size tokens ending just before i) and the value
// inp[i], with the count incremented when the pair is already present.
// Returns the number of (key,value) pairs touched, and writes the flattened keys/values/counts.
// keys are laid out as ndone * ngram_size; out_c[i] is the resulting count for pair i.
fn g_cache_update(inp: i64[:], n_new: i64, ng_min: i64, ng_max: i64,
                  keys: i64[:], vals: i64[:], counts: i64[:], sizes: i64[:], maxpairs: i64,
                  marker: i64) -> i64 {
    if marker != 205 {
        return -1;
    }
    if ng_max < ng_min || ng_min < 1 || n_new < 0 {
        return -1;
    }
    var ndone: i64 = 0;
    var size: i64 = ng_min;
    while size <= ng_max {
        var start: i64 = inp.len - n_new;
        if start < size {
            start = size;
        }
        var i: i64 = start;
        while i < inp.len {
            if ndone >= maxpairs {
                return ndone;
            }
            // key = inp[i-size .. i-1], value = inp[i]
            var k: i64 = 0;
            while k < size {
                keys[ndone * ng_max + k] = inp[i - size + k];
                k = k + 1;
            }
            vals[ndone] = inp[i];
            sizes[ndone] = size;
            // count: number of earlier pairs with the same key and value
            var c: i64 = 1;
            var j: i64 = 0;
            while j < ndone {
                // the key length is part of the key: comparing a size-2 key against a
                // size-1 key by their shared prefix merged distinct pairs (measured: 53 of
                // 200 random cases came out with fewer unique pairs than the native map)
                var same: i64 = 1;
                if vals[j] != vals[ndone] || sizes[j] != size {
                    same = 0;
                } else {
                    var q: i64 = 0;
                    while q < size {
                        if keys[j * ng_max + q] != keys[ndone * ng_max + q] {
                            same = 0;
                        }
                        q = q + 1;
                    }
                }
                if same == 1 {
                    c = c + counts[j];
                    counts[j] = 0;
                }
                j = j + 1;
            }
            if j > ndone {
                j = ndone;
            }
            counts[ndone] = c;
            ndone = ndone + 1;
            i = i + 1;
        }
        size = size + 1;
    }
    return ndone;
}

fn main() {
    let inp: i64[6] = [1, 2, 3, 1, 2, 3];
    let keys: i64[64] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    let vals: i64[64] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    let cnt: i64[64] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    let szs: i64[64] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    let r = g_cache_update(inp, 6, 1, 2, keys, vals, cnt, szs, 64, 205);
    if r < 0 {
        return;
    }
}
