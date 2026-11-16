// Apex aligned file reads in Goose (llama_file::read_aligned_chunk, opening arithmetic).
//
// A DirectIO read must start on an alignment boundary and cover whole alignment units, while the
// caller wants exactly `size` bytes from an arbitrary offset:
//   aligned_offset  the offset rounded down to the boundary
//   skip            how far into the aligned block the wanted bytes begin
//   bytes_to_read   the rounded-up length that must be read to cover `size`
//   buffer_size     the staging buffer, capped so a large tensor is never held twice
//
// Packet:
//   k=0  0 offset, 1 size, 2 alignment, 3 cap
//   k=1  0 out (four int64)
//   k=2  0 done
//
// Records: 0 plan (a = aligned_offset, b = bytes_to_read, c = skip, d = buffer_size)
extern "fg377k_get" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg377k_put" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg377k_ld_i64" fn ld(base:u64,byte:i64)->i64;
extern "fg377k_stw" fn stw(base:u64,byte:i64,v:i64);
extern "fg377k_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn run(packet:i64)->bool{
 let offset:i64=get(packet,0,0) as i64;
 let size:i64=get(packet,0,1) as i64;
 let alignment:i64=get(packet,0,2) as i64;
 let cap:i64=get(packet,0,3) as i64;
 let out:u64=get(packet,1,0);

 ensure(alignment>0 as i64,10);
 ensure((alignment&(alignment-(1 as i64)))==(0 as i64),11);
 ensure(offset>=(0 as i64),12);
 ensure(size>=(0 as i64),13);

 let aligned_offset:i64=offset&(~(alignment-(1 as i64)));
 let offset_from_alignment:i64=offset-aligned_offset;
 let bytes_to_read:i64=(offset_from_alignment+size+alignment-(1 as i64))&(~(alignment-(1 as i64)));
 let buffer_size:i64=if bytes_to_read<cap{bytes_to_read}else{cap};

 stw(out,0 as i64,aligned_offset);
 stw(out,8 as i64,bytes_to_read);
 stw(out,16 as i64,offset_from_alignment);
 stw(out,24 as i64,buffer_size);
 put(packet,2,0,1 as u64);
 return true;
}

fn main(){run(0);}
