// Apex KV sequence division and position extremes in Goose (P5-01).
//
// seq_div: a mirrored cache, d == 1, an empty range and per-cell mismatches all skip. A divided cell
// keeps its sequence bits — unlike a shift by a negative amount it can never go negative — and its
// shift grows by exactly the amount the position lost, which is what later rewrites the cached
// rotations. The per-sequence position index is unlinked before the division and relinked after.
//
// seq_pos_min / seq_pos_max: a mirrored cache delegates to the cache it mirrors, and that result is
// taken as given. Otherwise the extremes are over cells holding the sequence, empty cells skipped.
// An empty sequence yields -1 for both, which is the same sentinel as an empty cell, so "no
// positions" and "position zero" must not be confused by the caller.
//
// Packet, int64 slots:
//   k=0  0 op, 1 other, 2 n, 3 seq_id, 4 p0, 5 p1, 6 d, 7 pos, 8 seqbits, 9 other_min, 10 other_max
//   k=1  0 out
//   k=2  0 count ptr
//   k=3  0 done, 1 ret
//
// Records, three slots: 0 index unlinked, 1 position divided (a = index, b = new position),
// 2 index relinked, 3 shift accumulated (a = index, b = added amount).
extern "fg328k_get" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg328k_put" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg328k_ld_i64" fn ld(base:u64,byte:i64)->i64;
extern "fg328k_stw" fn stw(base:u64,byte:i64,v:i64);
extern "fg328k_fail" fn fail(code:i64);

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

fn posmin()->i64{ return (1 as i64)<<62 as i64; }
fn posmax()->i64{ return (0 as i64)-((1 as i64)<<62 as i64); }

fn run(packet:i64)->bool{
 let op:i64=get(packet,0,0) as i64;
 let other:i64=get(packet,0,1) as i64;
 let n:i64=get(packet,0,2) as i64;
 let seq_id:i64=get(packet,0,3) as i64;
 var p0:i64=get(packet,0,4) as i64;
 var p1:i64=get(packet,0,5) as i64;
 let d:i64=get(packet,0,6) as i64;
 let pos:u64=get(packet,0,7);
 let seqbits:u64=get(packet,0,8);
 let other_min:i64=get(packet,0,9) as i64;
 let other_max:i64=get(packet,0,10) as i64;
 let out:u64=get(packet,1,0);
 let cntp:u64=get(packet,2,0);
 ensure(n>0 as i64,60);
 ensure(pos!=(0 as u64)&&seqbits!=(0 as u64),61);
 var ret:i64=0;

 if op==(0 as i64){
  ensure(d!=(0 as i64),62);
  if other!=(0 as i64){ put(packet,3,0,1 as u64); put(packet,3,1,0 as i64 as u64); return true; }
  if d==(1 as i64){ put(packet,3,0,1 as u64); put(packet,3,1,0 as i64 as u64); return true; }
  if p0<(0 as i64){ p0=0 as i64; }
  if p1<(0 as i64){ p1=posmin(); }
  if p0==p1{ put(packet,3,0,1 as u64); put(packet,3,1,0 as i64 as u64); return true; }
  var count:i64=0;
  var i:i64=0;
  while i<n{
   let p:i64=at(pos,i);
   var inside:bool=false;
   if p>=p0&&p<p1{ inside=true; }
   var has:bool=false;
   if ((at(seqbits,i)&(1 as i64))!=(0 as i64)){ has=true; }
   if inside&&has{
    emit(out,cntp,0 as i64,i,0 as i64);
    let np:i64=p/d;
    emit(out,cntp,1 as i64,i,np);
    emit(out,cntp,3 as i64,i,p-np);
    emit(out,cntp,2 as i64,i,0 as i64);
    count=count+1 as i64;
   }
   i=i+1 as i64;
  }
  ret=count;
 }else{
  if op==(1 as i64){
   // seq_pos_min
   if other!=(0 as i64){ ret=other_min; }else{
    var m:i64=posmin();
    var found:i64=0;
    var i:i64=0;
    while i<n{
     let p:i64=at(pos,i);
     if p!=(0 as i64)-1 as i64{
      if ((at(seqbits,i)&(1 as i64))!=(0 as i64)){
       if p<m{ m=p; }
       found=1 as i64;
      }
     }
     i=i+1 as i64;
    }
    if found==(0 as i64){ m=(0 as i64)-1 as i64; }
    ret=m;
   }
  }else{
   if op==(2 as i64){
    // seq_pos_max
    if other!=(0 as i64){ ret=other_max; }else{
     var m:i64=posmax();
     var found:i64=0;
     var i:i64=0;
     while i<n{
      let p:i64=at(pos,i);
      if p!=(0 as i64)-1 as i64{
       if ((at(seqbits,i)&(1 as i64))!=(0 as i64)){
        if p>m{ m=p; }
        found=1 as i64;
       }
      }
      i=i+1 as i64;
     }
     if found==(0 as i64){ m=(0 as i64)-1 as i64; }
     ret=m;
    }
   }else{
    ensure(false,63);
   }
  }
 }
 put(packet,3,0,1 as u64);
 put(packet,3,1,ret as u64);
 return true;
}

fn main(){run(0);}
