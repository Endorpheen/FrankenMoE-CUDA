// Apex KV cache views in Goose: llama_kv_cache::{get_k, get_v} shapes and strides, and the n_kv
// padding of get_n_kv.
//
// get_k always presents [head_k, n_head_kv, n_kv, n_streams] with row strides that walk head ->
// heads -> cache -> streams. get_v depends on the layout: with a plain cache it has the same shape
// as K, and with a transposed V the first two dimensions swap and the row stride follows the cache
// size instead of the head count.
//
// n_kv pads the highest used position up to `max(n_pad, 256)` and clamps it to the cache size, then
// takes the maximum over the streams of the slot info — the pad keeps the graph constant across
// batches.
//
// Row sizes are `type_size * ne`, which is what ggml_row_size computes for the non-quantised types
// this cache uses.
//
// Packet, int64 slots:
//   k=0  0 op, 1 out, 2 a, 3 b, 4 c, 5 d, 6 n, 7 arr, 8 arr2
//   k=3  0 done, 1 ret
extern "fg318k_get" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg318k_put" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg318k_ld_i64" fn ld(base:u64,byte:i64)->i64;
extern "fg318k_st_i64" fn st(base:u64,byte:i64,v:i64);
extern "fg318k_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn at(arr:u64,i:i64)->i64{return ld(arr,i*(8 as i64));}
fn setat(arr:u64,i:i64,v:i64){st(arr,i*(8 as i64),v);}

fn padto(x:i64,align:i64)->i64{
 if align<=(1 as i64){ return x; }
 let q:i64=(x+align-(1 as i64))/align;
 return q*align;
}

fn run(packet:i64)->bool{
 let op:i64=get(packet,0,0) as i64;
 let out:u64=get(packet,0,1);
 let a:i64=get(packet,0,2) as i64;
 let b:i64=get(packet,0,3) as i64;
 let c:i64=get(packet,0,4) as i64;
 let d:i64=get(packet,0,5) as i64;
 let n:i64=get(packet,0,6) as i64;
 let arr:u64=get(packet,0,7);
 let arr2:u64=get(packet,0,8);
 var ret:i64=0;

 if op==(0 as i64){
  // k view: a = head_k, b = n_head_kv, c = n_kv, d = ns, n = kv_size, arr[0] = type size
  let ts:i64=at(arr,0 as i64);
  setat(out,0 as i64,a);              // ne0
  setat(out,1 as i64,b);              // ne1
  setat(out,2 as i64,c);              // ne2
  setat(out,3 as i64,d);              // ne3
  setat(out,4 as i64,ts*a);           // nb1: one head row
  setat(out,5 as i64,ts*a*b);         // nb2: all kv heads of one token
  setat(out,6 as i64,ts*a*b*n);       // nb3: one stream
  let s0k:i64=at(arr,1 as i64);
  setat(out,7 as i64,ts*a*b*n*s0k);   // offset: one stream stride per s0
  ret=1 as i64;
 }else{
  if op==(1 as i64){
   // v view: a = head_v, b = n_head_kv, c = n_kv, d = ns, n = kv_size, arr[0] = ts, arr[1] = s0,
   // arr[2] = v_trans, arr[3] = n_embd_v_gqa
   let ts:i64=at(arr,0 as i64);
   let s0:i64=at(arr,1 as i64);
   let vtrans:i64=at(arr,2 as i64);
   let gqa:i64=at(arr,3 as i64);
   let n_kv_:i64=c;
   setat(out,8 as i64,s0);
   if vtrans==(0 as i64){
    setat(out,0 as i64,a);
    setat(out,1 as i64,b);
    setat(out,2 as i64,n_kv_);
    setat(out,3 as i64,d);
    setat(out,4 as i64,ts*a);
    setat(out,5 as i64,ts*gqa);
    setat(out,6 as i64,ts*gqa*n);
    setat(out,7 as i64,ts*gqa*n*s0);
   }else{
    setat(out,0 as i64,n_kv_);
    setat(out,1 as i64,b);
    setat(out,2 as i64,a);
    setat(out,3 as i64,d);
    setat(out,4 as i64,ts*n_kv_*a);
    setat(out,5 as i64,ts*n_kv_);
    setat(out,6 as i64,ts*n_kv_*gqa);
    setat(out,7 as i64,ts*n_kv_*gqa*s0);
   }
   ret=1 as i64;
  }else{
   if op==(2 as i64){
    // n_kv: a = n_pad, n = streams, arr = cache sizes, arr2 = used_max_p1 per stream
    var n_pad_cur:i64=a;
    if n_pad_cur<(256 as i64){ n_pad_cur=256 as i64; }
    var res:i64=0;
    var s:i64=0;
    while s<n{
     let sz:i64=at(arr,s);
     let used:i64=at(arr2,s);
     var v:i64=n_pad_cur;
     let p:i64=padto(used,n_pad_cur);
     if p>v{ v=p; }
     var m:i64=sz;
     if v<m{ m=v; }
     if m>res{ res=m; }
     s=s+1 as i64;
    }
    ret=res;
   }else{
    ensure(false,60);
   }
  }
 }
 put(packet,3,0,1 as u64);
 put(packet,3,1,ret as u64);
 return true;
}

fn main(){run(0);}
