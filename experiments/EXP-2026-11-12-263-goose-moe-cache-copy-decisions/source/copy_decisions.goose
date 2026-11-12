// P1-10: upload-copy decisions of the MoE device cache, in Goose.
//
// Native reference: llama-moe-cache.cpp, copy_experts() and copy(). Two decisions live there and nowhere else:
//
//   1. how many consecutive experts one copy may move -- along the slot map for the read path (copy_experts) and
//      along the fill list for the write path (copy). A run ends at a negative slot, at the end of the range, or at
//      the first pair that is not (expert+n, slot+n).
//   2. which statistics bucket a batch belongs to: small up to 8*n_expert_used ids, large above.
//
// The copies themselves are ggml primitives, and the SLRU plan that produces the fills is already Goose (EXP-150).

extern "fg263c_fail" fn fail(c:i64);
extern "fg263c_ld_i32" fn ld_i32(base:u64, byte:i64)->i64;
extern "fg263c_st_i64" fn st_i64(base:u64, byte:i64, v:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}
fn at32(a:u64, i:i64)->i64 {return ld_i32(a, i*4 as i64);}

// copy_experts: length of the run of consecutive slots starting at expert e, last inclusive.
// A negative slot means the expert is not cached and nothing is copied.
fn run_from_slot(slots:u64, e:i64, last:i64) -> i64 {
    ensure(e >= 0 as i64 && last >= 0 as i64, 10);
    let s0:i64 = at32(slots, e);
    if s0 < 0 as i64 {return 0 as i64;}
    var n:i64 = 1 as i64;
    while e + n <= last && at32(slots, e + n) == s0 + n {
        n = n + 1 as i64;
    }
    n
}

// copy: length of the run of (expert, slot) pairs starting at index i of the fill list.
fn run_from_fills(expert:u64, slot:u64, i:i64, n_fills:i64) -> i64 {
    ensure(i >= 0 as i64 && i < n_fills, 11);
    let e0:i64 = at32(expert, i);
    let s0:i64 = at32(slot, i);
    var n:i64 = 1 as i64;
    while i + n < n_fills && at32(expert, i + n) == e0 + n && at32(slot, i + n) == s0 + n {
        n = n + 1 as i64;
    }
    n
}

// stats bucket: 0 = small, 1 = large.
fn stats_class(n_ids:i64, n_expert_used:i64) -> i64 {
    ensure(n_ids >= 0 as i64 && n_expert_used >= 0 as i64, 12);
    if n_ids <= 8 as i64 * n_expert_used {return 0 as i64;}
    return 1 as i64;
}

// total bytes one expert tensor contributes, summing the runs over the fill list.
fn fill_bytes(expert:u64, slot:u64, n_fills:i64, expert_size:i64) -> i64 {
    ensure(expert_size >= 0 as i64, 13);
    var i:i64 = 0 as i64;
    var bytes:i64 = 0 as i64;
    while i < n_fills {
        let n:i64 = run_from_fills(expert, slot, i, n_fills);
        bytes = bytes + n * expert_size;
        i = i + n;
    }
    bytes
}

fn main(){
    run_from_slot(0 as u64, 0 as i64, 0 as i64);
    run_from_fills(0 as u64, 0 as u64, 0 as i64, 1 as i64);
    stats_class(0 as i64, 1 as i64);
    fill_bytes(0 as u64, 0 as u64, 1 as i64, 1 as i64);
}
