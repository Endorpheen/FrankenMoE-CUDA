// mmap, direct-I/O and mlock decisions in Goose, from src/llama-model-loader.cpp
// (load-mode mapping, the data-offset alignment gate, the staging buffer size)
// and src/llama-mmap.cpp (read_aligned_chunk, llama_mlock::grow_to, align_range).
//
// Owned decisions:
//   - load mode to use_mmap/use_direct_io/use_mlock, with mmap dropped when the
//     platform does not support it;
//   - a mmap load requires the data section offset to be a multiple of the
//     backend alignment;
//   - a direct-I/O read is staged through an aligned window: aligned start,
//     offset inside it, rounded byte count, bounded buffer and the skip amount;
//   - the staging buffer is 64 MiB plus two alignments for direct I/O, 1 MiB
//     otherwise;
//   - mlock grows in page-aligned steps and stops for good after a failure;
//   - page alignment of an unmap range clamps an empty range to zero length.
//
// The system calls (mmap/munmap/mlock/read) stay native.
extern "fg460_st_i64" fn st_i64(p:u64,o:u64,v:i64);

fn put(out:u64,i:i64,v:i64){ st_i64(out,(i*(8 as i64)) as u64,v); }

const DIRECT_IO_BUFFER:i64 = 67108864 as i64;   // 64 MiB
const LOAD_MODE_AUTO:i64 = -1 as i64;
const LOAD_MODE_NONE:i64 = 0 as i64;
const LOAD_MODE_MMAP:i64 = 1 as i64;
const LOAD_MODE_MLOCK:i64 = 2 as i64;
const LOAD_MODE_MMAP_MLOCK:i64 = 3 as i64;
const LOAD_MODE_DIRECT_IO:i64 = 4 as i64;

// Writes use_mmap, use_direct_io, use_mlock.
export fn load_flags(mode:i64,mmap_supported:i64,out:u64)->i64{
 var mmap:i64=0 as i64;
 var dio:i64=0 as i64;
 var mlock:i64=0 as i64;
 if mode==LOAD_MODE_AUTO as i64 || mode==LOAD_MODE_MMAP as i64 || mode==LOAD_MODE_MMAP_MLOCK as i64 {
  mmap=1 as i64;
 }
 if mode==LOAD_MODE_DIRECT_IO as i64 { dio=1 as i64; }
 if mode==LOAD_MODE_MLOCK as i64 || mode==LOAD_MODE_MMAP_MLOCK as i64 { mlock=1 as i64; }
 if mmap!=0 as i64 && mmap_supported==0 as i64 { mmap=0 as i64; }
 put(out,0 as i64,mmap);
 put(out,1 as i64,dio);
 put(out,2 as i64,mlock);
 return 0 as i64;
}

// mmap needs the data section aligned to the backend alignment.
export fn align_ok(data_offset:i64,align:i64)->i64{
 if align<=0 as i64 { return 0 as i64; }
 if data_offset<0 as i64 { return 0 as i64; }
 if (data_offset % align)==0 as i64 { return 1 as i64; }
 return 0 as i64;
}

fn round_down(x:i64,align:i64)->i64{ return x & ((0 as i64)-align); }
fn round_up(x:i64,align:i64)->i64{ return (x+align-1) & ((0 as i64)-align); }

// Direct-I/O staging plan for a read at offset with size and device alignment.
// Writes aligned_offset, offset_from_alignment, bytes_to_read, buffer_size, skip.
export fn chunk_plan(offset:i64,size:i64,align:i64,out:u64)->i64{
 var a:i64=align;
 if a<1 as i64 { a=1 as i64; }
 let aligned:i64=round_down(offset,a);
 let from:i64=offset-aligned;
 let bytes:i64=round_up(from+size,a);
 var buf:i64=bytes;
 if buf>DIRECT_IO_BUFFER as i64 { buf=DIRECT_IO_BUFFER as i64; }
 put(out,0 as i64,aligned);
 put(out,1 as i64,from);
 put(out,2 as i64,bytes);
 put(out,3 as i64,buf);
 put(out,4 as i64,from);
 return 0 as i64;
}

// Staging buffer for the async path: 64 MiB plus two alignments, or 1 MiB when
// the files are buffered.
export fn staging_size(alignment:i64,out:u64)->i64{
 var n:i64=1048576 as i64;
 if alignment!=1 as i64 { n=DIRECT_IO_BUFFER+2 as i64*alignment; }
 put(out,0 as i64,n);
 return n;
}

// llama_mlock::grow_to: page-align the target, lock only the new tail, and stop
// for good after the first failure.
export fn mlock_step(failed:i64,size:i64,target:i64,granularity:i64,out:u64)->i64{
 var new_size:i64=size;
 var new_failed:i64=failed;
 var to_lock:i64=0 as i64;
 if failed==0 as i64 {
  let aligned:i64=round_up(target,granularity);
  if aligned>size { to_lock=aligned-size; new_size=aligned; }
 }
 put(out,0 as i64,new_size);
 put(out,1 as i64,to_lock);
 put(out,2 as i64,new_failed);
 return 0 as i64;
}

// Page alignment of an unmap range; an empty range stays empty.
export fn align_range(first:i64,last:i64,page:i64,out:u64)->i64{
 let offset_in_page:i64=first & (page-1 as i64);
 var f:i64=first;
 if offset_in_page!=0 as i64 { f=first+(page-offset_in_page); }
 var l:i64=last & ((0 as i64)-page);
 if l<=f { l=f; }
 put(out,0 as i64,f);
 put(out,1 as i64,l);
 return 0 as i64;
}
