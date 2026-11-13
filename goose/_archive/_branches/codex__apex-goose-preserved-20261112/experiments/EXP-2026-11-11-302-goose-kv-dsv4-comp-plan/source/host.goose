// Apex DSV4 compressed-attention plan in Goose: dsv4_build_comp_plan, the
// non-overlap subcase with no rollback planes (overlap == false, n_rs_seq == 0).
//
// Transferred: n_visible per token, state_pos, n_kv, the state write/read index lists, the dummy
// block that keeps the CSA graph shape stable when no block completes, the HCA dummy write when
// nothing completes at all, the persist rows (sorted by destination), and the graph stream count.
//
// Not transferred here, stated rather than implied: the overlap index layout (prev/cur halves),
// the restore/snapshot planes (n_rs_seq > 0), and the debug logging.
//
// Arena layout, int64 slots:
//   out   : n_visible[n_tokens] | state_pos[n_tokens] | write_idx[2*n_tokens+8] | write_pos[...]
//           | read_idx[...] | persist_src[...] | persist_dst[...]
//   counts: cnt_state_pos | cnt_write | cnt_read | cnt_persist | n_kv | n_stream_graph
//   scratch: persist rows as (dst,src,pos) triples
//
// Packet, int64 slots:
//   k=0  0 op, 1 out, 2 n_tokens, 3 ratio, 4 state_size, 5 kv_size, 6 n_stream, 7 pos,
//        8 n_seq_id, 9 seq_id, 10 width, 11 n_seqs_unq, 12 seq_id_unq, 13 scratch, 14 counts
//   k=3  0 done, 1 ret
extern "fg302k_get" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg302k_put" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg302k_ld_i64" fn ld(base:u64,byte:i64)->i64;
extern "fg302k_st_i64" fn st(base:u64,byte:i64,v:i64);
extern "fg302k_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn at(arr:u64,i:i64)->i64{return ld(arr,i*(8 as i64));}
fn setat(arr:u64,i:i64,v:i64){st(arr,i*(8 as i64),v);}

fn pad256(x:i64)->i64{
 let q:i64=(x+(255 as i64))/(256 as i64);
 return q*(256 as i64);
}

fn stream_offset(n_stream:i64,seq_id:i64,size:i64)->i64{
 if n_stream <= (1 as i64){ return 0 as i64; }
 if seq_id < (0 as i64){ return (0 as i64)-1 as i64; }
 if seq_id>=n_stream{ return (0 as i64)-1 as i64; }
 return seq_id*size;
}

fn token_has_seq(i:i64,seq_id:i64,width:i64,sid:u64,nsid:u64)->bool{
 let k:i64=at(nsid,i);
 var j:i64=0;
 while j<k{
  if at(sid,i*width+j)==seq_id{ return true; }
  j=j+1 as i64;
 }
 return false;
}

fn has_coupled(n:i64,nsid:u64)->bool{
 var i:i64=0;
 while i<n{
  if at(nsid,i) > 1 as i64{ return true; }
  i=i+1 as i64;
 }
 return false;
}

fn graph_n_stream(n_stream:i64,n_seqs_unq:i64,coupled:bool)->i64{
 if n_stream <= (1 as i64){ return 1 as i64; }
 if n_seqs_unq <= (1 as i64){ return 1 as i64; }
 if coupled{ return 1 as i64; }
 return n_seqs_unq;
}

// Source row for (seq, pos): scratch row for an upcoming token, the zero row for pos < 0, else the
// persistent row.
fn source_idx(pos:i64,seq_id:i64,state_size:i64,
              n_tokens:i64,posarr:u64,width:i64,sid:u64,nsid:u64,n_stream:i64,state_rows:i64)->i64{
 if pos < (0 as i64){ return state_rows+n_tokens; }
 // A later token with the same (sequence, position) wins: the native map keeps the last one.
 var best:i64=(0 as i64)-1 as i64;
 var i:i64=0;
 while i<n_tokens{
  if at(posarr,i)==pos{
   if token_has_seq(i,seq_id,width,sid,nsid){ best=i; }
  }
  i=i+1 as i64;
 }
 if best >= (0 as i64){ return state_rows+best; }
 let off:i64=stream_offset(n_stream,seq_id,state_size);
 return off+pos%state_size;
}

fn run(packet:i64)->bool{
 let out:u64=get(packet,0,1);
 let n_tokens:i64=get(packet,0,2) as i64;
 let ratio:i64=get(packet,0,3) as i64;
 let state_size:i64=get(packet,0,4) as i64;
 let kv_size:i64=get(packet,0,5) as i64;
 let n_stream:i64=get(packet,0,6) as i64;
 let posarr:u64=get(packet,0,7);
 let nsid:u64=get(packet,0,8);
 let sid:u64=get(packet,0,9);
 let width:i64=get(packet,0,10) as i64;
 let n_seqs_unq:i64=get(packet,0,11) as i64;
 let sid_unq:u64=get(packet,0,12);
 let scratch:u64=get(packet,0,13);
 let counts:u64=get(packet,0,14);
 let n_seq_tokens:i64=get(packet,0,15) as i64;
 ensure(n_tokens>(0 as i64)&&ratio>(0 as i64)&&state_size>(0 as i64),60);
 ensure(kv_size>(0 as i64)&&width>(0 as i64),61);
 if n_stream <= (1 as i64){
  if n_seqs_unq > (1 as i64){ fail(62); }
 }

 let state_rows:i64=state_size*n_stream;
 let coupled:bool=has_coupled(n_tokens,nsid);

 // Output arrays, one pointer each: a dense dummy block (ratio up to 128) does not fit a
 // per-token section, and overlapping sections silently corrupt each other.
 let a_vis:u64=get(packet,1,0);
 let a_spos:u64=get(packet,1,1);
 let a_widx:u64=get(packet,1,2);
 let a_wpos:u64=get(packet,1,3);
 let a_ridx:u64=get(packet,1,4);
 let a_psrc:u64=get(packet,1,5);
 let a_pdst:u64=get(packet,1,6);

 var sc:i64=0;
 while sc<64 as i64{ setat(scratch,512 as i64+sc,0 as i64); sc=sc+1 as i64; }
 var cnt_spos:i64=0;
 var cnt_write:i64=0;
 var cnt_read:i64=0;
 var cnt_persist:i64=0;
 var n_kv:i64=0;

 // n_visible and state_pos
 var i:i64=0;
 while i<n_tokens{
  let p:i64=at(posarr,i);
  setat(a_vis,i,0 as i64);
  if p >= (0 as i64){
   setat(a_spos,cnt_spos,p%ratio);
   cnt_spos=cnt_spos+1 as i64;
   let nv:i64=(p+(1 as i64))/ratio;
   setat(a_vis,i,nv);
   if nv>n_kv{ n_kv=nv; }
  }
  i=i+1 as i64;
 }

 // write/read lists and persist rows
 i=0;
 while i<n_tokens{
  let p:i64=at(posarr,i);
  if p < (0 as i64){ i=i+1 as i64; }else{
   let k:i64=at(nsid,i);
   var s:i64=0;
   while s<k{
    let seq_id:i64=at(sid,i*width+s);
    let off_st:i64=stream_offset(n_stream,seq_id,state_size);
    let st_idx:i64=off_st+p%state_size;

    // persist: first row for this destination wins; a later position replaces the source
    var found:i64=(0 as i64)-1 as i64;
    var q:i64=0;
    while q<cnt_persist{
     if at(scratch,q*(3 as i64))==st_idx{ found=q; }
     q=q+1 as i64;
    }
    if found < (0 as i64){
     setat(scratch,cnt_persist*(3 as i64),st_idx);
     setat(scratch,cnt_persist*(3 as i64)+(1 as i64),i);
     setat(scratch,cnt_persist*(3 as i64)+(2 as i64),p);
     cnt_persist=cnt_persist+1 as i64;
    }else{
     if p>at(scratch,found*(3 as i64)+(2 as i64)){
      setat(scratch,found*(3 as i64)+(1 as i64),i);
      setat(scratch,found*(3 as i64)+(2 as i64),p);
     }
    }

    if (p+(1 as i64))%ratio!=(0 as i64){ s=s+1 as i64; }else{
     let src_start:i64=p+(1 as i64)-ratio;
     let cache_off:i64=stream_offset(n_stream,seq_id,kv_size);
     setat(a_widx,cnt_write,cache_off+p/ratio);
     setat(a_wpos,cnt_write,src_start);
     cnt_write=cnt_write+1 as i64;
     setat(scratch,512 as i64+seq_id,at(scratch,512 as i64+seq_id)+(1 as i64));
     var j:i64=0;
     while j<ratio{
      let src:i64=source_idx(src_start+j,seq_id,state_size,n_tokens,posarr,width,sid,nsid,n_stream,state_rows);
      setat(a_ridx,cnt_read,src);
      cnt_read=cnt_read+1 as i64;
      j=j+1 as i64;
     }
     s=s+1 as i64;
    }
   }
   i=i+1 as i64;
  }
 }

 // CSA dummy block: keep the graph shape stable when no block completed
 if ratio==(4 as i64){
  if cnt_spos>(0 as i64){
   if coupled{
    if cnt_write==(0 as i64){
     var t:i64=0;
     var found_t:bool=false;
     while t<n_tokens&&!found_t{
      if at(posarr,t) >= (0 as i64){ found_t=true; }else{ t=t+1 as i64; }
     }
     ensure(found_t,63);
     let seq_id:i64=at(sid,t*width);
     let cache_off:i64=stream_offset(n_stream,seq_id,kv_size);
     let src:i64=source_idx(at(posarr,t),seq_id,state_size,n_tokens,posarr,width,sid,nsid,n_stream,state_rows);
     setat(a_widx,cnt_write,cache_off+kv_size-(1 as i64));
     setat(a_wpos,cnt_write,0 as i64);
     cnt_write=cnt_write+1 as i64;
     var j:i64=0;
     while j<ratio{
      setat(a_ridx,cnt_read,src);
      cnt_read=cnt_read+1 as i64;
      j=j+1 as i64;
     }
    }
   }else{
    var base_tok:i64=n_seq_tokens;
  if base_tok < (1 as i64){ base_tok=1 as i64; }
  let n_blk:i64=(base_tok+(ratio-(1 as i64)))/ratio;
    var s2:i64=0;
    while s2<n_seqs_unq{
     let seq_id:i64=at(sid_unq,s2);
     // writes for this sequence
     let n_w:i64=at(scratch,512 as i64+seq_id);
     var need:bool=true;
     if n_w>=n_blk{ need=false; }
     if need{
      if n_w+(1 as i64)!=n_blk{ fail(64); }
      var t:i64=0;
      var found:bool=false;
      while t<n_tokens&&!found{
       if at(posarr,t) >= (0 as i64){
        if token_has_seq(t,seq_id,width,sid,nsid){ found=true; }else{ t=t+1 as i64; }
       }else{ t=t+1 as i64; }
      }
      ensure(found,65);
      let cache_off:i64=stream_offset(n_stream,seq_id,kv_size);
      let src:i64=source_idx(at(posarr,t),seq_id,state_size,n_tokens,posarr,width,sid,nsid,n_stream,state_rows);
      setat(a_widx,cnt_write,cache_off+kv_size-(1 as i64));
      setat(a_wpos,cnt_write,0 as i64);
      cnt_write=cnt_write+1 as i64;
      var j:i64=0;
      while j<ratio{
       setat(a_ridx,cnt_read,src);
       cnt_read=cnt_read+1 as i64;
       j=j+1 as i64;
      }
     }
     s2=s2+1 as i64;
    }
   }
  }
 }

 // HCA: no block completes, but the graph still needs a masked dummy write
 if ratio==(128 as i64){
  if cnt_spos>(0 as i64){
   if cnt_write==(0 as i64){
    ensure(n_kv<kv_size,66);
    var t:i64=0;
    var found_t:bool=false;
    while t<n_tokens&&!found_t{
     if at(posarr,t) >= (0 as i64){ found_t=true; }else{ t=t+1 as i64; }
    }
    ensure(found_t,67);
    let seq_id:i64=at(sid,t*width);
    let cache_off:i64=stream_offset(n_stream,seq_id,kv_size);
    let src:i64=source_idx(at(posarr,t),seq_id,state_size,n_tokens,posarr,width,sid,nsid,n_stream,state_rows);
    setat(a_widx,cnt_write,cache_off+kv_size-(1 as i64));
    setat(a_wpos,cnt_write,0 as i64);
    cnt_write=cnt_write+1 as i64;
    var j:i64=0;
    while j<ratio{
     setat(a_ridx,cnt_read,src);
     cnt_read=cnt_read+1 as i64;
     j=j+1 as i64;
    }
   }
  }
 }

 // n_kv is padded to 256 and floored at 256 so the mask stays present
 n_kv=pad256(n_kv);
 if n_kv < (256 as i64){ n_kv=256 as i64; }

 // persist rows sorted by destination
 var a:i64=1;
 while a<cnt_persist{
  let dk:i64=at(scratch,a*(3 as i64));
  let ds:i64=at(scratch,a*(3 as i64)+(1 as i64));
  let dp:i64=at(scratch,a*(3 as i64)+(2 as i64));
  var m:i64=a-(1 as i64);
  var stop:bool=false;
  while m >= (0 as i64)&&!stop{
   let mk:i64=at(scratch,m*(3 as i64));
   if mk<=dk{ stop=true; }else{
    setat(scratch,(m+(1 as i64))*(3 as i64),mk);
    setat(scratch,(m+(1 as i64))*(3 as i64)+(1 as i64),at(scratch,m*(3 as i64)+(1 as i64)));
    setat(scratch,(m+(1 as i64))*(3 as i64)+(2 as i64),at(scratch,m*(3 as i64)+(2 as i64)));
    m=m-(1 as i64);
   }
  }
  setat(scratch,(m+(1 as i64))*(3 as i64),dk);
  setat(scratch,(m+(1 as i64))*(3 as i64)+(1 as i64),ds);
  setat(scratch,(m+(1 as i64))*(3 as i64)+(2 as i64),dp);
  a=a+1 as i64;
 }
 var q2:i64=0;
 while q2<cnt_persist{
  setat(a_psrc,q2,at(scratch,q2*(3 as i64)+(1 as i64)));
  setat(a_pdst,q2,at(scratch,q2*(3 as i64)));
  q2=q2+1 as i64;
 }

 setat(counts,0 as i64,cnt_spos);
 setat(counts,1 as i64,cnt_write);
 setat(counts,2 as i64,cnt_read);
 setat(counts,3 as i64,cnt_persist);
 setat(counts,4 as i64,n_kv);
 setat(counts,5 as i64,graph_n_stream(n_stream,n_seqs_unq,coupled));
 put(packet,3,0,1 as u64);
 put(packet,3,1,n_kv as u64);
 return true;
}

fn main(){run(0);}
