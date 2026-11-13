// Apex recurrent accessors in Goose (llama_memory_recurrent, remaining members).
//
// These are small but each has a rule that is easy to get wrong by writing the "obvious" version.
//
//   seq_pos_min  starts at the largest position and falls back to -1 when nothing matched; the
//                attention cache builds the same answer from an ordered index, this one scans cells.
//   seq_pos_max  starts at -1 and never falls back — an empty sequence and a sequence at position
//                -1 are indistinguishable here, which is native behaviour.
//   set_rs_idx   a negative id zeroes the whole array; otherwise both bounds are preconditions, and
//                the index may equal n_rs_seq (it is an inclusive bound).
//   can_shift    always true for the recurrent cache; nothing to consult.
//   is_empty     the buffer list decides, and the assert says it must agree with a zero total.
//   breakdown    sums buffer sizes per backend type; the same type can appear more than once.
//   sizes        sum `nbytes` over non-null tensors only, per family (r, s, p), independently.
//
// Packet:
//   k=0  0 op, 1 out, 2 n_cells, 3 cpos, 4 cmask, 5 seq_id, 6 n_seq_max, 7 n_rs_seq, 8 rs_idx,
//        9 idx, 10 keys, 11 sizes, 12 n_buf, 13 r_nbytes, 14 s_nbytes, 15 p_nbytes
//   k=1  0 cnt, 1 r_present, 2 s_present, 3 p_present
//   k=2  0 done, 1 ret
//
// Records, three slots: 0 extrema/breakdown answer (a = key or position, b = value),
// 1 rs_idx written (a = seq, b = value), 2 sizes (a = bytes).
extern "fg340k_get" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg340k_put" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg340k_ld_i64" fn ld(base:u64,byte:i64)->i64;
extern "fg340k_stw" fn stw(base:u64,byte:i64,v:i64);
extern "fg340k_fail" fn fail(code:i64);

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
 let n_cells:i64=get(packet,0,2) as i64;
 let cpos:u64=get(packet,0,3);
 let cmask:u64=get(packet,0,4);
 let seq_id:i64=get(packet,0,5) as i64;
 let n_seq_max:i64=get(packet,0,6) as i64;
 let n_rs_seq:i64=get(packet,0,7) as i64;
 let rs_idx:u64=get(packet,0,8);
 let idx:i64=get(packet,0,9) as i64;
 let keys:u64=get(packet,0,10);
 let sizes:u64=get(packet,0,11);
 let n_buf:i64=get(packet,0,12) as i64;
 let r_nb:u64=get(packet,0,13);
 let s_nb:u64=get(packet,0,14);
 let p_nb:u64=get(packet,0,15);
 let cntp:u64=get(packet,1,0);
 let r_present:u64=get(packet,1,1);
 let s_present:u64=get(packet,1,2);
 let p_present:u64=get(packet,1,3);
 var ret:i64=0;

 if op==(0 as i64){
  // seq_pos_min
  var mn:i64=(1 as i64)<<62 as i64;
  var i:i64=0;
  while i<n_cells{
   if (at(cmask,i)&((1 as i64)<<seq_id))!=(0 as i64){
    if at(cpos,i)<mn{ mn=at(cpos,i); }
   }
   i=i+1 as i64;
  }
  if mn==((1 as i64)<<62 as i64){ mn=(0 as i64)-1 as i64; }
  emit(out,cntp,0 as i64,0 as i64,mn);
  ret=mn;
 }else{
  if op==(1 as i64){
   // seq_pos_max: starts at -1 and never falls back
   var mx:i64=(0 as i64)-1 as i64;
   var i2:i64=0;
   while i2<n_cells{
    if (at(cmask,i2)&((1 as i64)<<seq_id))!=(0 as i64){
     if at(cpos,i2)>mx{ mx=at(cpos,i2); }
    }
    i2=i2+1 as i64;
   }
   emit(out,cntp,0 as i64,0 as i64,mx);
   ret=mx;
  }else{
   if op==(2 as i64){
    // set_rs_idx
    if seq_id<(0 as i64){
     var q:i64=0;
     while q<n_seq_max{
      setat(rs_idx,q,0 as i64);
      emit(out,cntp,1 as i64,q,0 as i64);
      q=q+1 as i64;
     }
     ret=0 as i64;
    }else{
     ensure(seq_id<n_seq_max,60);
     ensure(idx<=n_rs_seq,61);          // inclusive bound
     setat(rs_idx,seq_id,idx);
     emit(out,cntp,1 as i64,seq_id,idx);
     ret=idx;
    }
   }else{
    if op==(3 as i64){
     // memory_breakdown: sum per backend type
     var i3:i64=0;
     while i3<n_buf{
      let key:i64=at(keys,i3);
      var total:i64=at(sizes,i3);
      var j:i64=0;
      while j<n_buf{
       if j!=i3{
        if at(keys,j)==key{ total=total+at(sizes,j); }
       }
       j=j+1 as i64;
      }
      // report each key once, at its first occurrence
      var first:i64=1 as i64;
      var q2:i64=0;
      while q2<i3{
       if at(keys,q2)==key{ first=0 as i64; }
       q2=q2+1 as i64;
      }
      if first!=(0 as i64){ emit(out,cntp,0 as i64,key,total); }
      i3=i3+1 as i64;
     }
     ret=0 as i64;
    }else{
     if op==(4 as i64){
      // tensor family sizes
      var r:i64=0;
      var s:i64=0;
      var p:i64=0;
      var i4:i64=0;
      while i4<n_cells{
       if at(r_present,i4)!=(0 as i64){ r=r+at(r_nb,i4); }
       if at(s_present,i4)!=(0 as i64){ s=s+at(s_nb,i4); }
       if at(p_present,i4)!=(0 as i64){ p=p+at(p_nb,i4); }
       i4=i4+1 as i64;
      }
      emit(out,cntp,2 as i64,r,0 as i64);
      emit(out,cntp,2 as i64,s,0 as i64);
      emit(out,cntp,2 as i64,p,0 as i64);
      ret=r+s+p;
     }else{
      if op==(5 as i64){
       // can_shift: always true
       ret=1 as i64;
      }else{
       if op==(6 as i64){
        // is_empty: the buffer list decides; a zero total must agree
        var total2:i64=0;
        var i5:i64=0;
        while i5<n_buf{
         total2=total2+at(sizes,i5);
         i5=i5+1 as i64;
        }
        var empty:i64=0 as i64;
        if n_buf==(0 as i64){ empty=1 as i64; }
        ensure(empty==(0 as i64)||total2==(0 as i64),62);
        ret=empty;
       }else{
        ensure(false,63);
       }
      }
     }
    }
   }
  }
 }
 put(packet,2,0,1 as u64);
 put(packet,2,1,ret as u64);
 return true;
}

fn main(){run(0);}
