// Apex recurrent state write and read in Goose
// (llama_memory_recurrent::{state_write, state_read}).
//
// A recurrent cell is selected when it is non-empty (whole-cache request) or holds the sequence.
// Unlike the attention cache there is no SWA filter here and no per-stream split: one count, one
// meta section, one data section.
//
// The write keeps **two** range lists:
//
//   logical ranges over cell indices, coalesced the same way as elsewhere;
//   data ranges over *source rows*, where a row is `rs_idx[seq] * size + (src >= 0 ? src : i)` — the
//   rollback index shifts the row block, and a cell that remembers a previous source writes from
//   there. Two selected cells that are adjacent in cell order but not in row order (because a source
//   moved) therefore stay two ranges.
//
// A whole-cache write with rollback indices must resolve one index per cell by looking at **all**
// sequences in the cell; if they disagree the state cannot be written and native aborts. Saving to
// device memory also refuses more than one logical range.
//
// The read is a transaction: the section count, then meta (the head is only meaningful if meta
// succeeded), then data; a failure clears the sequence with the head and, crucially, with the cell
// count only when meta actually read one — otherwise zero. Afterwards a rollback index is reset when
// the cache uses them.
//
// Packet:
//   k=0  0 op, 1 seq_id, 2 size, 3 n_cells, 4 pos, 5 mask, 6 src, 7 n_rs_seq, 8 rs_idx, 9 on_device,
//        10 meta_ok, 11 data_throw, 12 head, 13 read_count
//   k=1  0 out, 1 cnt
//   k=2  0 done, 1 ret
//
// Records: 0 cell count (a = value), 1 logical range (a = from, b = to), 2 data range (a = cell_id,
// b = cell_id + 1), 3 meta written, 4 data written, 5 device refused (a = range count), 6 sequence
// cleared (a = head, b = count), 7 rollback index reset (a = seq).
extern "fg346k_get" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg346k_put" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg346k_ld_i64" fn ld(base:u64,byte:i64)->i64;
extern "fg346k_stw" fn stw(base:u64,byte:i64,v:i64);
extern "fg346k_fail" fn fail(code:i64);

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
 let op:i64=get(packet,0,0) as i64;
 let seq_id:i64=get(packet,0,1) as i64;
 let size:i64=get(packet,0,2) as i64;
 let n_cells:i64=get(packet,0,3) as i64;
 let pos:u64=get(packet,0,4);
 let mask:u64=get(packet,0,5);
 let src:u64=get(packet,0,6);
 let n_rs_seq:i64=get(packet,0,7) as i64;
 let rs_idx:u64=get(packet,0,8);
 let on_device:i64=get(packet,0,9) as i64;
 let meta_ok:i64=get(packet,0,10) as i64;
 let data_throw:i64=get(packet,0,11) as i64;
 let head:i64=get(packet,0,12) as i64;
 let read_count:i64=get(packet,0,13) as i64;
 let out:u64=get(packet,1,0);
 let cntp:u64=get(packet,1,1);
 ensure(size>=(0 as i64)&&n_cells>=(0 as i64),60);

 if op==(0 as i64){
  // a shared cell must resolve one rollback index; native aborts inside the collection loop
  if n_rs_seq!=(0 as i64){
   if seq_id==(0 as i64)-1 as i64{
    var ci:i64=0;
    while ci<n_cells{
     let cm:i64=at(mask,ci);
     let cp:i64=at(pos,ci);
     if cp!=(0 as i64)-1 as i64{
      var first:i64=1 as i64;
      var cur:i64=0 as i64;
      var cq:i64=0;
      while cq<64 as i64{
       if (cm&((1 as i64)<<cq))!=(0 as i64){
        let cv:i64=at(rs_idx,cq);
        if first!=(0 as i64){ cur=cv; first=0 as i64; }else{
         if cur!=cv{ done(packet,cntp,1 as i64); return true; }
        }
       }
       cq=cq+1 as i64;
      }
     }
     ci=ci+1 as i64;
    }
   }
  }

  // device saves refuse more than one logical range -- checked before anything is written
  if on_device!=(0 as i64){
   var n_ranges:i64=0;
   var q2:i64=0;
   var l2:i64=(0 as i64)-1 as i64;
   var j:i64=0;
   while j<n_cells{
    let p2:i64=at(pos,j);
    let m2:i64=at(mask,j);
    var sel2:i64=0 as i64;
    if seq_id==(0 as i64)-1 as i64{
     if p2!=(0 as i64)-1 as i64{ sel2=1 as i64; }
    }else{
     if (m2&((1 as i64)<<seq_id))!=(0 as i64){ sel2=1 as i64; }
    }
    if sel2!=(0 as i64){
     if l2<(0 as i64){ l2=j; }
    }else{
     if l2>=(0 as i64){ n_ranges=n_ranges+1 as i64; l2=(0 as i64)-1 as i64; }
    }
    j=j+1 as i64;
   }
   if l2>=(0 as i64){ n_ranges=n_ranges+1 as i64; }
   if n_ranges>(1 as i64){
    emit(out,cntp,5 as i64,n_ranges,0 as i64);
    done(packet,cntp,2 as i64);
    return true;
   }
  }


  var count:i64=0;
  var lbegin:i64=(0 as i64)-1 as i64;
  var d_begin:i64=0 as i64;
  var d_end:i64=0 as i64;
  var d_open:i64=0 as i64;
  var i:i64=0;
  while i<n_cells{
   let p:i64=at(pos,i);
   let m:i64=at(mask,i);
   var sel:i64=0 as i64;
   if seq_id==(0 as i64)-1 as i64{
    if p!=(0 as i64)-1 as i64{ sel=1 as i64; }
   }else{
    if (m&((1 as i64)<<seq_id))!=(0 as i64){ sel=1 as i64; }
   }
   if sel!=(0 as i64){
    count=count+1 as i64;
    var rsc:i64=0 as i64;
    if n_rs_seq!=(0 as i64){
     if seq_id!=(0 as i64)-1 as i64{
      rsc=at(rs_idx,seq_id);
     }else{
      // all sequences in the cell must agree on their rollback index
      var first:i64=1 as i64;
      var q:i64=0;
      while q<64 as i64{
       if (m&((1 as i64)<<q))!=(0 as i64){
        let v:i64=at(rs_idx,q);
        if first!=(0 as i64){ rsc=v; first=0 as i64; }else{
         if rsc!=v{ done(packet,cntp,1 as i64); return true; }
        }
       }
       q=q+1 as i64;
      }
     }
    }
    let sc:i64=at(src,i);
    var row:i64=i;
    if sc>=(0 as i64){ row=sc; }
    let cell_id:i64=rsc*size+row;
    if lbegin<(0 as i64){ lbegin=i; }
   }else{
    if lbegin>=(0 as i64){
     emit(out,cntp,1 as i64,lbegin,i);
     lbegin=(0 as i64)-1 as i64;
    }
   }
   i=i+1 as i64;
  }
  if lbegin>=(0 as i64){ emit(out,cntp,1 as i64,lbegin,n_cells); }

  // data ranges, in the order the logical section is written before them
  var di:i64=0;
  while di<n_cells{
   let p3:i64=at(pos,di);
   let m3:i64=at(mask,di);
   var sel3:i64=0 as i64;
   if seq_id==(0 as i64)-1 as i64{
    if p3!=(0 as i64)-1 as i64{ sel3=1 as i64; }
   }else{
    if (m3&((1 as i64)<<seq_id))!=(0 as i64){ sel3=1 as i64; }
   }
   if sel3!=(0 as i64){
    var rsc2:i64=0 as i64;
    if n_rs_seq!=(0 as i64){
     if seq_id!=(0 as i64)-1 as i64{
      rsc2=at(rs_idx,seq_id);
     }else{
      var first2:i64=1 as i64;
      var q3:i64=0;
      while q3<64 as i64{
       if (m3&((1 as i64)<<q3))!=(0 as i64){
        let v2:i64=at(rs_idx,q3);
        if first2!=(0 as i64){ rsc2=v2; first2=0 as i64; }else{
         if rsc2!=v2{ done(packet,cntp,1 as i64); return true; }
        }
       }
       q3=q3+1 as i64;
      }
     }
    }
    let sc2:i64=at(src,di);
    var row2:i64=di;
    if sc2>=(0 as i64){ row2=sc2; }
    let cid2:i64=rsc2*size+row2;
    if d_open==(0 as i64){
     d_begin=cid2;
     d_end=cid2+(1 as i64);
     d_open=1 as i64;
    }else{
     if d_end==cid2{ d_end=cid2+(1 as i64); }else{
      emit(out,cntp,2 as i64,d_begin,d_end);
      d_begin=cid2;
      d_end=cid2+(1 as i64);
     }
    }
   }
   di=di+1 as i64;
  }
  if d_open!=(0 as i64){ emit(out,cntp,2 as i64,d_begin,d_end); }

  emit(out,cntp,0 as i64,count,0 as i64);
  emit(out,cntp,3 as i64,0 as i64,0 as i64);
  emit(out,cntp,4 as i64,0 as i64,0 as i64);
  put(packet,2,0,1 as u64);
  put(packet,2,1,0 as i64 as u64);
  return true;
 }else{
  // state_read
  var ok:i64=meta_ok;
  if ok!=(0 as i64){ emit(out,cntp,3 as i64,0 as i64,0 as i64); }
  emit(out,cntp,4 as i64,0 as i64,0 as i64);
  if data_throw!=(0 as i64){ ok=0 as i64; }
  if ok==(0 as i64){
   var cc2:i64=0 as i64;
   if meta_ok!=(0 as i64){ cc2=read_count; }
   emit(out,cntp,6 as i64,head,cc2);
   done(packet,cntp,3 as i64);
   return true;
  }
  if n_rs_seq!=(0 as i64){ emit(out,cntp,7 as i64,seq_id,0 as i64); }
  put(packet,2,0,1 as u64);
  put(packet,2,1,0 as i64 as u64);
  return true;
 }
}

fn main(){run(0);}
