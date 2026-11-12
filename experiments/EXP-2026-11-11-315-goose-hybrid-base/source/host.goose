// Apex hybrid memory (llama_memory_hybrid): layer filters and the prepare order.
//
// The two caches split the layers by the model's recurrent pattern unless an explicit filter is
// given: attention takes the non-recurrent layers, the recurrent cache takes the recurrent ones.
//
// Splitting follows the recurrent pattern: a batch of all-output tokens splits by sequence, and
// otherwise by the equal split, where a non-unified attention cache needs the non-sequential form
// and a pending rollback window must keep its trailing tokens in one ubatch.
//
// Preparing runs the recurrent cache first: it can refuse, and the attention preparation is the more
// expensive one, so a refusal is discovered before it is paid for.
//
// Packet, int64 slots:
//   k=0  0 op, 1 out, 2 a, 3 b, 4 c, 5 d, 6 n, 7 arr
//   k=3  0 done, 1 ret
extern "fg315k_get" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg315k_put" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg315k_ld_i64" fn ld(base:u64,byte:i64)->i64;
extern "fg315k_st_i64" fn st(base:u64,byte:i64,v:i64);
extern "fg315k_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn at(arr:u64,i:i64)->i64{return ld(arr,i*(8 as i64));}
fn setat(arr:u64,i:i64,v:i64){st(arr,i*(8 as i64),v);}

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
 let arr3:u64=get(packet,0,9);
 var ret:i64=0;

 if op==(0 as i64){
  // layer filters: a = is_recr per layer, b = external filter per layer, c = external present
  var il:i64=0;
  while il<n{
   let recr:i64=at(arr,il);
   var fa:i64=0;
   var fr:i64=0;
   // Each cache has its own external filter; without one, the recurrent pattern decides.
   if c!=(0 as i64){ fa=at(arr2,il); }else{ if recr==(0 as i64){ fa=1 as i64; } }
   if d!=(0 as i64){ fr=at(arr3,il); }else{ if recr!=(0 as i64){ fr=1 as i64; } }
   setat(out,(2 as i64*il),fa);
   setat(out,(2 as i64*il+1 as i64),fr);
   il=il+1 as i64;
  }
  ret=1 as i64;
 }else{
  if op==(1 as i64){
   // split choice: a = embd_all, b = n_stream_attn, c = n_rs_seq
   if a!=(0 as i64){
    ret=0 as i64;                    // split_seq
   }else{
    let unified:bool=b==(1 as i64);
    var keep:i64=0;
    if c>(0 as i64){ keep=c+(1 as i64); }
    // 1 = split_equal, non-seq when the attention cache is not unified
    ret=1 as i64;
    setat(out,0 as i64,1 as i64);
    if !unified{ setat(out,0 as i64,2 as i64); }   // non-sequential
    setat(out,1 as i64,keep);
   }
  }else{
   if op==(2 as i64){
    // prepare order: a = recurrent prepared ok, b = attention heads empty
    var ne:i64=0;
    if a==(0 as i64){
     setat(out,ne,0 as i64); ne=ne+1 as i64;      // recurrent prepared and refused
     ret=0 as i64;                                // FAILED_PREPARE
    }else{
     setat(out,ne,0 as i64); ne=ne+1 as i64;      // recurrent prepared
     setat(out,ne,1 as i64); ne=ne+1 as i64;      // attention prepared
     if b!=(0 as i64){
      ret=0 as i64;                               // FAILED_PREPARE on empty heads
     }else{
      ret=1 as i64;
     }
    }
   }else{
    if op==(3 as i64){
     // attention stream count
     if a!=(0 as i64){ ret=1 as i64; }else{ ret=n; }
    }else{
     ensure(false,60);
    }
   }
  }
 }
 put(packet,3,0,1 as u64);
 put(packet,3,1,ret as u64);
 return true;
}

fn main(){run(0);}
