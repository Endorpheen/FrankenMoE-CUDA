// Apex KV state metadata restore in Goose (llama_kv_cache::state_read_meta, P5-08).
//
// Single sequence (`dest_seq_id != -1`): the section must fit the cache, the destination sequence is
// removed first, every record must carry exactly one id (the id is read and discarded in favour of the
// destination), and the cell layout either mirrors another cache — one stream, the right cell count,
// and every one of its cells free — or is searched for. The saved extension is written back after the
// batch is applied, because the batch only carries the token and the 2-D position.
//
// Whole cache: cells go in from zero, the mirror check is only about the count, and every id read must
// be inside the sequence range.
//
// Rejection codes, native order:
//   1 cell count exceeds the cache
//   2 a single-sequence record carries more than one id
//   3 the mirrored layout does not match
//   4 a mirrored cell is not free
//   5 no cells could be found
//   6 an id is out of range (whole-cache branch)
//
// Operation records, three slots: kind, a, b.
//   0 seq_rm(dest)   1 record(pos, ids)   2 mirror adopted   3 layout searched
//   4 batch applied  5 ext restored(cell) 6 cell written(i,pos) 7 head reset
//
// Packet, int64 slots:
//   k=0  0 dest_seq_id, 1 cell_count, 2 size, 3 has_ext, 4 n_seq_max, 5 sinfo_in_present,
//        6 sinfo_in_empty, 7 sinfo_in_streams, 8 sinfo_in_cells, 9 find_slot_ok, 10 idxs,
//        11 idxs_free, 12 rec_pos, 13 rec_nseq, 14 rec_ids, 15 n_records
//   k=1  0 out
//   k=2  0 count ptr
//   k=3  0 done, 1 ret (0 = ok, negative = rejection code)
extern "fg324k_get" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg324k_put" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg324k_ld_i64" fn ld(base:u64,byte:i64)->i64;
extern "fg324k_stw" fn stw(base:u64,byte:i64,v:i64);
extern "fg324k_fail" fn fail(code:i64);

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

fn done(packet:i64,code:i64){
 put(packet,3,0,1 as u64);
 put(packet,3,1,((0 as i64)-code as i64) as u64);
}

fn run(packet:i64)->bool{
 let dest:i64=get(packet,0,0) as i64;
 let cell_count:i64=get(packet,0,1) as i64;
 let size:i64=get(packet,0,2) as i64;
 let has_ext:i64=get(packet,0,3) as i64;
 let n_seq_max:i64=get(packet,0,4) as i64;
 let sin_present:i64=get(packet,0,5) as i64;
 let sin_empty:i64=get(packet,0,6) as i64;
 let sin_streams:i64=get(packet,0,7) as i64;
 let sin_cells:i64=get(packet,0,8) as i64;
 let find_ok:i64=get(packet,0,9) as i64;
 let idxs:u64=get(packet,0,10);
 let rec_pos:u64=get(packet,0,12);
 let rec_nseq:u64=get(packet,0,13);
 let rec_ids:u64=get(packet,0,14);
 let n_records:i64=get(packet,0,15) as i64;
 let out:u64=get(packet,1,0);
 let cntp:u64=get(packet,2,0);
 ensure(cell_count>=(0 as i64)&&n_records>=(0 as i64),60);
 ensure(cell_count==n_records,61);

 if cell_count>size{ done(packet,1 as i64); return true; }

 if dest!=(0 as i64)-1 as i64{
  emit(out,cntp,0 as i64,0 as i64,0 as i64);
  var i:i64=0;
  while i<cell_count{
   let nseq:i64=at(rec_nseq,i);
   if nseq!=(1 as i64){ done(packet,2 as i64); return true; }
   emit(out,cntp,1 as i64,at(rec_pos,i),nseq);
   i=i+1 as i64;
  }
  if sin_present!=(0 as i64){
   var bad:bool=false;
   if sin_empty!=(0 as i64){ bad=true; }
   if sin_streams!=(1 as i64){ bad=true; }
   if sin_cells!=cell_count{ bad=true; }
   if bad{ done(packet,3 as i64); return true; }
   var q:i64=0;
   while q<cell_count{
    let idx:i64=at(idxs,q);
    if idx>=size{ done(packet,4 as i64); return true; }
    q=q+1 as i64;
   }
   emit(out,cntp,2 as i64,0 as i64,0 as i64);
  }else{
   if find_ok==(0 as i64){ done(packet,5 as i64); return true; }
   emit(out,cntp,3 as i64,0 as i64,0 as i64);
  }
  emit(out,cntp,4 as i64,0 as i64,0 as i64);
  if has_ext!=(0 as i64){
   var e:i64=0;
   while e<cell_count{
    emit(out,cntp,5 as i64,at(idxs,e),0 as i64);
    e=e+1 as i64;
   }
  }
 }else{
  if sin_present!=(0 as i64){
   var bad:bool=false;
   if sin_empty!=(0 as i64){ bad=true; }
   if sin_streams!=(1 as i64){ bad=true; }
   if sin_cells!=cell_count{ bad=true; }
   if bad{ done(packet,3 as i64); return true; }
  }
  var i:i64=0;
  while i<cell_count{
   let nseq:i64=at(rec_nseq,i);
   var j:i64=0;
   while j<nseq{
    let id:i64=at(rec_ids,i*(8 as i64)+j);
    if id<(0 as i64){ done(packet,6 as i64); return true; }
    if id>=n_seq_max{ done(packet,6 as i64); return true; }
    j=j+1 as i64;
   }
   emit(out,cntp,6 as i64,i,at(rec_pos,i));
   emit(out,cntp,1 as i64,at(rec_pos,i),nseq);
   i=i+1 as i64;
  }
  emit(out,cntp,7 as i64,0 as i64,0 as i64);
 }
 put(packet,3,0,1 as u64);
 put(packet,3,1,0 as i64 as u64);
 return true;
}

fn main(){run(0);}
