// Multi-shard naming and shard validation in Goose, from src/llama.cpp
// (llama_split_path) and src/llama-model-loader.cpp (the n_split > 1 block).
//
// Owned decisions:
//   - a shard path is "<prefix>-<split_no+1 padded to 5>-of-<count padded to 5>.gguf",
//     and the call fails when the result does not fit the buffer;
//   - the model must be opened on split 0;
//   - the number of listed splits must equal split.count;
//   - every additional file must declare split.no equal to its position;
//   - the loaded tensor count must equal split.tensors.count.
//
// The decimal formatting is done here; the file open and the name map stay native.
extern "fg459_st_u8" fn st_u8(p:u64,o:u64,v:u32);
extern "fg459_st_i64" fn st_i64(p:u64,o:u64,v:i64);

fn put(out:u64,i:i64,v:i64){ st_i64(out,(i*(8 as i64)) as u64,v); }

fn digits(v:i64)->i64{
 if v<=0 as i64 { return 1 as i64; }
 var d:i64=1 as i64;
 var t:i64=v;
 while t>=10 as i64 { t=t/10 as i64; d=d+1 as i64; }
 return d;
}

// width is at least five: %05d
fn width(v:i64)->i64{
 let d:i64=digits(v);
 if d>5 as i64 { return d; }
 return 5 as i64;
}

fn put_num(v:i64,w:i64,out:u8[:],o0:i64)->i64{
 let d:i64=digits(v);
 var o:i64=o0;
 var zeros:i64=w-d;
 while zeros>0 as i64 {
  out[o]=48 as u8;
  o=o+1 as i64;
  zeros=zeros-1 as i64;
 }
 var div:i64=1 as i64;
 var k:i64=1 as i64;
 while k<d {
  div=div*10 as i64;
  k=k+1 as i64;
 }
 var rem:i64=v;
 while div>0 as i64 {
  let dig:i64=rem/div;
  out[o]=(48 as i64+dig) as u8;
  o=o+1 as i64;
  rem=rem-(dig*div);
  div=div/10 as i64;
 }
 return o;
}

// Returns the written length, or 0 when it would not fit (native snprintf check).
fn split_path(prefix:const u8[:],split_no:i64,count:i64,out:u8[:])->i64{
 if split_no<0 as i64 || count<0 as i64 { return 0 as i64; }
 let n:i64=split_no+1 as i64;
 let wa:i64=width(n);
 let wb:i64=width(count);
 // prefix + '-' + wa + "-of-" + wb + ".gguf"
 let total:i64=prefix.len+1 as i64+wa+4 as i64+wb+5 as i64;
 if total>=out.len { return 0 as i64; }
 var o:i64=0 as i64;
 var i:i64=0 as i64;
 while i<prefix.len {
  out[o]=prefix[i];
  o=o+1 as i64;
  i=i+1 as i64;
 }
 out[o]=45 as u8; o=o+1 as i64;                       // '-'
 o=put_num(n,wa,out,o);
 out[o]=45 as u8; o=o+1 as i64;                       // '-'
 out[o]=111 as u8; o=o+1 as i64;                      // 'o'
 out[o]=102 as u8; o=o+1 as i64;                      // 'f'
 out[o]=45 as u8; o=o+1 as i64;                       // '-'
 o=put_num(count,wb,out,o);
 out[o]=46 as u8; o=o+1 as i64;                       // '.'
 out[o]=103 as u8; o=o+1 as i64;                      // 'g'
 out[o]=103 as u8; o=o+1 as i64;                      // 'g'
 out[o]=117 as u8; o=o+1 as i64;                      // 'u'
 out[o]=102 as u8; o=o+1 as i64;                      // 'f'
 // snprintf's contract includes the terminator; the capacity check above
 // already guarantees room for it
 out[o]=0 as u8;
 return o;
}

// 1 when the model is opened on the first split.
fn first_shard_ok(idx:i64)->i64{
 if idx==0 as i64 { return 1 as i64; }
 return 0 as i64;
}

// 1 when the listed split count matches split.count.
fn split_count_ok(n_split:i64,listed:i64)->i64{
 if n_split==listed { return 1 as i64; }
 return 0 as i64;
}

// 1 when an additional file declares its own position.
fn shard_index_ok(expected:i64,actual:i64)->i64{
 if expected==actual { return 1 as i64; }
 return 0 as i64;
}

// 1 when the accumulated tensor count equals split.tensors.count.
fn tensor_count_ok(expected:i64,loaded:i64)->i64{
 if expected==loaded { return 1 as i64; }
 return 0 as i64;
}

// Endian: the container is little-endian only. The magic is compared as a u32
// read little-endian, so a big-endian file is rejected here; native gguf.cpp has
// no byte-swap path either.
fn magic_ok(v:i64)->i64{
 if v==1179993927 as i64 { return 1 as i64; }
 return 0 as i64;
}

fn version_ok(v:i64)->i64{
 if v==2 as i64 { return 1 as i64; }
 if v==3 as i64 { return 1 as i64; }
 return 0 as i64;
}

fn main(){
 digits(1 as i64);
 width(1 as i64);
 var buf:u8[64] = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
 split_path("m",0 as i64,2 as i64,buf);
 first_shard_ok(0 as i64);
 split_count_ok(2 as i64,2 as i64);
 shard_index_ok(1 as i64,1 as i64);
 tensor_count_ok(1 as i64,1 as i64);
 magic_ok(1179993927 as i64);
 version_ok(3 as i64);
}
