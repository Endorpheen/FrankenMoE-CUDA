// Apex KV state-read header decisions in Goose (llama_kv_cache::state_read_data, working line).
//
// The read is a fixed sequence of decisions before any transfer:
//   1 layer count must match            2 the section must fit the cache
//   3 a transposed state is rejected    then per layer: 4 key type, 5 key row size,
//   6 PLE row size, 7 value type, 8 value row size -- each only for tensors that exist.
//
// The payload transfer itself (io.read_tensor) stays native. Output: the first rejection code, or 0,
// plus per-layer read masks for keys, PLE rows and values, so the caller performs exactly the reads
// the native code would.
//
// Packet:
//   k=0  0 n_layer_ref, 1 n_layer, 2 cell_count, 3 size, 4 s_trans, 5 r_present, 6 p_present,
//        7 s_present, 8 r_type_ok, 9 r_row_ok, 10 p_row_ok, 11 s_type_ok, 12 s_row_ok
//   k=1  0 out, 1 cnt
//   k=2  0 done, 1 ret (0 ok, negative rejection code)
//
// Records: 0 mask (a = 0 keys / 1 ple / 2 values, b = per-layer bitmap)
extern "fg365k_get" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg365k_put" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg365k_ld_i64" fn ld(base:u64,byte:i64)->i64;
extern "fg365k_stw" fn stw(base:u64,byte:i64,v:i64);
extern "fg365k_fail" fn fail(code:i64);

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

fn done(packet:i64,cntp:u64,code:i64){
 put(packet,2,0,1 as u64);
 put(packet,2,1,((0 as i64)-code as i64) as u64);
}

fn run(packet:i64)->bool{
 let nl_ref:i64=get(packet,0,0) as i64;
 let nl:i64=get(packet,0,1) as i64;
 let cc:i64=get(packet,0,2) as i64;
 let size:i64=get(packet,0,3) as i64;
 let s_trans:i64=get(packet,0,4) as i64;
 let rp:u64=get(packet,0,5);
 let pp:u64=get(packet,0,6);
 let sp:u64=get(packet,0,7);
 let rt:i64=get(packet,0,8) as i64;
 let rr:i64=get(packet,0,9) as i64;
 let pr:i64=get(packet,0,10) as i64;
 let st:i64=get(packet,0,11) as i64;
 let sr:i64=get(packet,0,12) as i64;
 let out:u64=get(packet,1,0);
 let cntp:u64=get(packet,1,1);
 ensure(nl>=(0 as i64)&&cc>=(0 as i64)&&size>(0 as i64),60);

 if nl_ref!=nl{ done(packet,cntp,1 as i64); return true; }
 if cc>size{ done(packet,cntp,2 as i64); return true; }
 if s_trans!=(0 as i64){ done(packet,cntp,3 as i64); return true; }

 var k_mask:i64=0 as i64;
 var p_mask:i64=0 as i64;
 var v_mask:i64=0 as i64;
 var il:i64=0;
 while il<nl{
  if at(rp,il)!=(0 as i64){
   if rt==(0 as i64){ done(packet,cntp,4 as i64); return true; }
   if rr==(0 as i64){ done(packet,cntp,5 as i64); return true; }
   if cc>(0 as i64){ k_mask=k_mask|((1 as i64)<<il); }
   if at(pp,il)!=(0 as i64){
    if pr==(0 as i64){ done(packet,cntp,6 as i64); return true; }
    if cc>(0 as i64){ p_mask=p_mask|((1 as i64)<<il); }
   }
  }
  il=il+1 as i64;
 }
 il=0;
 while il<nl{
  if at(sp,il)!=(0 as i64){
   if st==(0 as i64){ done(packet,cntp,7 as i64); return true; }
   if sr==(0 as i64){ done(packet,cntp,8 as i64); return true; }
   if cc>(0 as i64){ v_mask=v_mask|((1 as i64)<<il); }
  }
  il=il+1 as i64;
 }
 emit(out,cntp,0 as i64,0 as i64,k_mask);
 emit(out,cntp,0 as i64,1 as i64,p_mask);
 emit(out,cntp,0 as i64,2 as i64,v_mask);
 put(packet,2,0,1 as u64);
 put(packet,2,1,0 as i64 as u64);
 return true;
}

fn main(){run(0);}
