// CPU_REPACK control policy and scheduling for Apex Q4_K_M on x86 AVX2.
// Source: inputs/native/ggml/src/ggml-cpu/repack.cpp
//   ggml_repack_get_optimal_repack_type:4925
//   tensor_traits::work_size:4557, forward_mul_mat:4650,
//   forward_mul_mat_one_chunk:4601, forward_mul_mat_id:4783
//   extra_buffer_type::supports_op/get_tensor_traits:5190
// All tensor field reads and tensor/workspace pointer effects are primitive
// calls; the loops, guards, row mapping and chunk policy stay here.
extern "fg006_rc_k" fn input_width()->i64;
extern "fg006_rc_chunk_gemm" fn chunk_gemm(s0s:i64,s0e:i64,s1s:i64,rows:i64);
extern "fg006_rc_chunk_gemv" fn chunk_gemv(s0s:i64,s0e:i64,s1s:i64,row:i64);
fn chunk(s0s:i64,s0e:i64,s1s:i64,s1e:i64)->i64 {
    let nr=s1e-s1s;let full=nr-nr%4;
    if full>0 {chunk_gemm(s0s,s0e,s1s,full);}
    var r=full;while r<nr {chunk_gemv(s0s,s0e,s1s,r);r+=1;}
    return 0;
}
extern "fg006_rc_chunk_set" fn chunk_set(value:i64);
extern "fg006_rc_chunk_add" fn chunk_add(delta:i64)->i64;
extern "fg006_rc_barrier" fn barrier();
extern "fg006_rc_quant4" fn quant4(i12:i64,i11:i64,ne10:i64);
extern "fg006_rc_quant1" fn quant1(i12:i64,i11:i64,ne10:i64);
extern "fg006_rc_rows_clear" fn rows_clear(n_as:i64);
extern "fg006_rc_ids_get" fn ids_get(iid1:i64,id:i64)->i32;
extern "fg006_rc_row_count" fn row_count(expert:i64)->i64;
extern "fg006_rc_row_set" fn row_set(expert:i64,index:i64,i1:i64,i2:i64);
extern "fg006_rc_row_inc" fn row_inc(expert:i64);
extern "fg006_rc_row_i1" fn row_i1(expert:i64,index:i64)->i64;
extern "fg006_rc_row_i2" fn row_i2(expert:i64,index:i64)->i64;
extern "fg006_rc_mmid_gemv" fn mmid_gemv(expert:i64,i1:i64,i2:i64,i11:i64,s0s:i64,s0e:i64);
extern "fg006_rc_witness" fn witness(mode:i64,value:i64);

struct Plan { nchunk0:i64, nchunk1:i64, dr0:i64 }
struct Bounds { ok:bool, s0s:i64, s0e:i64, s1s:i64, s1e:i64 }

fn min64(a:i64,b:i64)->i64 {return if a<b {a} else {b};}
fn max64(a:i64,b:i64)->i64 {return if a>b {a} else {b};}
fn align8(x:i64)->i64 {return if x%8!=0 {x+8-x%8} else {x};}

fn select_q4_K(ne1:i64,avx2:i64,neon:i64,matmul_int8:i64,dotprod:i64)->i64 {
    assert(ne1>=0);
    if avx2!=0&&ne1%8==0 {return 1;}
    if neon!=0&&matmul_int8!=0&&ne1%8==0 {return 1;}
    if neon!=0&&dotprod!=0&&ne1%8==0 {return 2;}
    return 0;
}

fn row_size_q8(nelements:i64)->i64 {
    assert(nelements>=0&&nelements%256==0);
    return (nelements/256)*292;
}
fn work_size(is_id:i64,ne10:i64,ne11:i64,ne12:i64,ne02:i64,ne13:i64)->i64 {
    assert(ne10>0&&ne11>=0&&ne12>=0&&ne02>=0&&ne10%256==0);
    let n13=if ne13==0 {1} else {ne13};
    let ne=ne10*ne11*ne12*n13;
    let base=row_size_q8(ne);
    if is_id==0 {return base;}
    let padded=if base%8!=0 {base+8-base%8} else {base};
    return padded+8*ne02*(ne12+1);
}

fn supports(is_mul_mat:i64,is_mul_mat_id:i64,dims:i64,repack_buft:i64,has_traits:i64,
            src1_nonhost:i64,src1_f32:i64)->bool {
    if repack_buft==0||has_traits==0||src1_nonhost!=0 {return false;}
    if is_mul_mat!=0&&dims==2 {return src1_f32!=0;}
    if is_mul_mat_id!=0&&dims==3 {return src1_f32!=0;}
    return false;
}
fn get_traits(is_mul_mat:i64,is_mul_mat_id:i64,repack_buft:i64,has_extra:i64)->bool {
    if repack_buft==0||has_extra==0 {return false;}
    return is_mul_mat!=0||is_mul_mat_id!=0;
}
fn provider(index:i64,amx:i64,riscv:i64,kleidi:i64,repack:i64)->i64 {
    if index==0&&amx!=0 {return 1;}
    if index==1&&riscv!=0 {return 2;}
    if index==2&&kleidi!=0 {return 3;}
    if index==3&&repack!=0 {return 4;}
    return 0;
}

fn plan(nr0:i64,nth:i64,ne12:i64,numa:i64)->Plan {
    assert(nr0>0&&nth>0&&ne12>0);
    let nth_scaled=nth*4;
    let chunk_size0=(nr0+nth_scaled-1)/nth_scaled;
    var nchunk0=(nr0+chunk_size0-1)/chunk_size0;
    let min_chunk:i64=8;
    if nchunk0>0&&(nr0/nchunk0)<min_chunk&&nr0>=min_chunk {nchunk0=(nr0+min_chunk-1)/min_chunk;}
    var dr0=(nr0+nchunk0-1)/nchunk0;
    if nth==1||((nchunk0<nth||numa!=0)&&(nr0+nth-1)/nth>=min_chunk) {
        nchunk0=nth;dr0=(nr0+nchunk0-1)/nchunk0;
    }
    let max_nchunk=(nr0+min_chunk-1)/min_chunk;
    nchunk0=min64(nchunk0,max_nchunk);
    return Plan{nchunk0:nchunk0,nchunk1:ne12,dr0:dr0};
}
fn bounds(p:Plan,current:i64,ne01:i64,ne11:i64)->Bounds {
    assert(p.nchunk0>0&&current>=0&&current<p.nchunk0*p.nchunk1);
    let ith0=current%p.nchunk0;
    let ith1=current/p.nchunk0;
    var s0s=p.dr0*ith0;
    var s0e=min64(s0s+p.dr0,ne01);
    let s1s=ith1*ne11;
    let s1e=(ith1+1)*ne11;
    s0s=align8(s0s);s0e=align8(s0e);s0e=min64(s0e,ne01);
    return Bounds{ok:s0s<s0e,s0s:s0s,s0e:s0e,s1s:s1s,s1e:s1e};
}

fn quantize_rows(ith:i64,nth:i64,ne11:i64,ne12:i64,ne10:i64) {
    assert(ith>=0&&ith<nth&&ne11>=0&&ne12>=0&&ne10>0);
    var i12:i64=0;
    while i12<ne12 {
        var i11:i64=ith*4;
        while i11<ne11-ne11%4 {quant4(i12,i11,ne10);i11+=nth*4;}
        let processed=ne11-ne11%4;
        var tail:i64=processed+ith;
        while tail<ne11 {quant1(i12,tail,ne10);tail+=nth;}
        i12+=1;
    }
    witness(1,ne12*ne11);
}

fn run_forward(ith:i64,nth:i64,nr0:i64,ne01:i64,ne11:i64,ne12:i64,numa:i64)->i64 {
    let p=plan(nr0,nth,ne12,numa);
    if ith==0 {chunk_set(nth);}
    barrier();
    var current=ith;var chunks:i64=0;
    while current<p.nchunk0*p.nchunk1 {
        let b=bounds(p,current,ne01,ne11);
        if b.ok {chunk(b.s0s,b.s0e,b.s1s,b.s1e);chunks+=1;}
        current=chunk_add(1);
    }
    witness(2,chunks);return chunks;
}

fn quantize_mmid(ith:i64,nth:i64,ne11:i64,ne12:i64,ne10:i64) {
    assert(ith>=0&&ith<nth&&ne11>=0&&ne12>=0&&ne10>0);
    var i12:i64=0;
    while i12<ne12 {
        var i11:i64=ith;
        while i11<ne11 {quant1(i12,i11,ne10);i11+=nth;}
        i12+=1;
    }
    witness(1,ne12*ne11);
}

fn run_mmid(ith:i64,nth:i64,ne01:i64,ne11:i64,ne12:i64,ne02:i64,n_ids:i64,ids_ne1:i64)->i64 {
    assert(ith>=0&&ith<nth&&ne01>0&&ne11>0&&ne12>0&&ne02>0&&n_ids>0&&ids_ne1>=0);
    quantize_mmid(ith,nth,ne11,ne12,input_width());
    if ith==0 {
        rows_clear(ne02);
        var iid1:i64=0;
        while iid1<ids_ne1 {
            var id:i64=0;
            while id<n_ids {
                let expert=ids_get(iid1,id) as i64;
                assert(expert>=0&&expert<ne02);
                let index=row_count(expert);
                row_set(expert,index,id,iid1);
                row_inc(expert);
                id+=1;
            }
            iid1+=1;
        }
    }
    barrier();
    var cur_a:i64=0;var rows:i64=0;
    while cur_a<ne02 {
        let cne1=row_count(cur_a);
        if cne1>0 {
            var s0s=(ith*ne01)/nth;
            var s0e=((ith+1)*ne01)/nth;
            s0s=align8(s0s);s0e=align8(s0e);
            if s0e>ne01 {s0e=ne01;}
            if s0s<s0e {
                var ir1:i64=0;
                while ir1<cne1 {
                    let id=row_i1(cur_a,ir1);
                    let i12=row_i2(cur_a,ir1);
                    let i11=id%ne11;
                    mmid_gemv(cur_a,id,i12,i11,s0s,s0e);
                    rows+=1;ir1+=1;
                }
            }
        }
        cur_a+=1;
    }
    witness(3,rows);return rows;
}

fn entry(mode:i64,a:i64,b:i64,c:i64,d:i64,e:i64,f:i64,g:i64,h:i64)->i64 {
    if mode==0 {return select_q4_K(a,b,c,d,e);}
    if mode==1 {return work_size(a,b,c,d,e,f);}
    if mode==2 {return provider(a,b,c,d,e);}
    if mode==3 {return if supports(a,b,c,d,e,f,g) {1}else {0};}
    if mode==4 {return if get_traits(a,b,c,d) {1}else {0};}
    if mode==5 {return run_forward(a,b,c,d,e,f,g);}
    if mode==6 {return run_mmid(a,b,c,d,e,f,g,h);}
    if mode==7 {
        let p=plan(a,b,c,d);
        if e==0 {return p.nchunk0;}
        if e==1 {return p.nchunk1;}
        return p.dr0;
    }
    if mode==8 {
        let p=Plan{nchunk0:a,nchunk1:b,dr0:c};
        let q=bounds(p,d,e,f);
        if g==0 {return if q.ok {1}else {0};}
        if g==1 {return q.s0s;}
        if g==2 {return q.s0e;}
        if g==3 {return q.s1s;}
        return q.s1e;
    }
    assert(mode==9);
    quantize_rows(a,b,c,d,e);return 0;
}
fn main() {
    let p=plan(8,1,1,0);
    let b=bounds(p,0,8,4);
    let z=entry(0,8,1,0,0,0,0,0,0);
    print(select_q4_K(8,1,0,0,0)+work_size(0,512,4,1,1,1)+if b.ok {1}else {0}+z);
}
