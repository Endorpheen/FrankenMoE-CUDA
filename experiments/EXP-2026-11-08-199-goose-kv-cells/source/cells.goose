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
}
