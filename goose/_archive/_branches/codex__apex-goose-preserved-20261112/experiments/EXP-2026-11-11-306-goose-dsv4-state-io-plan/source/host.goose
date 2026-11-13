// Apex DSV4 state tensor I/O plan in Goose:
// dsv4_state_write_tensor_streams / dsv4_state_read_tensor_streams, without the bytes.
//
// Each call writes four metadata fields (type, ne0, rows, row size) and then one transfer record per
// stream, at offset stream*stream_stride and size n_rows*row_size. The read side checks the same
// metadata and emits records for s0+s. Every rejection is a contract: native throws, so the FFI
// form is -1 and the gate pins it.
//
// Record layout: (offset, size) pairs in the output array.
//
// Packet, int64 slots:
//   k=0  0 op, 1 out, 2 n_rows, 3 tensor_rows, 4 row_size, 5 s0, 6 ns, 7 ids, 8 ids_len,
//        9 ne2, 10 type, 11 ne0
//   k=1  0..3 = stream metadata for the read side (type, ne0, rows, row_size)
//   k=3  0 done, 1 ret (record count, or -1)
extern "fg306k_get" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg306k_put" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg306k_ld_i64" fn ld(base:u64,byte:i64)->i64;
extern "fg306k_st_i64" fn st(base:u64,byte:i64,v:i64);
extern "fg306k_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn at(arr:u64,i:i64)->i64{return ld(arr,i*(8 as i64));}
fn setat(arr:u64,i:i64,v:i64){st(arr,i*(8 as i64),v);}

fn run(packet:i64)->bool{
 let op:i64=get(packet,0,0) as i64;
 let out:u64=get(packet,0,1);
 let n_rows:i64=get(packet,0,2) as i64;
 let tensor_rows:i64=get(packet,0,3) as i64;
 let row_size:i64=get(packet,0,4) as i64;
 let s0:i64=get(packet,0,5) as i64;
 let ns:i64=get(packet,0,6) as i64;
 let ids:u64=get(packet,0,7);
 let ids_len:i64=get(packet,0,8) as i64;
 let ne2:i64=get(packet,0,9) as i64;
 let type_i:i64=get(packet,0,10) as i64;
 let ne0:i64=get(packet,0,11) as i64;
 let has_ids:bool=get(packet,0,15)!=(0 as u64);
 ensure(n_rows>=(0 as i64)&&ns>=(0 as i64),60);
 ensure(row_size>=(0 as i64),61);

 let stream_stride:i64=tensor_rows*row_size;
 let size:i64=n_rows*row_size;
 var ret:i64=0;

 if op==(0 as i64){
  if n_rows>tensor_rows{ ret=(0 as i64)-1 as i64; }else{
   if size==(0 as i64){ ret=0 as i64; }else{
    if has_ids{
     if ids_len!=ns{ ret=(0 as i64)-1 as i64; }
    }
    if ret>=(0 as i64){
     var s:i64=0;
     var bad:bool=false;
     while s<ns&&!bad{
      var stream:i64=s0+s;
      if has_ids{ stream=at(ids,s); }
      if stream>=ne2{ bad=true; }else{
       setat(out,(2 as i64*s),stream*stream_stride);
       setat(out,(2 as i64*s+1 as i64),size);
      }
      s=s+1 as i64;
     }
     if bad{ ret=(0 as i64)-1 as i64; }else{ ret=ns; }
    }
   }
  }
 }else{
  if op==(1 as i64){
   let rt:i64=get(packet,1,0) as i64;
   let rn:i64=get(packet,1,1) as i64;
   let rr:i64=get(packet,1,2) as i64;
   let rs:i64=get(packet,1,3) as i64;
   if type_i!=rt{ ret=(0 as i64)-1 as i64; }else{
    if ne0!=rn{ ret=(0 as i64)-1 as i64; }else{
     if n_rows!=rr{ ret=(0 as i64)-1 as i64; }else{
      if row_size!=rs{ ret=(0 as i64)-1 as i64; }else{
       if n_rows>tensor_rows{ ret=(0 as i64)-1 as i64; }else{
        if size==(0 as i64){ ret=0 as i64; }else{
         var s:i64=0;
         while s<ns{
          setat(out,(2 as i64*s),(s0+s)*stream_stride);
          setat(out,(2 as i64*s+1 as i64),size);
          s=s+1 as i64;
         }
         ret=ns;
        }
       }
      }
     }
    }
   }
  }else{
   ensure(false,62);
  }
 }
 put(packet,3,0,1 as u64);
 put(packet,3,1,ret as u64);
 return true;
}

fn main(){run(0);}
