// Apex recurrent state payload plan in Goose:
// llama_memory_recurrent::{state_write_data, state_read_data}, without the bytes.
//
// The native functions do two things: they validate the stream header and every tensor's type
// and row size, and they translate cell ranges into (tensor, offset, size) transfer records. The
// bytes themselves are ggml tensor rows; this module produces the records, so the arithmetic that
// decides which bytes move is testable on its own.
//
// Record stream, five int64 slots each: kind, id, off, size, val.
//   kind 0: u32 field, val = value
//   kind 1: u64 field, val = value
//   kind 2: write_tensor(id, off, size)
//   kind 3: read_tensor(id, off, size)
//   id = il*4 + which, which 0 = r, 1 = s, 2 = p
//
// Layer tables (packet k=1..3): r_present, r_type, r_row, s_present, s_type, s_row, p_present, p_row.
//
// Packet, int64 slots:
//   k=0  0 op, 1 out records, 2 out count ptr, 3 n_layer, 4 n_embd_r, 5 n_embd_s,
//        6 ple_conv_state, 7 size, 8 cell_count, 9 head, 10 s_trans, 11 range_count,
//        12 ranges, 13 stream_u32 (input for read)
//   k=1  r_present ptr, 2 r_type ptr, 3 r_row ptr
//   k=2  s_present ptr, 2 s_type ptr, 3 s_row ptr
//   k=3  p_present ptr, 2 p_row ptr, 4 stream cursor
//   k=4 (outputs) 0 ok
extern "fg297k_get" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg297k_put" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg297k_ld_i64" fn ld(base:u64,byte:i64)->i64;
extern "fg297k_st_i64" fn st(base:u64,byte:i64,v:i64);
extern "fg297k_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn emit(st_out:u64,cntp:u64,kind:i64,id:i64,off:i64,size:i64,val:i64){
 let c:i64=ld(cntp,0 as i64);
 st(st_out,(c*(5 as i64))*(8 as i64),kind);
 st(st_out,(c*(5 as i64)+1 as i64)*(8 as i64),id);
 st(st_out,(c*(5 as i64)+2 as i64)*(8 as i64),off);
 st(st_out,(c*(5 as i64)+3 as i64)*(8 as i64),size);
 st(st_out,(c*(5 as i64)+4 as i64)*(8 as i64),val);
 st(cntp,0 as i64,c+(1 as i64));
}

fn u32e(st_out:u64,cntp:u64,v:i64){ emit(st_out,cntp,0 as i64,0 as i64,0 as i64,0 as i64,v); }
fn u64e(st_out:u64,cntp:u64,v:i64){ emit(st_out,cntp,1 as i64,0 as i64,0 as i64,0 as i64,v); }
fn wr(st_out:u64,cntp:u64,id:i64,off:i64,size:i64){ emit(st_out,cntp,2 as i64,id,off,size,0 as i64); }
fn rd(st_out:u64,cntp:u64,id:i64,off:i64,size:i64){ emit(st_out,cntp,3 as i64,id,off,size,0 as i64); }

fn at(arr:u64,i:i64)->i64{return ld(arr,i*(8 as i64));}

// state_write_data
fn write_plan(packet:i64)->bool{
 let st_out:u64=get(packet,0,1);
 let cntp:u64=get(packet,0,2);
 let n_layer:i64=get(packet,0,3) as i64;
 let n_embd_s:i64=get(packet,0,5) as i64;
 let size_:i64=get(packet,0,7) as i64;
 let s_trans:i64=get(packet,0,10) as i64;
 let rc:i64=get(packet,0,11) as i64;
 let ranges:u64=get(packet,0,12);
 let rp:u64=get(packet,1,0);
 let rtype:u64=get(packet,1,1);
 let rrow:u64=get(packet,1,2);
 let sp:u64=get(packet,2,0);
 let stype:u64=get(packet,2,1);
 let srow:u64=get(packet,2,2);
 let pp:u64=get(packet,3,0);
 let prow:u64=get(packet,3,1);
 st(cntp,0 as i64,0 as i64);
 u32e(st_out,cntp,0 as i64);
 u32e(st_out,cntp,n_layer);
 var il:i64=0;
 while il<n_layer{
  if at(rp,il)!=(0 as i64){
   u32e(st_out,cntp,at(rtype,il));
   u64e(st_out,cntp,at(rrow,il));
   var k:i64=0;
   while k<rc{
    let lo:i64=at(ranges,(2 as i64*k));
    let hi:i64=at(ranges,(2 as i64*k+1 as i64));
    let rr:i64=at(rrow,il);
    let nn:i64=hi-lo;
    let sz:i64=nn*rr;
    wr(st_out,cntp,il*(4 as i64),lo*rr,sz);
    k=k+1 as i64;
   }
  }
  if at(pp,il)!=(0 as i64){
   u64e(st_out,cntp,at(prow,il));
   var k:i64=0;
   while k<rc{
    let lo:i64=at(ranges,(2 as i64*k));
    let hi:i64=at(ranges,(2 as i64*k+1 as i64));
    let sz:i64=(hi-lo)*at(prow,il);
    wr(st_out,cntp,il*(4 as i64)+(2 as i64),(lo*at(prow,il)),sz);
    k=k+1 as i64;
   }
  }
  il=il+1 as i64;
 }
 if s_trans==(0 as i64){
  il=0;
  while il<n_layer{
   if at(sp,il)!=(0 as i64){
    u32e(st_out,cntp,at(stype,il));
    u64e(st_out,cntp,at(srow,il));
    var k:i64=0;
    while k<rc{
     let lo:i64=at(ranges,(2 as i64*k));
     let hi:i64=at(ranges,(2 as i64*k+1 as i64));
     let sz:i64=(hi-lo)*at(srow,il);
     wr(st_out,cntp,il*(4 as i64)+(1 as i64),(lo*at(srow,il)),sz);
     k=k+1 as i64;
    }
   }
   il=il+1 as i64;
  }
 }else{
  il=0;
  while il<n_layer{
   if at(sp,il)!=(0 as i64){
    u32e(st_out,cntp,at(stype,il));
    u32e(st_out,cntp,(get(packet,3,2) as i64));
    u32e(st_out,cntp,n_embd_s);
    var j:i64=0;
    while j<n_embd_s{
     var k:i64=0;
     while k<rc{
      let lo:i64=at(ranges,(2 as i64*k));
      let hi:i64=at(ranges,(2 as i64*k+1 as i64));
      let el:i64=get(packet,3,2) as i64;
      let off:i64=(lo+j*size_)*el;
      let sz:i64=(hi-lo)*el;
      wr(st_out,cntp,il*(4 as i64)+(1 as i64),off,sz);
      k=k+1 as i64;
     }
     j=j+1 as i64;
    }
   }
   il=il+1 as i64;
  }
 }
 return true;
}

// state_read_data: validates the header and each tensor, then emits the read records.
fn read_plan(packet:i64)->bool{
 let st_out:u64=get(packet,0,1);
 let cntp:u64=get(packet,0,2);
 let n_layer:i64=get(packet,0,3) as i64;
 let n_embd_s:i64=get(packet,0,5) as i64;
 let size_:i64=get(packet,0,7) as i64;
 let cell_count:i64=get(packet,0,8) as i64;
 let head:i64=get(packet,0,9) as i64;
 let s_trans:i64=get(packet,0,10) as i64;
 let stream:u64=get(packet,0,13);
 let rp:u64=get(packet,1,0);
 let rtype:u64=get(packet,1,1);
 let rrow:u64=get(packet,1,2);
 let sp:u64=get(packet,2,0);
 let stype:u64=get(packet,2,1);
 let srow:u64=get(packet,2,2);
 let pp:u64=get(packet,3,0);
 let prow:u64=get(packet,3,1);
 var cur:i64=ld(get(packet,3,4),0 as i64);
 st(cntp,0 as i64,0 as i64);

 let hdr_s:i64=ld(stream,cur); cur=cur+(8 as i64);
 let hdr_n:i64=ld(stream,cur); cur=cur+(8 as i64);
 if hdr_n!=n_layer{ st(get(packet,3,4),0 as i64,cur);
   put(packet,4,0,0 as u64);
   return true; }
 if cell_count>size_{ st(get(packet,3,4),0 as i64,cur);
   put(packet,4,0,0 as u64);
   return true; }
 if hdr_s!=(0 as i64){ st(get(packet,3,4),0 as i64,cur);
   put(packet,4,0,0 as u64);
   return true; }

 var il:i64=0;
 while il<n_layer{
  if at(rp,il)!=(0 as i64){
   let tr:i64=ld(stream,cur); cur=cur+(8 as i64);
   let rr:i64=ld(stream,cur); cur=cur+(8 as i64);
   if tr!=at(rtype,il){ st(get(packet,3,4),0 as i64,cur);
   put(packet,4,0,0 as u64);
   return true; }
   if rr!=at(rrow,il){ st(get(packet,3,4),0 as i64,cur);
   put(packet,4,0,0 as u64);
   return true; }
   if cell_count>(0 as i64){
    rd(st_out,cntp,il*(4 as i64),head*at(rrow,il),cell_count*at(rrow,il));
   }
  }
  if at(pp,il)!=(0 as i64){
   let pr:i64=ld(stream,cur); cur=cur+(8 as i64);
   if pr!=at(prow,il){ st(get(packet,3,4),0 as i64,cur);
   put(packet,4,0,0 as u64);
   return true; }
   if cell_count>(0 as i64){
    rd(st_out,cntp,il*(4 as i64)+(2 as i64),head*at(prow,il),cell_count*at(prow,il));
   }
  }
  il=il+1 as i64;
 }
 if s_trans==(0 as i64){
  il=0;
  while il<n_layer{
   if at(sp,il)!=(0 as i64){
    let ts:i64=ld(stream,cur); cur=cur+(8 as i64);
    let sr:i64=ld(stream,cur); cur=cur+(8 as i64);
    if ts!=at(stype,il){ st(get(packet,3,4),0 as i64,cur);
   put(packet,4,0,0 as u64);
   return true; }
    if sr!=at(srow,il){ st(get(packet,3,4),0 as i64,cur);
   put(packet,4,0,0 as u64);
   return true; }
    if cell_count>(0 as i64){
     rd(st_out,cntp,il*(4 as i64)+(1 as i64),head*at(srow,il),cell_count*at(srow,il));
    }
   }
   il=il+1 as i64;
  }
 }else{
  il=0;
  while il<n_layer{
   if at(sp,il)!=(0 as i64){
    let ts:i64=ld(stream,cur); cur=cur+(8 as i64);
    let el:i64=ld(stream,cur); cur=cur+(8 as i64);
    let ne:i64=ld(stream,cur); cur=cur+(8 as i64);
    if ts!=at(stype,il){ st(get(packet,3,4),0 as i64,cur);
   put(packet,4,0,0 as u64);
   return true; }
    if el!=(get(packet,3,2) as i64){ st(get(packet,3,4),0 as i64,cur);
   put(packet,4,0,0 as u64);
   return true; }
    if ne!=n_embd_s{ st(get(packet,3,4),0 as i64,cur);
   put(packet,4,0,0 as u64);
   return true; }
    if cell_count>(0 as i64){
     var j:i64=0;
     while j<n_embd_s{
      rd(st_out,cntp,il*(4 as i64)+(1 as i64),(head+j*size_)*el,cell_count*el);
      j=j+1 as i64;
     }
    }
   }
   il=il+1 as i64;
  }
 }
 st(get(packet,3,4),0 as i64,cur);
 put(packet,3,3,cur as u64);
 put(packet,4,0,1 as u64);
 return true;
}

fn run(packet:i64)->bool{
 let op:i64=get(packet,0,0) as i64;
 ensure(op==(0 as i64)||op==(1 as i64),60);
 if op==(0 as i64){ return write_plan(packet); }
 return read_plan(packet);
}

fn main(){run(0);}
