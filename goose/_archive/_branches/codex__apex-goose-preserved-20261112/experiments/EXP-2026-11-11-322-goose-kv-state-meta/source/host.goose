// Apex KV state metadata in Goose (llama_kv_cache::state_write_meta, P5-08).
//
// For every cell in the selected ranges the metadata stream carries: the position, the number of
// sequence ids it is written with, the cell extension when the cache has one, and those ids in
// increasing order. A named sequence writes only itself when the cell carries it; an unnamed write
// carries every sequence the cell holds. The order of the ids matters — the stream is read back by
// position and the ids are re-inserted in the order they appear.
//
// Record layout in the output, one cell after another:
//   pos | n_seq_id | [ext_x, ext_y, ext_tok when has_ext] | ids...
//
// Packet, int64 slots:
//   k=0  0 n_seq_max, 1 seq_id, 2 has_ext, 3 cells_pos, 4 cells_seqw, 5 W, 6 cells_ext, 7 ranges,
//        8 n_ranges
//   k=1  0 out
//   k=3  0 done, 1 ret (cells written)
extern "fg322k_get" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg322k_put" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg322k_ld_i64" fn ld(base:u64,byte:i64)->i64;
extern "fg322k_st_i64" fn st(base:u64,byte:i64,v:i64);
extern "fg322k_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn at(arr:u64,i:i64)->i64{return ld(arr,i*(8 as i64));}
fn setat(arr:u64,i:i64,v:i64){st(arr,i*(8 as i64),v);}

fn bit_of(s:i64)->i64{
 var b:i64=1 as i64;
 var k:i64=0;
 while k<s%64 as i64{ b=b*2 as i64; k=k+1 as i64; }
 b
}

fn run(packet:i64)->bool{
 let n_seq_max:i64=get(packet,0,0) as i64;
 let seq_id:i64=get(packet,0,1) as i64;
 let has_ext:i64=get(packet,0,2) as i64;
 let cpos:u64=get(packet,0,3);
 let cseqw:u64=get(packet,0,4);
 let W:i64=get(packet,0,5) as i64;
 let cext:u64=get(packet,0,6);
 let ranges:u64=get(packet,0,7);
 let n_ranges:i64=get(packet,0,8) as i64;
 let out:u64=get(packet,1,0);
 ensure(n_seq_max>=(0 as i64)&&W>(0 as i64)&&n_ranges>=(0 as i64),60);

 var w:i64=0;
 var cells:i64=0;
 var r:i64=0;
 while r<n_ranges{
  let lo:i64=at(ranges,(2 as i64*r));
  let hi:i64=at(ranges,(2 as i64*r+1 as i64));
  var i:i64=lo;
  while i<hi{
   // ids in increasing order, filtered by the requested sequence
   var ids:i64=0;
   var ids_start:i64=w+(2 as i64)+(has_ext*3 as i64);
   var cur:i64=0;
   while cur<n_seq_max{
    var take:bool=false;
    if cur==seq_id{ take=true; }
    if seq_id==(0 as i64)-1 as i64{ take=true; }
    if take{
     let wi:i64=cur/64 as i64;
     if (at(cseqw,i*W+wi)&bit_of(cur))!=(0 as i64){
      setat(out,ids_start+ids,cur);
      ids=ids+1 as i64;
     }
    }
    cur=cur+1 as i64;
   }
   setat(out,w,at(cpos,i));
   setat(out,w+(1 as i64),ids);
   if has_ext!=(0 as i64){
    setat(out,w+(2 as i64),at(cext,i*(3 as i64)));
    setat(out,w+(3 as i64),at(cext,i*(3 as i64)+(1 as i64)));
    setat(out,w+(4 as i64),at(cext,i*(3 as i64)+(2 as i64)));
   }
   w=ids_start+ids;
   cells=cells+1 as i64;
   i=i+1 as i64;
  }
  r=r+1 as i64;
 }
 put(packet,3,0,1 as u64);
 put(packet,3,1,cells as u64);
 return true;
}

fn main(){run(0);}
