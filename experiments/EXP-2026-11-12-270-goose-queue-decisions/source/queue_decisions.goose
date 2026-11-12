// P8-03: server queue scheduling decisions, in Goose.
//
// Native reference: tools/server/server-queue.cpp.
//
//   1. pop_deferred_task(): when a slot changes state, take the first deferred task that targets that slot;
//      if none does, take the first deferred task. The decision is which index to pop.
//   2. cleanup_pending_task(): a cancel for task N removes every queued and deferred task targeting N.
//      The decision is which entries to drop, over three queues that are cleaned together.
//
// Both are pure functions over flat arrays; the queues, their locks and their ownership stay in the server.

extern "fg270q_fail" fn fail(c:i64);
extern "fg270q_ld_i64" fn ld_i64(base:u64, byte:i64)->i64;
extern "fg270q_st_i64" fn st_i64(base:u64, byte:i64, v:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}
fn at(a:u64, i:i64)->i64 {return ld_i64(a, i*8 as i64);}
fn setbit(m:u64, i:i64){st_i64(m, (i/64 as i64)*8 as i64, at(m, i/64 as i64) | ((1 as i64) << (i % (64 as i64))));}

// index of the deferred task to pop, or -1 when the deferred queue is empty.
fn pick_deferred(slot_of:u64, count:i64, id_slot:i64) -> i64 {
    ensure(count >= 0 as i64, 10);
    if count == 0 as i64 {return -1 as i64;}
    var i:i64 = 0 as i64;
    while i < count {
        if at(slot_of, i) == id_slot {return i;}
        i = i + 1 as i64;
    }
    return 0 as i64;
}

// mask of entries whose task id equals id_target, over one queue of `count` tasks.
fn cleanup_mask(task_ids:u64, count:i64, id_target:i64, out_mask:u64) -> i64 {
    ensure(count >= 0 as i64, 11);
    var n:i64 = 0 as i64;
    var i:i64 = 0 as i64;
    while i < count {
        if at(task_ids, i) == id_target {
            setbit(out_mask, i);
            n = n + 1 as i64;
        }
        i = i + 1 as i64;
    }
    n
}

fn main(){
    pick_deferred(0 as u64, 0 as i64, 0 as i64);
    cleanup_mask(0 as u64, 0 as i64, 0 as i64, 0 as u64);
}
