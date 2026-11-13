// KV cell ownership operations in Goose, from llama-kv-cells.h.
//
// The C++ class keeps its state in std::vector / std::bitset / std::map. This module does not
// reimplement those containers: the host bridge owns a flat arena (pos[], seq[] bitset words,
// used[] flags, seq_pos counts) and Goose performs the *policy* over it -- which cells are
// occupied, what a clear does, what sequence removal leaves behind, what the used range is.
//
// That split is deliberate. The container allocation stays native, which keeps the port honest:
// every arithmetic decision that was inside the class is now in Goose, and the containers are
// storage rather than behaviour.
//
// Cell state layout in the arena, all int64 slots:
//   pos[i]         -1 = empty, else the position
//   seqw[i*w() + w]  W words of LLAMA_MAX_SEQ bits: which sequences see this cell
//   shift[i]       the shift value, 0 when unused
//   used[i]        1 = the cell is in the used set
// The host recomputes the std::set from used[] when it needs the ordered view.
extern "fg199k_fail" fn fail(code:i64);
extern "fg199k_ld_i64" fn ld_i64(base:u64,byte:i64)->i64;
extern "fg199k_st_i64" fn st_i64(base:u64,byte:i64,v:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

// W is written as a literal everywhere rather than a const: the generated code reads consts
// from gg_globals, which the emitter initialises inside gg_program -- and the bridge never runs
// that, so a const here reads as 0. Found by a direct slot dump, not by reading the generated C.
fn w()->i64{return 8 as i64;}

// Arena regions are consecutive and sized by the cell count, so every offset depends on n:
//   pos[n] | seq[n*W] | used[n] | shift[n]        (all int64 slots)
fn seq_base(n:i64)->i64{return n*8 as i64;}
fn used_base(n:i64)->i64{return (n+n*w())*8 as i64;}
fn shift_base(n:i64)->i64{return (n+n*w()+n)*8 as i64;}
fn pos_at(a:u64,i:i64)->i64{return ld_i64(a,i*8 as i64);}
fn seqword(a:u64,n:i64,i:i64,w:i64)->i64{return ld_i64(a,seq_base(n)+(i*w()+w)*8 as i64);}
fn used_at(a:u64,n:i64,i:i64)->i64{return ld_i64(a,used_base(n)+i*8 as i64);}

// is_empty: a cell is empty exactly when its position is -1.
fn is_empty(n:i64,pos:i64)->bool{
 ensure(n>=0,10);
 if pos<0{return true;}
 return pos==(0 as i64)-1 as i64;
}

// rm: clear a non-empty cell. The sequence set, the shift and the used flag all reset, and the
// position becomes -1. Returns nothing; the caller's arena is the state.
fn rm(a:u64,n:i64,i:i64){
 ensure(n>0&&i>=0&&i<n,11);
 ensure(pos_at(a,i)!=(0 as i64)-1 as i64,12);
 st_i64(a,i*8 as i64,(0 as i64)-1 as i64);
 st_i64(a,used_base(n)+i*8 as i64,0 as i64);
 var w:i64=0;
 while w<w(){st_i64(a,seq_base(n)+(i*w()+w)*8 as i64,0 as i64);w+=1;}
 st_i64(a,shift_base(n)+i*8 as i64,0 as i64);
}

// seq_rm: drop one sequence from a cell. Returns 1 when the cell became empty, else 0.
// The caller has already established that the cell is non-empty and holds seq_id.
fn seq_rm(a:u64,n:i64,i:i64,seq_id:i64,widx:i64,bit:i64)->i64{
 ensure(n>0&&i>=0&&i<n,20);
 ensure(seq_id>=0,21);
 ensure(pos_at(a,i)!=(0 as i64)-1 as i64,22);
 let cur:i64=seqword(a,n,i,widx);
 ensure((cur&bit)!=0,23);
 let next:i64=cur-(cur&bit);
 st_i64(a,seq_base(n)+(i*w()+widx)*8 as i64,next);
 // any remaining sequence bit in this cell?
 var any:i64=0;
 var w:i64=0;
 while w<w(){
  let v:i64=if w==widx{next}else{seqword(a,n,i,w)};
  if v!=0{any=1;}
  w+=1;
 }
 if any==0{
  // Native calls seq[i].reset() only in rm(); in seq_rm the bitset keeps its other bits, and
  // here there are none, so the stored words must be zeroed or a later seq_add would see a
  // stale bit. The gate caught exactly that.
  var z:i64=0;
  while z<w(){st_i64(a,seq_base(n)+(i*w()+z)*8 as i64,0 as i64);z+=1;}
  st_i64(a,i*8 as i64,(0 as i64)-1 as i64);
  st_i64(a,used_base(n)+i*8 as i64,0 as i64);
  st_i64(a,shift_base(n)+i*8 as i64,0 as i64);
  return 1 as i64;
 }
 return 0 as i64;
}

// seq_keep: keep only seq_id in the cell. Returns 1 when the cell became empty.
fn seq_keep(a:u64,n:i64,i:i64,seq_id:i64,widx:i64,bit:i64)->i64{
 ensure(n>0&&i>=0&&i<n,30);
 ensure(seq_id>=0,31);
 let cur:i64=seqword(a,n,i,widx);
 if (cur&bit)!=0{
  var w:i64=0;
  while w<w(){
   let v:i64=if w==widx{bit}else{0 as i64};
   st_i64(a,seq_base(n)+(i*w()+w)*8 as i64,v);
   w+=1;
  }
  return 0 as i64;
 }
 var any:i64=0;
 var w:i64=0;
 while w<w(){if seqword(a,n,i,w)!=0{any=1;}w+=1;}
 if any!=0{
  var k:i64=0;
  while k<w(){st_i64(a,seq_base(n)+(i*w()+k)*8 as i64,0 as i64);k+=1;}
  st_i64(a,i*8 as i64,(0 as i64)-1 as i64);
  st_i64(a,used_base(n)+i*8 as i64,0 as i64);
  st_i64(a,shift_base(n)+i*8 as i64,0 as i64);
  return 1 as i64;
 }
 // Native reaches here when the cell is already empty: nothing to do, no trap.
 return 0 as i64;
}

// seq_add: register seq_id on a cell that is already positioned and does not have it.
fn seq_add(a:u64,n:i64,i:i64,widx:i64,bit:i64){
 ensure(n>0&&i>=0&&i<n,40);
 ensure(pos_at(a,i)!=(0 as i64)-1 as i64,41);
 let cur:i64=seqword(a,n,i,widx);
 ensure((cur&bit)==0,42);
 st_i64(a,seq_base(n)+(i*w()+widx)*8 as i64,cur+bit);
}

// pos_set: place a cell at a position, marking it used.
fn pos_set(a:u64,n:i64,i:i64,pos:i64){
 ensure(n>0&&i>=0&&i<n,50);
 ensure(pos>=(0 as i64)-1 as i64,51);
 st_i64(a,i*8 as i64,pos);
 if pos!=(0 as i64)-1 as i64{st_i64(a,used_base(n)+i*8 as i64,1 as i64);}
}

// seq_count: number of sequence bits set in the cell.
fn seq_count(a:u64,n:i64,i:i64)->i64{
 ensure(n>0&&i>=0&&i<n,60);
 var c:i64=0;
 var w:i64=0;
 while w<w(){
  var v:i64=seqword(a,n,i,w);
  var b:i64=0;
  while b<32 as i64{
   if ((v>>b)&1 as i64)!=0{c+=1;}
   b+=1;
  }
  w+=1;
 }
 return c;
}

// used_max_p1: one past the highest index marked used, 0 when none is.
fn used_max_p1(a:u64,n:i64)->i64{
 ensure(n>=0,70);
 var last:i64=-1;
 var i:i64=0;
 while i<n{if used_at(a,n,i)!=0{last=i;}i+=1;}
 if last<0{return 0 as i64;}
 return last+1 as i64;
}


// place_cell: one step of llama_kv_cache::apply_ubatch for a single token, from
// llama-kv-cache.cpp:1147-1178.
//
// Native sequence for token i on cell idx:
//   if (!cells.is_empty(idx)) { rm(idx) }
//   cells.pos_set(idx, ubatch.pos[i])
//   for each of ubatch.seq_id[i][*] : cells.seq_add(idx, seq)
// The extension record (pos_2d x/y and token id) is deliberately not modelled here: it is a
// separate struct in native and carries no decision this profile makes, so it stays with the host.
//
// Returns 1 when an occupied cell was cleared first, else 0, so the caller can count reuse.
// seq_base_addr is the arena's seq region; seq_n is how many sequences this token carries;
// seq_bits[i] is the sequence id of the i-th.
extern "fg199k_seq_bits" fn seq_bit_of(base:u64,i:i64)->i64;

fn place_cell(a:u64,n:i64,idx:i64,pos:i64,seq_n:i64,seqs:u64)->i64{
 ensure(n>0&&idx>=0&&idx<n,80);
 ensure(pos>=(0 as i64)-1 as i64,81);
 ensure(seq_n>=0,82);
 var reused:i64=0;
 if is_empty(n,pos_at(a,idx))==false{
  rm(a,n,idx);
  reused=1 as i64;
 }
 pos_set(a,n,idx,pos);
 var s:i64=0;
 while s<seq_n{
  let sid:i64=ld_i64(seqs,s*8 as i64);
  ensure(sid>=0,83);
  let widx:i64=sid/32 as i64;
  let bit:i64=(1 as i64)<<(sid%32 as i64);
  let cur:i64=seqword(a,n,idx,widx);
  // A bitset set is idempotent, so a repeated sequence id is not an error -- native just sets it
  // again. Only the store is guarded, so a duplicate does not change the word.
  if (cur&bit)==0{
   st_i64(a,seq_base(n)+(idx*w()+widx)*8 as i64,cur+bit);
  }
  s+=1;
 }
 return reused;
}

// plan_cell: the decision apply_ubatch needs before touching the container, with no arena --
// the container itself is the state, so the module only answers "clear first, and at which
// position". r[0]=1 when the cell must be cleared, r[1]=the position to place.
fn plan_cell(occupied:i64,pos:i64,r:u64){
 ensure(pos>=(0 as i64)-1 as i64,100);
 ensure(occupied==0||occupied==1,101);
 st_i64(r,0,if occupied==1{1 as i64}else{0 as i64});
 st_i64(r,8,pos);
}


// find_slot decisions, from llama-kv-cache.cpp:1026-1094 and llama-hparams.h:506.

// head_start: line 1026-1029. If enough cells are free before the head, native restarts the
// search at 0 rather than wrapping; that choice decides which cells get reused.
fn head_start(head_cur:i64,used:i64,n_tokens:i64)->i64{
 ensure(head_cur>=0&&used>=0&&n_tokens>=0,120);
 if head_cur>used+2 as i64*n_tokens{return 0 as i64;}
 return head_cur;
}

// is_masked_swa: llama-hparams.h. swa_type is the enum value (0 NONE, 1 STANDARD, 2 CHUNKED,
// 3 SYMMETRIC). Returns 1 when the cell is masked and therefore not reusable.
fn is_masked_swa(n_swa:i64,swa_type:i64,p0:i64,p1:i64)->i64{
 ensure(p0>=0&&p1>=0,130);
 ensure(n_swa>=0,131);
 if swa_type==0 as i64{return 0 as i64;}
 if swa_type==1 as i64{
  if p1-p0>=n_swa{return 1 as i64;}
  return 0 as i64;
 }
 if swa_type==2 as i64{
  // Native divides without a guard; n_swa == 0 is not a configuration it reaches (the SWA type
  // is only chosen for models that configure a window). Mirror the arithmetic, not an assert.
  if n_swa==0 as i64{return 0 as i64;}
  let chunk:i64=(p1/n_swa)*n_swa;
  if p0<chunk{return 1 as i64;}
  return 0 as i64;
 }
 if swa_type==3 as i64{
  let half:i64=n_swa/2 as i64;
  let diff:i64=p1-p0;
  if diff<(0 as i64)-half||diff>half{return 1 as i64;}
  return 0 as i64;
 }
 ensure(false,133);
 return 0 as i64;
}

// can_use: lines 1060-1082. A cell is usable when it is empty, or when it holds exactly one
// sequence whose position is masked by SWA relative to that sequence's max position.
//   occupied 1 when the cell is in use
//   seq_count the number of sequences in the cell
//   pos_cell  the cell position, seq_pos_max_1 the sequence's max position + 1
fn can_use_cell(occupied:i64,seq_count:i64,pos_cell:i64,seq_pos_max_1:i64,
                n_swa:i64,swa_type:i64)->i64{
 ensure(occupied==0||occupied==1,140);
 ensure(seq_count>=0,141);
 if occupied==0{return 1 as i64;}
 if seq_count!=1{return 0 as i64;}
 if is_masked_swa(n_swa,swa_type,pos_cell,seq_pos_max_1)==1{return 1 as i64;}
 return 0 as i64;
}


// seq_rm at cache level, from llama-kv-cache.cpp:404-467.

// pos_in: llama-kv-cells.h:421, `pos[i] >= p0 && pos[i] < p1`.
fn pos_in(pos:i64,p0:i64,p1:i64)->i64{
 ensure(p0>=0,150);
 if pos>=p0&&pos<p1{return 1 as i64;}
 return 0 as i64;
}

// norm_range: lines 416-422. A negative p0 means "from the start", a negative p1 means
// "to the end". The max position value is the native numeric_limits<llama_pos>::max().
fn norm_range(p0:i64,p1:i64,r:u64)->i64{
 let pmax:i64=2147483647 as i64;
 let a:i64=if p0<0 as i64{0 as i64}else{p0};
 let b:i64=if p1<0 as i64{pmax}else{p1};
 st_i64(r,0,a);
 st_i64(r,8,b);
 // Native does not require p0 <= p1; an inverted range simply matches nothing.
 return 1 as i64;
}

// head_after_rm: line 435-437. `if (new_head != size && new_head < head) head = new_head;`
// new_head is `size` when nothing was freed, which the caller passes as freed=0.
fn head_after_rm(freed:i64,new_head:i64,head:i64)->i64{
 ensure(head>=0&&new_head>=0,160);
 if freed==0 as i64{return head;}
 if new_head<head{return new_head;}
 return head;
}


// seq_cp, from llama-kv-cache.cpp:473-560.

// same_stream: `if (s0 == s1)` -- both sequences share a stream, so only metadata changes and
// no buffer copy happens.
fn same_stream(s0:i64,s1:i64)->i64{
 ensure(s0>=0&&s1>=0,170);
 if s0==s1{return 1 as i64;}
 return 0 as i64;
}

// seq_cp_range_ok: `if (seq_id_src == seq_id_dst) return;` -- copying a sequence onto itself is
// a no-op rather than a full copy.
fn seq_cp_self(src:i64,dst:i64)->i64{
 ensure(src>=0&&dst>=0,171);
 if src==dst{return 1 as i64;}
 return 0 as i64;
}

// seq_cp_is_full: lines 517-526. Cross-stream copies are only supported for full buffers; a
// range that starts above 0 and stops short of the end makes the copy partial and the native
// code then asserts.
fn seq_cp_is_full(p0:i64,p1:i64,size:i64)->i64{
 ensure(size>=0,180);
 var full:i64=1;
 if p0>0 as i64&&p0+1 as i64<size{full=0;}
 if p1>0 as i64&&p1+1 as i64<size{full=0;}
 return full;
}


// seq_add / seq_div at cache level, from llama-kv-cache.cpp:592-678 and llama-kv-cells.h:448-496.

// seq_add_skip: line 605-615. A zero shift is a no-op, and an empty range is skipped to avoid
// looping over the whole cache.
fn seq_add_skip(shift:i64,p0:i64,p1:i64)->i64{
 ensure(p0>=0&&p1>=0,190);
 if shift==0 as i64{return 1 as i64;}
 if p0==p1{return 1 as i64;}
 return 0 as i64;
}

// pos_add: llama-kv-cells.h:448. Adds to the position and the shift; if the position goes
// negative the cell is emptied instead. Returns 1 when the cell became empty, which is what the
// caller counts to pick a new head.
//   r[0] new pos, r[1] new shift, r[2] empty flag
fn pos_add(pos:i64,shift:i64,d:i64,r:u64)->i64{
 ensure(pos!=(0 as i64)-1 as i64,191);
 let np:i64=pos+d;
 st_i64(r,8,shift+d);
 if np<0 as i64{
  st_i64(r,0,(0 as i64)-1 as i64);
  st_i64(r,8,0 as i64);
  st_i64(r,16,1 as i64);
  return 1 as i64;
 }
 st_i64(r,0,np);
 st_i64(r,16,0 as i64);
 return 0 as i64;
}

// pos_div: llama-kv-cells.h:477. Divides the position and accumulates the difference into the
// shift, so the two stay consistent.
fn pos_div(pos:i64,shift:i64,d:i64,r:u64){
 ensure(pos!=(0 as i64)-1 as i64,200);
 ensure(d!=0 as i64,201);
 let np:i64=pos/d;
 st_i64(r,0,np);
 st_i64(r,8,shift+(pos-np));
}

// seq_add_head: line 632. `head = new_head != size ? new_head : 0` -- unlike seq_rm this does not
// keep the old head when nothing was freed; it restarts from zero.
fn seq_add_head(freed:i64,new_head:i64)->i64{
 ensure(new_head>=0,210);
 if freed==0 as i64{return 0 as i64;}
 return new_head;
}

// seq_div_skip: `if (d == 1) return;` plus the same empty-range early exit.
fn seq_div_skip(d:i64,p0:i64,p1:i64)->i64{
 ensure(p0>=0&&p1>=0,220);
 if d==1 as i64{return 1 as i64;}
 if p0==p1{return 1 as i64;}
 return 0 as i64;
}


// get_n_kv, from llama-kv-cache.cpp:1285-1299. This runs 150 times per Apex pass and decides the
// KV length the graph is built for, so it is a real decision rather than bookkeeping.
//
// Native:  result = max over streams of min(size, max(n_pad_cur, PAD(used_max_p1, n_pad_cur)))
// where n_pad_cur = max(n_pad, 256).
fn pad_up(v:i64,n:i64)->i64{
 ensure(n>0,230);
 if v%n==0 as i64{return v;}
 return v+(n-v%n);
}

// buf_n_pad: max(n_pad, 256).
fn buf_n_pad(n_pad:i64)->i64{
 ensure(n_pad>=0,231);
 if n_pad>256 as i64{return n_pad;}
 return 256 as i64;
}

// n_kv_one: the per-stream candidate.
fn n_kv_one(size:i64,used_max_p1:i64,n_pad:i64)->i64{
 ensure(size>=0&&used_max_p1>=0,232);
 let pstep:i64=buf_n_pad(n_pad);
 let p:i64=pad_up(used_max_p1,pstep);
 let m:i64=if pstep>p{pstep}else{p};
 if size<m{return size;}
 return m;
}


// update / get_can_shift, from llama-kv-cache.cpp:836-916 and 1213-1223.

// can_shift: line 1213. Step35 uses per-layer RoPE dims, and K-shift assumes a single global
// n_rot, so it is refused for that architecture and whenever a position spans several embeddings.
fn can_shift(is_step35:i64,n_pos_per_embd:i64)->i64{
 ensure(n_pos_per_embd>=0,240);
 if is_step35!=0{return 0 as i64;}
 if n_pos_per_embd>1 as i64{return 0 as i64;}
 return 1 as i64;
}

// shift_needed: line 894-898. The shift graph is built only when RoPE is active and at least one
// layer actually has RoPE. `rope_type_none` is the enum value for LLAMA_ROPE_TYPE_NONE.
fn shift_needed(do_shift:i64,rope_type_none:i64,layers_with_rope:i64)->i64{
 ensure(layers_with_rope>=0,241);
 if do_shift==0 as i64{return 0 as i64;}
 if rope_type_none!=0 as i64{return 0 as i64;}
 if layers_with_rope==0 as i64{return 0 as i64;}
 return 1 as i64;
}

// update_updated: line 891-905. `updated` becomes true only when the shift graph actually ran.
fn update_updated(shift_computed:i64)->i64{
 if shift_computed!=0{return 1 as i64;}
 return 0 as i64;
}

// should_purge: llama-kv-cache.cpp:1228 `cells.seq_pos_min(s) <= seq_pos_max_rm[s]`.
// Returns 1 when the purge loop must call seq_rm for this sequence.
fn should_purge(seq_pos_min:i64,max_rm:i64)->i64{
 ensure(max_rm>=(0 as i64)-1 as i64,110);
 ensure(seq_pos_min>=(0 as i64)-1 as i64,111);
 if max_rm==(0 as i64)-1 as i64{return 0 as i64;}
 if seq_pos_min<=max_rm{return 1 as i64;}
 return 0 as i64;
}

// head_after_place: llama-kv-cache.cpp:1216 `head = sinfo.idxs[s].back() + 1`.
fn head_after_place(last_idx:i64)->i64{
 ensure(last_idx>=(0 as i64)-1 as i64,90);
 return last_idx+1 as i64;
}

// Used range over the arena, so the caller can rebuild the ordered std::set.
fn main(){
 is_empty(0 as i64,0 as i64);
 rm(0 as u64,1 as i64,0 as i64);
 seq_rm(0 as u64,1 as i64,0 as i64,0 as i64,0 as i64,1 as i64);
 seq_keep(0 as u64,1 as i64,0 as i64,0 as i64,0 as i64,1 as i64);
 seq_add(0 as u64,1 as i64,0 as i64,0 as i64,1 as i64);
 pos_set(0 as u64,1 as i64,0 as i64,0 as i64);
 seq_count(0 as u64,1 as i64,0 as i64);
 used_max_p1(0 as u64,1 as i64);
 place_cell(0 as u64,1 as i64,0 as i64,0 as i64,0 as i64,0 as u64);
 head_after_place(0 as i64);
 plan_cell(0 as i64,0 as i64,0 as u64);
 should_purge(0 as i64,0 as i64);
 head_start(0 as i64,0 as i64,0 as i64);
 is_masked_swa(0 as i64,0 as i64,0 as i64,0 as i64);
 can_use_cell(0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64);
 pos_in(0 as i64,0 as i64,0 as i64);
 norm_range(0 as i64,0 as i64,0 as u64);
 head_after_rm(0 as i64,0 as i64,0 as i64);
 same_stream(0 as i64,0 as i64);
 seq_cp_self(0 as i64,0 as i64);
 seq_cp_is_full(0 as i64,0 as i64,0 as i64);
 seq_add_skip(0 as i64,0 as i64,0 as i64);
 pos_add(0 as i64,0 as i64,0 as i64,0 as u64);
 pos_div(0 as i64,0 as i64,1 as i64,0 as u64);
 seq_add_head(0 as i64,0 as i64);
 seq_div_skip(1 as i64,0 as i64,0 as i64);
 pad_up(0 as i64,256 as i64);
 buf_n_pad(0 as i64);
 n_kv_one(0 as i64,0 as i64,0 as i64);
 can_shift(0 as i64,1 as i64);
 shift_needed(0 as i64,0 as i64,0 as i64);
 update_updated(0 as i64);
}
