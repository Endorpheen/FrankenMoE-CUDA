// Maximum number of drafted tokens across the configured speculative types
// (common_speculative_n_max over common_params_speculative).
// Type ids follow enum common_speculative_type in common.h:
//   0 NONE, 1 DRAFT_SIMPLE, 2 EAGLE3, 3 MTP, 4 DFLASH, 5 DSPARK,
//   6 NGRAM_SIMPLE, 7 MAP_K, 8 MAP_K4V, 9 NGRAM_MOD, 10 NGRAM_CACHE, 11 COUNT
// size_m[i] is the ngram size for the ngram types, draft_nmax[i] the draft width for
// the model-based types; each array is read only for the types that use it.
fn g_spec_n_max(types: i64[:], size_m: i64[:], draft_nmax: i64[:], ntypes: i64, marker: i64) -> i64 {
    if marker != 195 {
        return -1;
    }
    if ntypes < 0 || types.len < ntypes {
        return -1;
    }
    var nmax: i64 = 0;
    var i: i64 = 0;
    while i < ntypes {
        let t = types[i];
        if t == 1 || t == 2 || t == 3 || t == 4 || t == 5 {
            var d = draft_nmax[i];
            if d < 0 {
                d = 0;
            }
            if d > nmax {
                nmax = d;
            }
        } else {
            if t == 6 || t == 7 || t == 8 {
                if size_m[i] > nmax {
                    nmax = size_m[i];
                }
            } else {
                if t == 9 {
                    var d2 = draft_nmax[i];
                    if d2 < 0 {
                        d2 = 0;
                    }
                    if d2 > nmax {
                        nmax = d2;
                    }
                } else {
                    if t == 10 {
                        if 8 > nmax {
                            nmax = 8;
                        }
                    }
                }
            }
        }
        i = i + 1;
    }
    return nmax;
}

fn main() {
    let t: i64[3] = [3, 6, 0];
    let s: i64[3] = [0, 4, 0];
    let d: i64[3] = [5, 0, 0];
    let r = g_spec_n_max(t, s, d, 3, 195);
    if r < 0 {
        return;
    }
}
