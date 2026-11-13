// Apex recurrent state metadata in Goose:
// llama_memory_recurrent::state_write_meta and the whole-cache branch of state_read_meta.
//
// The metadata stream is a flat sequence of 32-bit values: pos, n_seq_id, then n_seq_id
// sequence ids per cell, for every cell in the given ranges. Reading it back either rebuilds a
// single sequence through find_slot (the dest_seq_id branch, which EXP293 covers separately) or
// restores the whole cache: clear, read every cell, reject out-of-range ids and duplicate tails,
// then head = 0, used = cell_count and src = cell_id for every restored cell.
//
// I/O stays external: the host supplies a flat byte buffer and two 32-bit primitives.
//
// Cell arena is the same layout as EXP293/EXP295. The stream cursor lives in the arena too:
//   ... | head | used | n | rs_idx... | stream_off
// rs_idx is not used by these two functions and is sized 0 here.
//
// Packet, int64 slots:
//   k=0  0 op, 1 arena, 2 n, 3 SW, 4 n_seq_max, 5 stream, 6 range_count, 7 ranges
//   k=1  0 dest_seq_id, 1 cell_count
//   k=3  0 done, 1 ret
extern "fg296k_get" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg296k_put" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg296k_ld_i64" fn ld(base:u64,byte:i64)->i64;
extern "fg296k_st_i64" fn st(base:u64,byte:i64,v:i64);
extern "fg296k_wr32" fn wr32(base:u64,off:i64,v:i64);
extern "fg296k_rd32" fn rd32(base:u64,off:i64)->i64;
extern "fg296k_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn pos_off(i:i64)->i64{return i*8 as i64;}
fn src_off(n:i64,i:i64)->i64{return (n+i)*8 as i64;}
fn src0_off(n:i64,i:i64)->i64{return (2*n+i)*8 as i64;}
fn tail_off(n:i64,i:i64)->i64{return (3*n+i)*8 as i64;}
fn seqw_off(n:i64,i:i64,sw:i64,w:i64)->i64{return (4*n+i*sw+w)*8 as i64;}
fn head_off(n:i64,sw:i64)->i64{return (4*n+n*sw)*8 as i64;}
fn used_off(n:i64,sw:i64)->i64{return (4*n+n*sw+1 as i64)*8 as i64;}
fn cursor_off(n:i64,sw:i64)->i64{return (4*n+n*sw+3 as i64)*8 as i64;}

fn bit_of(s:i64)->i64{
 var b:i64=1 as i64;
 var k:i64=0;
 while k<s%64 as i64{
  b=b*2 as i64;
  k=k+1 as i64;
 }
 b
}

fn seq_test(a:u64,n:i64,i:i64,sw:i64,s:i64)->bool{
 return (ld(a,seqw_off(n,i,sw,s/64 as i64))&bit_of(s))!=(0 as i64);
}

fn seq_clear(a:u64,n:i64,i:i64,sw:i64){
 var w:i64=0;
 while w<sw{ st(a,seqw_off(n,i,sw,w),0 as i64); w=w+1 as i64; }
}

fn seq_set(a:u64,n:i64,i:i64,sw:i64,s:i64){
 let wi:i64=s/64 as i64;
 st(a,seqw_off(n,i,sw,wi),ld(a,seqw_off(n,i,sw,wi))|bit_of(s));
}

fn seq_size(a:u64,n:i64,i:i64,sw:i64)->i64{
 var c:i64=0;
 var s:i64=0;
 while s<sw*64 as i64{
  if seq_test(a,n,i,sw,s){ c=c+1 as i64; }
  s=s+1 as i64;
 }
 c
}

fn clear_cells(a:u64,n:i64,sw:i64){
 var i:i64=0;
 while i<n{
  st(a,pos_off(i),(0 as i64)-1 as i64);
  seq_clear(a,n,i,sw);
  st(a,src_off(n,i),(0 as i64)-1 as i64);
  st(a,tail_off(n,i),(0 as i64)-1 as i64);
  i=i+1 as i64;
 }
 st(a,head_off(n,sw),0 as i64);
 st(a,used_off(n,sw),0 as i64);
}

// state_write_meta: pos, n_seq_id, ids. n_seq_id is 0 unless the whole cache is written.
fn write_meta(a:u64,n:i64,sw:i64,stream:u64,range_count:i64,ranges:u64,seq_id:i64){
 var off:i64=ld(a,cursor_off(n,sw));
 var r:i64=0;
 while r<range_count{
  let lo:i64=ld(ranges,(2 as i64*r)*(8 as i64));
  let hi:i64=ld(ranges,(2 as i64*r+1 as i64)*(8 as i64));
  var i:i64=lo;
  while i<hi{
   let p:i64=ld(a,pos_off(i));
   var ns:i64=0;
   if seq_id==(0 as i64)-1 as i64{ ns=seq_size(a,n,i,sw); }
   wr32(stream,off,p);
   off=off+(4 as i64);
   wr32(stream,off,ns);
   off=off+(4 as i64);
   if ns>(0 as i64){
    var s:i64=0;
    while s<sw*64 as i64{
     if seq_test(a,n,i,sw,s){
      wr32(stream,off,s);
      off=off+(4 as i64);
     }
     s=s+1 as i64;
    }
   }
   i=i+1 as i64;
  }
  r=r+1 as i64;
 }
 st(a,cursor_off(n,sw),off);
}

// The whole-cache branch of state_read_meta.
fn read_meta_all(a:u64,n:i64,sw:i64,stream:u64,n_seq_max:i64,cell_count:i64)->i64{
 if cell_count>n{ return 0 as i64; }
 clear_cells(a,n,sw);
 var off:i64=ld(a,cursor_off(n,sw));
 var i:i64=0;
 while i<cell_count{
  let p:i64=rd32(stream,off);
  off=off+(4 as i64);
  let ns:i64=rd32(stream,off);
  off=off+(4 as i64);
  st(a,pos_off(i),p);
  var j:i64=0;
  while j<ns{
   let sid:i64=rd32(stream,off);
   off=off+(4 as i64);
   if sid<0 as i64{ return 0 as i64; }
   if sid>=n_seq_max{ return 0 as i64; }
   seq_set(a,n,i,sw,sid);
   let t:i64=ld(a,tail_off(n,sid));
   if t!=(0 as i64)-1 as i64{ return 0 as i64; }
   st(a,tail_off(n,sid),i);
   j=j+1 as i64;
  }
  i=i+1 as i64;
 }
 st(a,head_off(n,sw),0 as i64);
 st(a,used_off(n,sw),cell_count);
 i=0;
 while i<cell_count{
  st(a,src_off(n,i),i);
  i=i+1 as i64;
 }
 st(a,cursor_off(n,sw),off);
 return 1 as i64;
}

fn run(packet:i64)->bool{
 let op:i64=get(packet,0,0) as i64;
 let a:u64=get(packet,0,1);
 let n:i64=get(packet,0,2) as i64;
 let sw:i64=get(packet,0,3) as i64;
 let n_seq_max:i64=get(packet,0,4) as i64;
 let stream:u64=get(packet,0,5);
 let range_count:i64=get(packet,0,6) as i64;
 let ranges:u64=get(packet,0,7);
 let dest:i64=get(packet,1,0) as i64;
 let cell_count:i64=get(packet,1,1) as i64;
 ensure(n>0 as i64&&sw>(0 as i64),70);
 var ret:i64=0;
 if op==(0 as i64){
  write_meta(a,n,sw,stream,range_count,ranges,dest);
  ret=1 as i64;
 }else{
  // dest_seq_id == -1 only: the single-sequence branch needs find_slot (EXP293)
  ensure(dest==(0 as i64)-1 as i64,71);
  ret=read_meta_all(a,n,sw,stream,n_seq_max,cell_count);
 }
 put(packet,3,0,1 as u64);
 put(packet,3,1,ret as u64);
 return true;
}

fn main(){run(0);}
