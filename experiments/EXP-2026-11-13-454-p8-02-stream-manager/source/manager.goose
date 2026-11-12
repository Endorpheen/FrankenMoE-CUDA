// Stream session manager policy in Goose, from tools/server/server-stream.cpp
// (create_or_replace, evict, evict_and_cancel, stop_gc, gc_loop).
//
// Owned decisions:
//   - create_or_replace keeps "one conversation = at most one live session" and
//     retires the previous session by cancelling it first and then finalizing it;
//   - evict retires a found session by finalizing it only (its producer is not
//     cancelled);
//   - evict_and_cancel retires a found session by cancelling first, then
//     finalizing, so a producer drain loop stops pulling output;
//   - stop_all finalizes every session without cancelling;
//   - GC drops a session only when it has completed (completed_at != 0) and its
//     completion time is at or below now - TTL.
//
// The map, the shared_mutex, the thread and the notify calls stay native.
extern "fg454_st_i64" fn st_i64(p:u64,o:u64,v:i64);

fn put(out:u64,i:i64,v:i64){ st_i64(out,(i*(8 as i64)) as u64,v); }

// op: 1 create_or_replace (found), 2 evict (found), 3 evict_and_cancel (found).
// Writes replace (0/1), cancel_old (0/1), finalize_old (0/1).
fn retire_plan(op:i64,found:i64,out:u64)->i64{
 var replace:i64=0 as i64;
 var cancel:i64=0 as i64;
 var finalize:i64=0 as i64;
 if found!=0 as i64 {
  finalize=1 as i64;
  if op==1 as i64 {
   replace=1 as i64;
   cancel=1 as i64;
  }
  if op==3 as i64 { cancel=1 as i64; }
 }
 put(out,0 as i64,replace);
 put(out,1 as i64,cancel);
 put(out,2 as i64,finalize);
 return 0 as i64;
}

// stop_gc finalizes every live session, never cancels.
fn stop_all_plan(out:u64)->i64{
 put(out,0 as i64,0 as i64);  // cancel
 put(out,1 as i64,1 as i64);  // finalize
 return 0 as i64;
}

// gc_cutoff(now, ttl) -> now - ttl
fn gc_cutoff(now:i64,ttl:i64,out:u64)->i64{
 put(out,0 as i64,now-ttl);
 return 0 as i64;
}

// gc keeps a session while it is alive or not yet expired.
fn gc_decision(completed:i64,now:i64,ttl:i64,out:u64)->i64{
 var drop:i64=0 as i64;
 if completed!=0 as i64 {
  if completed<=(now-ttl) { drop=1 as i64; }
 }
 put(out,0 as i64,drop);
 return 0 as i64;
}

fn main(){
 retire_plan(1 as i64,1 as i64,0 as u64);
 stop_all_plan(0 as u64);
 gc_cutoff(0 as i64,300 as i64,0 as u64);
 gc_decision(0 as i64,0 as i64,300 as i64,0 as u64);
}
