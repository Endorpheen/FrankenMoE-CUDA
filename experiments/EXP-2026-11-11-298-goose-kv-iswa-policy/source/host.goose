// Apex SWA KV cache policy in Goose (llama_kv_cache_iswa).
//
// The iswa cache is a pair: a non-SWA cache over the layers that are not sliding-window, and an
// SWA cache over the layers that are. This module transfers the parts that decide what those two
// caches look like:
//
//   1. the SWA cache size: min(base, n_swa * (unified ? n_seq_max : 1) + n_ubatch), padded to 256,
//      and replaced by the base size when swa_full is set;
//   2. the layer filters: an external filter AND the layer's own SWA bit, split into base and SWA;
//   3. delegation: seq_rm succeeds only when both caches succeed, the other sequence operations go
//      to both, and the position observers read only the SWA cache (the base is a superset);
//   4. memory breakdown: the two caches' per-buffer-type sizes summed, in key order like the
//      native std::map.
//
// Packet, int64 slots:
//   k=0  0 op, 1 out, 2 size_base, 3 n_swa, 4 unified, 5 n_seq_max, 6 n_ubatch, 7 swa_full,
//        8 n_layer_all, 9 swa_mask, 10 ext_present, 11 ext_mask, 12 a, 13 b, 14 count
//   k=3  0 done, 1 ret
extern "fg298k_get" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg298k_put" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg298k_ld_i64" fn ld(base:u64,byte:i64)->i64;
extern "fg298k_st_i64" fn st(base:u64,byte:i64,v:i64);
extern "fg298k_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn at(arr:u64,i:i64)->i64{return ld(arr,i*(8 as i64));}

// GGML_PAD(x, 256)
fn pad256(x:i64)->i64{
 let r:i64=(x+(255 as i64))/(256 as i64);
 return r*(256 as i64);
}

fn run(packet:i64)->bool{
 let op:i64=get(packet,0,0) as i64;
 let out:u64=get(packet,0,1);
 let size_base:i64=get(packet,0,2) as i64;
 let n_swa:i64=get(packet,0,3) as i64;
 let unified:bool=get(packet,0,4)!=(0 as u64);
 let n_seq_max:i64=get(packet,0,5) as i64;
 let n_ubatch:i64=get(packet,0,6) as i64;
 let swa_full:bool=get(packet,0,7)!=(0 as u64);
 let n_layer_all:i64=get(packet,0,8) as i64;
 let swa_mask:u64=get(packet,0,9);
 let ext_present:bool=get(packet,0,10)!=(0 as u64);
 let ext_mask:u64=get(packet,0,11);
 let ret_in:i64=get(packet,3,1) as i64;
 ensure(n_swa>=(0 as i64)&&n_seq_max>=(0 as i64)&&n_ubatch>=(0 as i64),60);

 var ret:i64=0;
 if op==(0 as i64){
  let scale:i64=1 as i64;
  var sc:i64=scale;
  if unified{ sc=n_seq_max; }
  let want:i64=n_swa*sc+n_ubatch;
  var size_swa:i64=size_base;
  if want<size_base{ size_swa=want; }
  size_swa=pad256(size_swa);
  if swa_full{ size_swa=size_base; }
  ret=size_swa;
 }else{
  if op==(1 as i64){
   // base[il] = filter(il) && !is_swa(il); swa[il] = filter(il) && is_swa(il)
   ensure(n_layer_all>=0 as i64,61);
   var il:i64=0;
   while il<n_layer_all{
    var pass:bool=true;
    if ext_present{ pass=at(ext_mask,il)!=(0 as i64); }
    let sw:bool=at(swa_mask,il)!=(0 as i64);
    var b:i64=0;
    var s:i64=0;
    if pass{
     if !sw{ b=1 as i64; }
     if sw{ s=1 as i64; }
    }
    st(out,(2 as i64*il)*(8 as i64),b);
    st(out,(2 as i64*il+1 as i64)*(8 as i64),s);
    il=il+1 as i64;
   }
   ret=1 as i64;
  }else{
   if op==(2 as i64){
    // seq_rm: both caches must succeed
    let rb:i64=get(packet,0,12) as i64;
    let rs:i64=get(packet,0,13) as i64;
    if rb!=(0 as i64)&&rs!=(0 as i64){ ret=1 as i64; }
   }else{
    if op==(3 as i64){
     // memory breakdown: sum by buffer type, ordered like std::map
     let cb:i64=get(packet,0,14) as i64;
     let kb:u64=get(packet,0,12);
     let vb:u64=get(packet,0,13);
     let ks:u64=get(packet,0,11);
     let vs:u64=get(packet,0,10);
     let cs:i64=ret_in;
     // Merge: start from the base map, add the SWA values, then insertion-sort by key.
     var n:i64=cb;
     var i:i64=0;
     while i<cb{
      st(out,(i*(2 as i64))*(8 as i64),at(kb,i));
      st(out,(i*(2 as i64)+1 as i64)*(8 as i64),at(vb,i));
      i=i+1 as i64;
     }
     var j:i64=0;
     while j<cs{
      let key:i64=at(ks,j);
      let val:i64=at(vs,j);
      var found:i64=(0 as i64)-1 as i64;
      var q:i64=0;
      while q<n{
       if ld(out,(q*(2 as i64))*(8 as i64))==key{ found=q; }
       q=q+1 as i64;
      }
      if found>=0 as i64{
       let old:i64=ld(out,(found*(2 as i64)+1 as i64)*(8 as i64));
       st(out,(found*(2 as i64)+1 as i64)*(8 as i64),old+val);
      }else{
       st(out,(n*(2 as i64))*(8 as i64),key);
       st(out,(n*(2 as i64)+1 as i64)*(8 as i64),val);
       n=n+1 as i64;
      }
      j=j+1 as i64;
     }
     // insertion sort by key
     i=1;
     while i<n{
      let kk:i64=ld(out,(i*(2 as i64))*(8 as i64));
      let vv:i64=ld(out,(i*(2 as i64)+1 as i64)*(8 as i64));
      var m:i64=i-(1 as i64);
      var stop:bool=false;
      while m>=0 as i64&&!stop{
       let mk:i64=ld(out,(m*(2 as i64))*(8 as i64));
       if mk<=kk{ stop=true; }else{
        st(out,((m+(1 as i64))*(2 as i64))*(8 as i64),mk);
        st(out,((m+(1 as i64))*(2 as i64)+1 as i64)*(8 as i64),ld(out,(m*(2 as i64)+1 as i64)*(8 as i64)));
        m=m-(1 as i64);
       }
      }
      st(out,((m+(1 as i64))*(2 as i64))*(8 as i64),kk);
      st(out,((m+(1 as i64))*(2 as i64)+1 as i64)*(8 as i64),vv);
      i=i+1 as i64;
     }
     ret=n;
    }else{
     ensure(false,62);
    }
   }
  }
 }
 put(packet,3,0,1 as u64);
 put(packet,3,1,ret as u64);
 return true;
}

fn main(){run(0);}
