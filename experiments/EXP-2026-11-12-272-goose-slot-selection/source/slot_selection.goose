// P8-03: server slot-selection decisions, in Goose.
//
// Native reference: tools/server/server-context.cpp.
//
//   1. pick_similar(): among eligible slots, choose the one whose prompt LCP fraction is strictly the largest and
//      strictly above the threshold. Ties keep the earlier slot. The fraction is computed in f32 exactly as the native
//      code does (lcp / n_tokens), so rounding behaviour is identical.
//   2. pick_lru(): the first free slot, preferring empty prompts, then the smallest t_last_used, ties keeping the
//      earlier slot -- the same order std::stable_sort produces in get_free_slots().
//
// Both return an index or -1; the slots themselves, their state and the prompt cache stay in the server.

extern "fg272s_fail" fn fail(c:i64);
extern "fg272s_ld_i64" fn ld_i64(base:u64, byte:i64)->i64;
extern "fg272s_st_i64" fn st_i64(base:u64, byte:i64, v:i64);
extern "fg272s_from_bits" fn from_bits(x:u32)->f32;

fn ensure(v:bool,c:i64){if !v{fail(c);}}
fn at(a:u64, i:i64)->i64 {return ld_i64(a, i*8 as i64);}

fn pick_similar(eligible:u64, lcp:u64, count:i64, n_tokens:i64, thr_bits:i64) -> i64 {
    ensure(count >= 0 as i64 && n_tokens > 0 as i64, 10);
    let thr:f32 = from_bits(thr_bits as! u32);
    var best:f32 = 0 as f32;
    var ret:i64 = -1 as i64;
    var i:i64 = 0 as i64;
    while i < count {
        if at(eligible, i) != 0 as i64 {
            let sim:f32 = (at(lcp, i) as f32) / (n_tokens as f32);
            if sim > best && sim > thr {
                best = sim;
                ret  = i;
            }
        }
        i = i + 1 as i64;
    }
    ret
}

fn pick_lru(idle:u64, empty:u64, t_last:u64, count:i64) -> i64 {
    ensure(count >= 0 as i64, 11);
    var ret:i64 = -1 as i64;
    var i:i64 = 0 as i64;
    while i < count {
        if at(idle, i) != 0 as i64 {
            if ret < 0 as i64 {
                ret = i;
            } else {
                let e_best:i64 = at(empty, ret);
                let e_cur:i64  = at(empty, i);
                if (e_best == 0 as i64 && e_cur != 0 as i64) ||
                   (e_best == e_cur && at(t_last, i) < at(t_last, ret)) {
                    ret = i;
                }
            }
        }
        i = i + 1 as i64;
    }
    ret
}

fn main(){
    pick_similar(0 as u64, 0 as u64, 0 as i64, 1 as i64, 0 as i64);
    pick_lru(0 as u64, 0 as u64, 0 as u64, 0 as i64);
}
