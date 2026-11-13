// Apex KV shift input and rotation matrix copy in Goose.
//
// set_input_k_shift: one entry per cell of every stream — zero for an empty cell, the accumulated
// shift otherwise. The stream stride is that stream's own cell count, not a common width, so a cache
// whose streams differ in size still lands each entry where the native code puts it.
//
// set_input_k_rot / set_input_v_rot: a straight copy of the precomputed Hadamard matrix for the
// requested width. The width must already be in the table (native asserts on the lookup), and the
// copy covers the whole tensor, which for an F32 square matrix is width² × 4 bytes.
//
// Deliberately not here: set_input_pos_bucket. Its bucket arithmetic mixes float and double
// intermediates, and Goose has f32 but no double, so a port could drift at the integer boundary where
// floorf lands. Deferred with the diagnostic rather than approximated.
//
// Packet:
//   k=0  0 op, 1 out, 2 n_stream, 3 sizes, 4 pos, 5 shift, 6 rot_present, 7 n_rot, 8 n_rot_src
//   k=1  0 count ptr
//   k=2  0 done, 1 ret
//
// Records, three slots: 0 cell entry (a = flat index, b = value), 1 matrix copied (a = bytes).
extern "fg333k_get" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg333k_put" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg333k_ld_i64" fn ld(base:u64,byte:i64)->i64;
extern "fg333k_stw" fn stw(base:u64,byte:i64,v:i64);
extern "fg333k_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn at(arr:u64,i:i64)->i64{return ld(arr,i*(8 as i64));}
fn setat(arr:u64,i:i64,v:i64){stw(arr,i*(8 as i64),v);}

fn emit(out:u64,cntp:u64,kind:i64,a:i64,b:i64){
 let n:i64=at(cntp,0 as i64);
 setat(out,n*(3 as i64),kind);
 setat(out,n*(3 as i64)+1 as i64,a);
 setat(out,n*(3 as i64)+2 as i64,b);
 setat(cntp,0 as i64,n+(1 as i64));
}

fn run(packet:i64)->bool{
 let op:i64=get(packet,0,0) as i64;
 let out:u64=get(packet,0,1);
 let n_stream:i64=get(packet,0,2) as i64;
 let sizes:u64=get(packet,0,3);
 let pos:u64=get(packet,0,4);
 let shift:u64=get(packet,0,5);
 let rot_present:i64=get(packet,0,6) as i64;
 let n_rot:i64=get(packet,0,7) as i64;
 let n_rot_src:i64=get(packet,0,8) as i64;
 let cntp:u64=get(packet,1,0);
 var ret:i64=0;

 if op==(0 as i64){
  ensure(n_stream>=(0 as i64),60);
  ensure(pos!=(0 as u64)&&shift!=(0 as u64)&&sizes!=(0 as u64),61);
  var off:i64=0;
  var s:i64=0;
  while s<n_stream{
   let sz:i64=at(sizes,s);
   var i:i64=0;
   while i<sz{
    var v:i64=0 as i64;
    if at(pos,off+i)!=(0 as i64)-1 as i64{ v=at(shift,off+i); }
    emit(out,cntp,0 as i64,s*sz+i,v);
    i=i+1 as i64;
   }
   off=off+sz;
   s=s+1 as i64;
  }
  ret=off;
 }else{
  if op==(1 as i64){
   if rot_present==(0 as i64){
    // native asserts on the table lookup; the port reports -1 instead of aborting the host
    put(packet,2,0,1 as u64);
    put(packet,2,1,((0 as i64)-1 as i64) as u64);
    return true;
   }
   if n_rot_src!=n_rot{
    put(packet,2,0,1 as u64);
    put(packet,2,1,((0 as i64)-1 as i64) as u64);
    return true;
   }
   let bytes:i64=n_rot*n_rot*(4 as i64);
   emit(out,cntp,1 as i64,bytes,0 as i64);
   ret=bytes;
  }else{
   ensure(false,64);
  }
 }
 put(packet,2,0,1 as u64);
 put(packet,2,1,ret as u64);
 return true;
}

fn main(){run(0);}
