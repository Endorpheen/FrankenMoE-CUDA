// Async-upload staging ring in Goose, from src/llama-model-loader.cpp
// (load_all_data: alignment aggregation, n_buffers rotation, the chunk loop and
// the upload_backend eligibility chain, plus the biggest-first tensor ordering).
//
// Owned decisions:
//   - the staging alignment is the maximum read alignment of the files, at least 1;
//   - the ring rotates through four buffers;
//   - each chunk reads min(buffer_size, remaining) bytes, skips the leading
//     alignment padding on the first chunk and trims the trailing padding on the
//     last one;
//   - async uploads are eligible only when the loader is not mmap/checking, the
//     buffer type is the device default and the device exposes async, host-buffer
//     and event capabilities with a host buffer type;
//   - without mmap, staged (non-host) tensors are loaded first, largest first.
//
// The buffers, events and the transfer itself stay native.
extern "fg461_st_i64" fn st_i64(p:u64,o:u64,v:i64);

fn put(out:u64,i:i64,v:i64){ st_i64(out,(i*(8 as i64)) as u64,v); }

// Maximum read alignment across the files, never below 1.
export fn align_max(xs:i64[:])->i64{
 var m:i64=1 as i64;
 var i:i64=0 as i64;
 while i<xs.len {
  if xs[i]>m { m=xs[i]; }
  i=i+1 as i64;
 }
 return m;
}

// Four staging buffers, used round-robin.
export fn ring_next(idx:i64,n:i64)->i64{
 if n<=0 as i64 { return 0 as i64; }
 return (idx+1 as i64)%n;
}

// One chunk of the aligned read. bytes_read is how much of total is done, from
// is the offset inside the alignment window, tail is the trailing padding of the
// whole read (read_end - (offset + n_size)). Writes read_size, pointer offset
// inside the staged chunk and the number of bytes to copy.
export fn chunk(total:i64,buffer_size:i64,bytes_read:i64,from:i64,tail:i64,out:u64)->i64{
 let remaining:i64=total-bytes_read;
 var n:i64=buffer_size;
 if remaining<n { n=remaining; }
 var ptr:i64=0 as i64;
 if bytes_read==0 as i64 { ptr=from; }
 var copy:i64=n-ptr;
 if bytes_read+n>total-tail { copy=copy-tail; }
 put(out,0 as i64,n);
 put(out,1 as i64,ptr);
 put(out,2 as i64,copy);
 return 0 as i64;
}

// Eligibility reason: 0 eligible, 1 mmap/checking, 2 no buffer, 3 no device,
// 4 not the device default type, 5 missing async/host/event caps, 6 no host
// buffer type, 7 allocation or backend failure.
export fn eligible(use_mmap:i64,check_tensors:i64,has_buf:i64,has_device:i64,
                   is_default:i64,has_async:i64,has_host:i64,has_events:i64,
                   has_host_buft:i64,alloc_ok:i64,backend_ok:i64,out:u64)->i64{
 var r:i64=0 as i64;
 if use_mmap!=0 as i64 || check_tensors!=0 as i64 { r=1 as i64; }
 if r==0 as i64 && has_buf==0 as i64 { r=2 as i64; }
 if r==0 as i64 && has_device==0 as i64 { r=3 as i64; }
 if r==0 as i64 && is_default==0 as i64 { r=4 as i64; }
 if r==0 as i64 {
  if has_async==0 as i64 || has_host==0 as i64 || has_events==0 as i64 { r=5 as i64; }
 }
 if r==0 as i64 && has_host_buft==0 as i64 { r=6 as i64; }
 if r==0 as i64 && (alloc_ok==0 as i64 || backend_ok==0 as i64) { r=7 as i64; }
 put(out,0 as i64,r);
 return r;
}

// Biggest-first ordering used when tensors are staged through host memory:
// staged tensors come first, and among them the larger one first. Returns 1 when
// a must precede b.
export fn before(a_staged:i64,a_bytes:i64,b_staged:i64,b_bytes:i64)->i64{
 if a_staged!=b_staged { return a_staged; }
 if a_staged!=0 as i64 && a_bytes>b_bytes { return 1 as i64; }
 return 0 as i64;
}
