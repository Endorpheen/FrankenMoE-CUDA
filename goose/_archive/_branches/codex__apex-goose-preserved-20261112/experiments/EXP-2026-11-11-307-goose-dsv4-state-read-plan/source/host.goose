// Apex DSV4 compressed-state restore, header and call order in Goose:
// llama_dsv4_comp_state::state_read, without the tensor bytes (EXP306 transfers the per-stream
// records themselves).
//
// The restore reads a six-field header, validates it, resolves the destination stream range, and
// then for every layer checks the layer id and performs two tensor-stream reads (kv then score).
// The order and the rejection codes are the contract; a restore that reads one tensor too many, or
// accepts a header it should not, is exactly what this pins.
//
// Rejections, in native order:
//   1 version mismatch
//   2 ratio / state size / embedding size mismatch
//   3 layer count mismatch
//   4 destination stream range mismatch or out-of-range sequence
//   5 layer id mismatch
// Native throws; the FFI form is a negative code, stated and pinned.
//
// Packet, int64 slots:
//   k=0  0 version_ref, 1 ratio_ref, 2 state_size_ref, 3 n_embd_ref, 4 ns_ref, 5 n_layer_ref,
//        6 seq_id, 7 n_stream, 8 ratio, 9 state_size, 10 n_embd, 11 n_layers, 12 il_ref
//   k=3  0 done, 1 ret (negative code, or the number of tensor reads performed)
extern "fg307k_get" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg307k_put" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg307k_ld_i64" fn ld(base:u64,byte:i64)->i64;
extern "fg307k_st_i64" fn st(base:u64,byte:i64,v:i64);
extern "fg307k_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn at(arr:u64,i:i64)->i64{return ld(arr,i*(8 as i64));}

fn run(packet:i64)->bool{
 let version_ref:i64=get(packet,0,0) as i64;
 let ratio_ref:i64=get(packet,0,1) as i64;
 let state_size_ref:i64=get(packet,0,2) as i64;
 let n_embd_ref:i64=get(packet,0,3) as i64;
 let ns:i64=get(packet,0,4) as i64;
 let n_layer_ref:i64=get(packet,0,5) as i64;
 let seq_id:i64=get(packet,0,6) as i64;
 let n_stream:i64=get(packet,0,7) as i64;
 let ratio:i64=get(packet,0,8) as i64;
 let state_size:i64=get(packet,0,9) as i64;
 let n_embd:i64=get(packet,0,10) as i64;
 let n_layers:i64=get(packet,0,11) as i64;
 let il_ref:u64=get(packet,0,12);
 ensure(n_layers>=(0 as i64)&&n_stream>(0 as i64),60);

 var ret:i64=0;
 if version_ref!=(1 as i64){
  ret=(0 as i64)-1 as i64;
 }else{
  if ratio_ref!=ratio||state_size_ref!=state_size||n_embd_ref!=n_embd{
   ret=(0 as i64)-2 as i64;
  }else{
   if n_layer_ref!=n_layers{
    ret=(0 as i64)-3 as i64;
   }else{
    // dsv4_state_dst_stream_range
    var s0:i64=0;
    var ok_range:bool=true;
    if seq_id >= (0 as i64){
     if ns!=(1 as i64){ ok_range=false; }
     if ok_range{
      if n_stream > (1 as i64){
       if seq_id>=n_stream{ ok_range=false; }else{ s0=seq_id; }
      }else{ s0=0; }
     }
    }else{
     if ns!=n_stream{ ok_range=false; }else{ s0=0; }
    }
    if !ok_range{
     ret=(0 as i64)-4 as i64;
    }else{
     var reads:i64=0;
     var il:i64=0;
     var bad:bool=false;
     while il<n_layers&&!bad{
      if at(il_ref,il)!=il{ bad=true; }else{
       reads=reads+2 as i64;
      }
      il=il+1 as i64;
     }
     if bad{ ret=(0 as i64)-5 as i64; }else{ ret=reads; }
    }
   }
  }
 }
 put(packet,3,0,1 as u64);
 put(packet,3,1,ret as u64);
 return true;
}

fn main(){run(0);}
