// Apex KV read-orchestration guards in Goose (llama_kv_cache::state_read_sinfo, opening checks).
//
// Two guards run before any section is read, and two inside the stream loop:
//   1 the mirrored layout must have exactly one entry per stream       (before reading)
//   2 the section's stream count must match                            (after reading the header)
//   3 a zero-cell stream requires an empty mirrored entry
//   4 a failed meta or a throwing data half must clear the sequence
//
// Only the guard decisions live here; the reads, the clear and the throw stay in llama.
//
// Packet:
//   k=0  0 op, 1 a, 2 b, 3 c
//   k=1  0 out
//   k=2  0 done, 1 ret (0 ok, negative rejection code)
//
// Records: 0 guard (a = op, b = code)
extern "fg373k_get" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg373k_put" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg373k_ld_i64" fn ld(base:u64,byte:i64)->i64;
extern "fg373k_stw" fn stw(base:u64,byte:i64,v:i64);
extern "fg373k_fail" fn fail(code:i64);

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
 let a:i64=get(packet,0,1) as i64;
 let b:i64=get(packet,0,2) as i64;
 let c:i64=get(packet,0,3) as i64;
 let out:u64=get(packet,1,0);
 let cntp:u64=get(packet,2,0);
 var ret:i64=0 as i64;

 if op==(0 as i64){
  // mirrored layout size: present, one entry per stream
  if a!=(0 as i64)&&b!=c{ ret=(0 as i64)-1 as i64; }
 }else{
  if op==(1 as i64){
   // section stream count
   if a!=b{ ret=(0 as i64)-2 as i64; }
  }else{
   if op==(2 as i64){
    // zero-cell stream: the mirrored entry must be empty
    if a==(0 as i64)&&b==(0 as i64){ ret=(0 as i64)-3 as i64; }
   }else{
    ensure(false,60);
   }
  }
 }
 emit(out,cntp,0 as i64,op,ret);
 put(packet,2,0,1 as u64);
 put(packet,2,1,ret as u64);
 return true;
}

fn main(){run(0);}
