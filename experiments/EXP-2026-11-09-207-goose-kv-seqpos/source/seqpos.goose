// KV per-sequence position index in Goose (APX: P5-01, the "sequence" clause).
//
// Native keeps, per sequence, a std::set<(pos, cell)> ordered by position
// (llama-kv-cells.h:528). Its three consumers are:
//   seq_pos_min / seq_pos_max  (:373-396)  begin()/rbegin() first->first
//   seq_pos_rm / seq_pos_add   (:544-562)  erase/insert one cell per sequence bit
//   seq_pos_get                (:322)      the ordered view, used only by
//                                          llama-memory-hybrid-idx.cpp:414
//
// The third is NOT reachable in this profile: hybrid-idx memory is not instantiated for Apex
// (`n_stream_kv` path absent, no hybrid-idx log line). Porting the ordered container would mean
// reimplementing std::set in Goose for a consumer that never runs, so this stage ports the two
// decisions that ARE reached -- min and max -- and records the third as not-reachable rather than
// pretending it is done.
//
// min/max are a scan over the cells that carry the sequence, which is what the native set answers
// in O(1) because it stays ordered. That is a deliberate trade: the scan is O(cells) but it is
// called a handful of times per ubatch, and it removes an ordered container from the port. The
// performance is measured, not assumed -- see the Apex row in the README.
extern "fg207p_fail" fn fail(code: i64);
extern "fg207p_ld_i64" fn ld_i64(base: u64, byte: i64) -> i64;

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}
// seq_has_bit: seq words for cell i, word w, bit b. W words per cell.

fn has_bit(seqw: u64, i: i64, s: i64, w: i64) -> i64 {
    ensure(i >= 0 && s >= 0 && w > 0, 700);
    let widx: i64 = s / 64 as i64;
    let bit: i64 = (1 as i64) << (s % 64 as i64);

    if widx >= w {
        return 0 as i64;
    }
    let word: i64 = ld_i64(seqw, (i * w + widx) * 8 as i64);

    if word & bit != 0 as i64 {
        return 1 as i64;
    }
    return 0 as i64;
}
// seq_pos_min: llama-kv-cells.h:373. Smallest position among the cells carrying `s`, or -1.

fn seq_pos_min(pos: u64, seqw: u64, n: i64, s: i64, w: i64) -> i64 {
    ensure(n >= 0 && s >= 0 && w > 0, 710);
    var best: i64 = -1;
    var i: i64 = 0;

    while i < n {
        if has_bit(seqw, i, s, w) == 1 as i64 {
            let p: i64 = ld_i64(pos, i * 8 as i64);
            // native seq_pos only ever holds non-empty cells: rm() resets the bits before the position
            // becomes -1, and pos_set() inserts after it is set. A stale bit on an empty cell must not
            // contribute, which is what the first version of this scan got wrong.

            if p != (0 as i64) - 1 as i64 {
                if best == (0 as i64) - 1 as i64 {
                    best = p;
                } else {
                    if p < best {
                        best = p;
                    }
                }
            }
        }
        i += 1;
    }
    return best;
}
// seq_pos_max: llama-kv-cells.h:386. Largest position among the cells carrying `s`, or -1.

fn seq_pos_max(pos: u64, seqw: u64, n: i64, s: i64, w: i64) -> i64 {
    ensure(n >= 0 && s >= 0 && w > 0, 720);
    var best: i64 = -1;
    var i: i64 = 0;

    while i < n {
        if has_bit(seqw, i, s, w) == 1 as i64 {
            let p: i64 = ld_i64(pos, i * 8 as i64);

            if p != (0 as i64) - 1 as i64 {
                if best == (0 as i64) - 1 as i64 {
                    best = p;
                } else {
                    if p > best {
                        best = p;
                    }
                }
            }
        }
        i += 1;
    }
    return best;
}

fn main() {
    has_bit(0 as u64, 0 as i64, 0 as i64, 0 as i64);
    seq_pos_min(0 as u64, 0 as u64, 0 as i64, 0 as i64, 0 as i64);
    seq_pos_min(8 as u64, 16 as u64, 1 as i64, 1 as i64, 4 as i64);
    seq_pos_max(0 as u64, 0 as u64, 0 as i64, 0 as i64, 0 as i64);
    seq_pos_max(8 as u64, 16 as u64, 1 as i64, 1 as i64, 4 as i64);
}

