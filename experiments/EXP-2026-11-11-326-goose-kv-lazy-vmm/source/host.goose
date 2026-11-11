// Apex lazy VMM KV buffer policy in Goose (P5-06).
//
// With a backend that can reserve virtual memory and commit pages on demand, the KV cache allocates
// its tensors from that buffer instead of a plain device buffer — but only when the value cache is
// not transposed. A transposed V spreads each cell across the whole tensor, so the committed range
// would have to be the whole tensor and the reservation buys nothing.
//
// Committing is expressed per tensor as (offset, size): the offset of the tensor inside its buffer
// plus one row per requested cell. When nothing is lazy, or no cells are requested, it is a no-op.
//
// Three call sites request a commit, and each takes the highest cell it will touch:
//   state write : the largest range end
//   state read  : the largest run end
//   ubatch apply: n_kv for the current slots
//
// Packet, int64 slots:
//   k=0  0 op, 1 out, 2 n_kv, 3 vmm_lazy, 4 offload, 5 v_trans, 6 lazy_available, 7 n_layer,
//        8 n_stream, 9 ranges, 10 n_ranges
//   k=1  0 tensors, 1 count ptr
//   k=2  0 count ptr (output)
//   k=3  0 done, 1 ret
extern "fg326k_get" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg326k_put" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg326k_ld_i64" fn ld(base:u64,byte:i64)->i64;
extern "fg326k_stw" fn stw(base:u64,byte:i64,v:i64);
extern "fg326k_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn at(arr:u64,i:i64)->i64{return ld(arr,i*(8 as i64));}
fn setat(arr:u64,i:i64,v:i64){stw(arr,i*(8 as i64),v);}

fn emit(out:u64,cntp:u64,off:i64,size:i64){
 let n:i64=at(cntp,0 as i64);
 setat(out,n*(2 as i64),off);
 setat(out,n*(2 as i64)+1 as i64,size);
 setat(cntp,0 as i64,n+(1 as i64));
}

fn run(packet:i64)->bool{
 let op:i64=get(packet,0,0) as i64;
 let out:u64=get(packet,0,1);
 let n_kv:i64=get(packet,0,2) as i64;
 let vmm_lazy:i64=get(packet,0,3) as i64;
 let offload:i64=get(packet,0,4) as i64;
 let v_trans:i64=get(packet,0,5) as i64;
 let lazy_avail:i64=get(packet,0,6) as i64;
 let n_layer:i64=get(packet,0,7) as i64;
 let n_stream:i64=get(packet,0,8) as i64;
 let ranges:u64=get(packet,0,9);
 let n_ranges:i64=get(packet,0,10) as i64;
 let tensors:u64=get(packet,1,0);
 let cntp:u64=get(packet,2,0);
 var ret:i64=0;

 if op==(0 as i64){
  // buffer choice: lazy only when offloaded, not transposed, and available
  var use_lazy:i64=0;
  if offload!=(0 as i64){
   if v_trans==(0 as i64){
    if lazy_avail!=(0 as i64){ use_lazy=1 as i64; }
   }
  }
  setat(out,0 as i64,use_lazy);
  setat(out,1 as i64,use_lazy);
  ret=use_lazy;
 }else{
  if op==(1 as i64){
   // commit plan: one (offset, size) per present tensor, when lazy and n_kv > 0
   if vmm_lazy==(0 as i64){ ret=0 as i64; }else{
    if n_kv==(0 as i64){ ret=0 as i64; }else{
     let count:i64=n_layer*n_stream*(2 as i64);
     var i:i64=0;
     while i<count{
      let present:i64=at(tensors,i*(4 as i64));
      let off:i64=at(tensors,i*(4 as i64)+1 as i64);
      let row:i64=at(tensors,i*(4 as i64)+2 as i64);
      if present!=(0 as i64){
       if row>=(0 as i64){
        emit(out,cntp,off,row*n_kv);
       }
      }
      i=i+1 as i64;
     }
     ret=at(cntp,0 as i64);
    }
   }
  }else{
   if op==(2 as i64){
    // highest cell touched by the ranges (0 when there are none)
    var mx:i64=0;
    var r:i64=0;
    while r<n_ranges{
     let hi:i64=at(ranges,(2 as i64*r+1 as i64));
     if hi>mx{ mx=hi; }
     r=r+1 as i64;
    }
    ret=mx;
   }else{
    ensure(false,62);
   }
  }
 }
 put(packet,3,0,1 as u64);
 put(packet,3,1,ret as u64);
 return true;
}

fn main(){run(0);}
