// P7-03: mmap/lock bookkeeping decisions, in Goose.
//
// Native reference: llama-model-loader.cpp, load_all_data() and init_mappings().
//
//   should_lock(has_locks, is_lazy): a lazy tensor must not be locked, because locking faults the whole mapping in --
//   exactly what lazy loading avoids. This is the per-tensor gate before llama_mlock::grow_to().
//
//   range_update(cur_first, cur_second, off, size): the used-range bookkeeping per mapping file, a min/max update
//   with an empty-range sentinel convention (first = SIZE_MAX, second = 0).
//
// Both return values; the mlock and mmap calls themselves stay OS primitives.

extern "fg286m_fail" fn fail(c:i64);
extern "fg286m_st_i64" fn st_i64(base:u64, byte:i64, v:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn should_lock(has_locks:i64, is_lazy:i64) -> i64 {
    ensure(has_locks >= 0 as i64 && is_lazy >= 0 as i64, 10);
    if has_locks != 0 as i64 && is_lazy == 0 as i64 {return 1 as i64;}
    return 0 as i64;
}

// returns the packed (first, second) in two outputs: out[0] = first, out[1] = second
fn range_update(first:i64, second:i64, off:i64, size:i64, out:u64) -> bool {
    ensure(size >= 0 as i64, 11);
    let new_first:i64 = if off < first {off} else {first};
    let end:i64 = off + size;
    let new_second:i64 = if end > second {end} else {second};
    st_i64(out, 0 as i64, new_first);
    st_i64(out, 8 as i64, new_second);
    true
}


fn main(){
    should_lock(0 as i64, 0 as i64);
    range_update(0 as i64, 0 as i64, 0 as i64, 0 as i64, 0 as u64);
}
