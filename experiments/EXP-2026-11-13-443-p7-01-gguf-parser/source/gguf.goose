// Minimal GGUF structural parser in Goose.
//
// Reads the header, every metadata key/value (skipping values by type,
// including arrays), and every tensor descriptor, then writes fixed 64-bit
// records to the caller buffer. Returns the number of words written.
//
// The Python reference in check.py parses the same bytes independently; the
// fixture compares the record streams. Truncated/corrupt inputs return a
// negative code instead of reading past the buffer.
extern "fg443_ld_u8" fn ld_u8(p:u64,o:u64)->u8;
extern "fg443_ld_u32" fn ld_u32(p:u64,o:u64)->u32;
extern "fg443_ld_u64" fn ld_u64(p:u64,o:u64)->u64;
extern "fg443_st_u64" fn st_u64(p:u64,o:u64,v:u64);

fn w64(out:u64,cap:i64,o:i64,v:u64)->i64{
 if o >=(0 as i64)&&o+(8 as i64)<=cap{ st_u64(out,o as u64,v); }
 return o+(8 as i64);
}

fn hash_str(buf:u64,pos:i64,n:i64)->u64{
 var h:u64=14695981039346656037 as u64;
 var i:i64=0 as i64;
 while i <n{
  h=(h^((ld_u8(buf,(pos+i) as u64)) as u64))*(1099511628211 as u64);
  i=i+(1 as i64);
 }
 return h;
}

// new position after one value of type t, or negative on error.
fn skip_one(buf:u64,len:i64,pos:i64,t:i64)->i64{
 if t==(0 as i64)||t==(1 as i64)||t==(7 as i64){return pos+(1 as i64);}
 if t==(2 as i64)||t==(3 as i64){return pos+(2 as i64);}
 if t==(4 as i64)||t==(5 as i64)||t==(6 as i64){return pos+(4 as i64);}
 if t==(10 as i64)||t==(11 as i64)||t==(12 as i64){return pos+(8 as i64);}
 if t==(8 as i64){
  if pos+(8 as i64)>len{return -(20 as i64);}
  let n:i64=ld_u64(buf,pos as u64) as i64;
  if n < 0 as i64 {return -(21 as i64);}
 if pos+(8 as i64)+n > len {return -(21 as i64);}
  return pos+(8 as i64)+n;
 }
 return -(22 as i64);
}

fn skip_array(buf:u64,len:i64,pos:i64)->i64{
 if pos+(12 as i64)>len{return -(30 as i64);}
 let et:i64=(ld_u32(buf,pos as u64) as i64);
 let ecount:i64=ld_u64(buf,(pos+(4 as i64)) as u64) as i64;
 if ecount < 0 as i64 {return -(31 as i64);}
 if ecount > 100000000 as i64 {return -(31 as i64);}
 var p:i64=pos+(12 as i64);
 var i:i64=0 as i64;
 while i <ecount{
  p=skip_one(buf,len,p,et);
  if p < 0 as i64 {return p;}
  i=i+(1 as i64);
 }
 return p;
}

fn parse(buf:u64,len:i64,out:u64,cap:i64)->i64{
 if len < (24 as i64){return -(1 as i64);}
 if ld_u32(buf,0 as u64)!=(1179993927 as u32){return -(2 as i64);}
 let version:i64=(ld_u32(buf,(4 as i64) as u64) as i64);
 if version!=(2 as i64)&&version!=(3 as i64){return -(3 as i64);}
 let tensor_count:i64=ld_u64(buf,(8 as i64) as u64) as i64;
 let kv_count:i64=ld_u64(buf,(16 as i64) as u64) as i64;
 if tensor_count < 0 as i64 {return -(4 as i64);}
 if tensor_count > 10000000 as i64 {return -(4 as i64);}
 if kv_count < 0 as i64 {return -(4 as i64);}
 if kv_count > 10000000 as i64 {return -(4 as i64);}
 var pos:i64=24 as i64;
 var o:i64=0 as i64;
 o=w64(out,cap,o,((version as u64)<< (32 as u64))|(1179993927 as u64));
 o=w64(out,cap,o,tensor_count as u64);
 o=w64(out,cap,o,kv_count as u64);
 var k:i64=0 as i64;
 while k <kv_count{
  if pos+(8 as i64)>len{return -(10 as i64);}
  let kl:i64=ld_u64(buf,pos as u64) as i64;
  if kl < 0 as i64 {return -(11 as i64);}
 if pos+(8 as i64)+kl+(4 as i64) > len {return -(11 as i64);}
  let kh:u64=hash_str(buf,pos+(8 as i64),kl);
  pos=pos+(8 as i64)+kl;
  let vt:i64=(ld_u32(buf,pos as u64) as i64);
  pos=pos+(4 as i64);
  var a0:u64=0 as u64;
  var a1:u64=0 as u64;
  if vt==(9 as i64){
   if pos+(12 as i64)>len{return -(12 as i64);}
   a0=(ld_u32(buf,pos as u64) as u64);
   a1=ld_u64(buf,(pos+(4 as i64)) as u64);
   pos=skip_array(buf,len,pos);
   if pos < 0 as i64 {return pos;}
  }else{
   pos=skip_one(buf,len,pos,vt);
   if pos < 0 as i64 {return pos;}
  }
  o=w64(out,cap,o,kl as u64);
  o=w64(out,cap,o,kh);
  o=w64(out,cap,o,vt as u64);
  o=w64(out,cap,o,a0);
  o=w64(out,cap,o,a1);
  k=k+(1 as i64);
 }
 var t:i64=0 as i64;
 while t <tensor_count{
  if pos+(8 as i64)>len{return -(40 as i64);}
  let nl:i64=ld_u64(buf,pos as u64) as i64;
  if nl < 0 as i64 {return -(41 as i64);}
 if pos+(8 as i64)+nl+(4 as i64) > len {return -(41 as i64);}
  let nh:u64=hash_str(buf,pos+(8 as i64),nl);
  pos=pos+(8 as i64)+nl;
  let nd:i64=(ld_u32(buf,pos as u64) as i64);
  pos=pos+(4 as i64);
  if nd < 1 as i64 {return -(42 as i64);}
 if nd > 4 as i64 {return -(42 as i64);}
  var d0:u64=0 as u64;
  var d1:u64=0 as u64;
  var d2:u64=0 as u64;
  var d3:u64=0 as u64;
  var j:i64=0 as i64;
  while j <nd{
   if pos+(8 as i64)>len{return -(43 as i64);}
   let dv:u64=ld_u64(buf,pos as u64);
   if j==(0 as i64){d0=dv;}
   if j==(1 as i64){d1=dv;}
   if j==(2 as i64){d2=dv;}
   if j==(3 as i64){d3=dv;}
   pos=pos+(8 as i64);
   j=j+(1 as i64);
  }
  if pos+(12 as i64)>len{return -(44 as i64);}
  let ty:u64=ld_u32(buf,pos as u64) as u64;
  let off:u64=ld_u64(buf,(pos+(4 as i64)) as u64);
  pos=pos+(12 as i64);
  o=w64(out,cap,o,nl as u64);
  o=w64(out,cap,o,nh);
  o=w64(out,cap,o,ty);
  o=w64(out,cap,o,nd as u64);
  o=w64(out,cap,o,d0);
  o=w64(out,cap,o,d1);
  o=w64(out,cap,o,d2);
  o=w64(out,cap,o,d3);
  o=w64(out,cap,o,off);
  t=t+(1 as i64);
 }
 return o/(8 as i64);
}


fn key_eq(buf:u64,pos:i64,n:i64,kp:u64,kl:i64)->bool{
 if n!=kl {return false;}
 var i:i64=0 as i64;
 while i<n{
  if ld_u8(buf,(pos+i) as u64)!=ld_u8(kp,(i as u64)){return false;}
  i=i+(1 as i64);
 }
 return true;
}

// Returns 1 and writes five words: type, value offset, length/count, elem type, 0.
// Returns 0 if the key is absent, negative on malformed input.
fn lookup(buf:u64,len:i64,kp:u64,kl:i64,out:u64,cap:i64)->i64{
 if len < 24 as i64 {return -(1 as i64);}
 if ld_u32(buf,0 as u64) != 1179993927 as u32 {return -(2 as i64);}
 let version:i64=(ld_u32(buf,(4 as i64) as u64) as i64);
 if version != 2 as i64 && version != 3 as i64 {return -(3 as i64);}
 let kv_count:i64=ld_u64(buf,(16 as i64) as u64) as i64;
 if kv_count < 0 as i64 {return -(4 as i64);}
 if kv_count > 10000000 as i64 {return -(4 as i64);}
 var pos:i64=24 as i64;
 var k:i64=0 as i64;
 while k<kv_count{
  if pos+(8 as i64) > len {return -(10 as i64);}
  let n:i64=ld_u64(buf,pos as u64) as i64;
  if n < 0 as i64 {return -(11 as i64);}
  if pos+(8 as i64)+n+(4 as i64) > len {return -(11 as i64);}
  if key_eq(buf,pos+(8 as i64),n,kp,kl){
   pos=pos+(8 as i64)+n;
   let vt:i64=(ld_u32(buf,pos as u64) as i64);
   pos=pos+(4 as i64);
   if vt == 8 as i64 {
    if pos+(8 as i64) > len {return -(12 as i64);}
    let sl:i64=ld_u64(buf,pos as u64) as i64;
    if sl < 0 as i64 {return -(12 as i64);}
    if pos+(8 as i64)+sl > len {return -(12 as i64);}
    w64(out,cap,0 as i64,8 as u64);
    w64(out,cap,8 as i64,(pos+(8 as i64)) as u64);
    w64(out,cap,16 as i64,sl as u64);
    w64(out,cap,24 as i64,0 as u64);
    w64(out,cap,32 as i64,0 as u64);
    return 1 as i64;
   }
   if vt == 9 as i64 {
    if pos+(12 as i64) > len {return -(13 as i64);}
    let et:i64=(ld_u32(buf,pos as u64) as i64);
    let cnt:i64=ld_u64(buf,(pos+(4 as i64)) as u64) as i64;
    if cnt < 0 as i64 {return -(13 as i64);}
    if cnt > 100000000 as i64 {return -(13 as i64);}
    w64(out,cap,0 as i64,9 as u64);
    w64(out,cap,8 as i64,(pos+(12 as i64)) as u64);
    w64(out,cap,16 as i64,cnt as u64);
    w64(out,cap,24 as i64,et as u64);
    w64(out,cap,32 as i64,0 as u64);
    return 1 as i64;
   }
   var sz:i64=0 as i64;
   if vt == 0 as i64 || vt == 1 as i64 || vt == 7 as i64 {sz=1 as i64;}
   if vt == 2 as i64 || vt == 3 as i64 {sz=2 as i64;}
   if vt == 4 as i64 || vt == 5 as i64 || vt == 6 as i64 {sz=4 as i64;}
   if vt == 10 as i64 || vt == 11 as i64 || vt == 12 as i64 {sz=8 as i64;}
   if sz == 0 as i64 {return -(14 as i64);}
   if pos+sz > len {return -(14 as i64);}
   w64(out,cap,0 as i64,vt as u64);
   w64(out,cap,8 as i64,pos as u64);
   w64(out,cap,16 as i64,sz as u64);
   w64(out,cap,24 as i64,0 as u64);
   w64(out,cap,32 as i64,0 as u64);
   return 1 as i64;
  }
  pos=pos+(8 as i64)+n;
  let vt:i64=(ld_u32(buf,pos as u64) as i64);
  pos=pos+(4 as i64);
  if vt == 9 as i64 {
   pos=skip_array(buf,len,pos);
   if pos < 0 as i64 {return pos;}
  }else{
   pos=skip_one(buf,len,pos,vt);
   if pos < 0 as i64 {return pos;}
  }
  k=k+(1 as i64);
 }
 return 0 as i64;
}

fn main(){lookup(0 as u64,0 as i64,0 as u64,0 as i64,0 as u64,0 as i64); parse(0 as u64,0 as i64,0 as u64,0 as i64);}
