// mmap/pread read decisions in Goose, from src/llama-mmap.cpp (llama_file:
// init_fd, read_raw, read_raw_unsafe, read_alignment) and the loader's
// mmap-vs-file source selection.
//
// Owned decisions:
//   - buffered reads are truncated at EOF, and a short read only errors when
//     something was requested before the end;
//   - a direct read retries on EINTR, falls back to buffered I/O on EFAULT/EINVAL,
//     and is fatal otherwise;
//   - an EOF exactly at the file size zero-fills the requested padding, while an
//     EOF before it is an error;
//   - the read alignment is the block size for a direct descriptor and 1 for a
//     buffered stream;
//   - a tensor's bytes come from the mapping when mmap is used, otherwise from
//     the file at the weight offset.
//
// The syscalls stay native.
extern "fg464_st_i64" fn st_i64(p:u64,o:u64,v:i64);

fn put(out:u64,i:i64,v:i64){ st_i64(out,(i*(8 as i64)) as u64,v); }

// Buffered read plan: bytes to ask for, and whether a short read is an error.
// A request that starts at or past EOF reads zero bytes without an error.
export fn buffered_plan(size:i64,off:i64,len:i64,out:u64)->i64{
 var to_read:i64=len;
 if off>=size as i64 { to_read=0 as i64; }
 else{
  let left:i64=size-off;
  if left<to_read { to_read=left; }
 }
 var short_is_error:i64=0 as i64;
 if to_read>0 as i64 { short_is_error=1 as i64; }
 put(out,0 as i64,to_read);
 put(out,1 as i64,short_is_error);
 return to_read;
}

// errno classes: 1 EINTR, 2 EFAULT, 3 EINVAL, 4 other.
// Actions: 0 retry, 1 fall back to buffered, 2 fatal.
export fn err_action(e:i64,out:u64)->i64{
 var a:i64=2 as i64;
 if e==1 as i64 { a=0 as i64; }
 if e==2 as i64 || e==3 as i64 { a=1 as i64; }
 put(out,0 as i64,a);
 return a;
}

// EOF handling for a direct read: 1 means zero-fill the rest of the request is
// allowed (the position is exactly the file size), 0 means error.
export fn eof_policy(size:i64,pos:i64,out:u64)->i64{
 var zero_ok:i64=0 as i64;
 if pos==size as i64 { zero_ok=1 as i64; }
 put(out,0 as i64,zero_ok);
 return zero_ok;
}

// 1 for a buffered stream, the block size for an O_DIRECT descriptor.
export fn read_align(fd_open:i64,blksize:i64,out:u64)->i64{
 var a:i64=1 as i64;
 if fd_open!=0 as i64 && blksize>0 as i64 { a=blksize; }
 put(out,0 as i64,a);
 return a;
}

// Source selection for a tensor read: 1 mapping, 0 file.
export fn src_kind(use_mmap:i64,has_mapping:i64,out:u64)->i64{
 var k:i64=0 as i64;
 if use_mmap!=0 as i64 && has_mapping!=0 as i64 { k=1 as i64; }
 put(out,0 as i64,k);
 return k;
}

// Byte offset of the tensor inside its mapping: mapping base + weight offset +
// inner offset, or 0 when the read does not come from a mapping.
export fn map_offset(kind:i64,weight_offs:i64,inner:i64,out:u64)->i64{
 var o:i64=0 as i64;
 if kind!=0 as i64 { o=weight_offs+inner; }
 put(out,0 as i64,o);
 return o;
}
