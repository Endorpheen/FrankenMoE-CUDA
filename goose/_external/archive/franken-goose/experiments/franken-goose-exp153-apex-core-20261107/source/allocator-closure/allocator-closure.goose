// allocator-closure.goose - Goose port of the last native-owned allocator
// control loop reachable in the Apex CPU scheduler path.
//
// Scope (worker-8, APX014/015 remainder):
//   inputs/active-ggml-alloc.c:ggml_gallocr_reserve_n_size  size loop.
// Everything else in that file is already routed through the qualified parent
// Goose kernels (buffers, memory, graph-allocator, buffer-lifecycle) or is a
// pure forwarder with no owned loop:
//   - tallocr_new/alloc                -> buffers kernel
//   - dyn_tallocr new/reset/free/alloc -> memory kernel
//   - dyn_tallocr_free_bytes/coalesce  -> memory kernel (fg_selfhost_release_block)
//   - vbuffer alloc/free/reset/size    -> memory kernel
//   - gallocr new/free/limits/needs/
//     alloc_graph/init_tensor/extra    -> graph-allocator kernel
//   - reserve_n_impl                   -> graph-allocator kernel reserve()
//
// This module must not duplicate those. It only owns:
//   1. reserve_n_size per-buffer sum (mode 0),
//   2. the aligned_offset helper (mode 7),
//   3. the op_can_inplace classification contract (mode 6).
// Values 2 and 3 are the exact tables the native allocator uses; they are
// verified for the whole enum range against the qualified runtime by
// allocator-closure-fixture.c, so the Goose allocator path cannot silently
// drift from the native reuse policy.
//
// Typed primitives: only galloc/tallocr field reads, size_t array writes and
// the already-qualified memory kernel max_size accessor (mode 4). No native
// owned loop remains here.

extern "fg153_ac_nbuffers" fn nbuffers(g:i64)->i64;
extern "fg153_ac_talloc" fn talloc(g:i64,i:i64)->i64;
extern "fg153_ac_nchunks" fn nchunks(t:i64)->i64;
extern "fg153_ac_sizeget" fn sizeget(a:i64,i:i64)->i64;
extern "fg153_ac_sizeset" fn sizeset(a:i64,i:i64,v:i64);
extern "fg153_ac_witness" fn witness(mode:i64);
extern "fg153_memory" fn memory(mode:i64,a:i64,b:i64,c:i64,d:i64)->i64;

fn ensure(ok:bool) {if !ok {witness(255);assert(false);}}
fn add(a:i64,b:i64)->i64 {ensure(a>=0&&b>=0&&a<=9223372036854775807-b);return a+b;}

// ggml_gallocr_reserve_n_size: sizes[i] = sum over chunks of max_size.
fn buffer_sizes(g:i64,out:i64) {
    ensure(g!=0&&out!=0);
    let count=nbuffers(g);
    ensure(count>=0&&count<=64);
    for i in count {
        let allocator=talloc(g,i);
        let chunks=nchunks(allocator);
        ensure(chunks>=0&&chunks<=16);
        var total:i64=0;
        for c in chunks {
            let chunk_size=memory(4,allocator,c,0,0);
            ensure(chunk_size>=0);
            total=add(total,chunk_size);
        }
        sizeset(out,i,total);
    }
}

// aligned_offset from inputs/active-ggml-alloc.c.
fn aligned(base:i64,offset:i64,alignment:i64)->i64 {
    ensure(alignment>0&&(alignment&(alignment-1))==0);
    ensure(base>=0&&offset>=0&&base<=9223372036854775807-offset);
    let absolute=add(base,offset);
    let remainder=absolute%alignment;
    let adjust=if remainder==0 {0}else{alignment-remainder};
    return add(offset,adjust);
}

// ggml_op_can_inplace: exact numeric set from inputs/active-ggml-alloc.c.
fn inplace(op:i64)->bool {
    return op==73||op==32||op==45||op==44||op==2||op==3||op==4||op==6||op==7
        ||op==8||op==9||op==10||op==11||op==91||op==48||op==49||op==23||op==25
        ||op==26||op==50||op==46||op==47;
}

fn entry(mode:i64,a:i64,b:i64,c:i64,d:i64,e:i64,f:i64)->i64 {
    witness(mode);
    if mode==0 {buffer_sizes(a,b);return 0;}
    if mode==1 {return nbuffers(a);}
    if mode==2 {return talloc(a,b);}
    if mode==3 {return nchunks(a);}
    if mode==4 {ensure(a!=0);return memory(4,a,b,0,0);}
    if mode==5 {return sizeget(a,b);}
    if mode==6 {if inplace(a) {return 1;}return 0;}
    if mode==7 {return aligned(a,b,c);}
    if mode==8 {sizeset(a,b,c);return 0;}
    ensure(false);
    return 0;
}

fn main() {print(entry(6,2,0,0,0,0,0));}
