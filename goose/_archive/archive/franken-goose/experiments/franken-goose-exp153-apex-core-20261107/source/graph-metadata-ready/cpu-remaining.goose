// worker-4: remaining CPU GetRows port, quantised Q4_K/Q6_K -> F32.
// Status: UNVERIFIED PROPOSAL for parent Codex. Not wired into any runtime.
// Target semantics: ggml_compute_forward_get_rows_q
//   inputs/ggml-native-ops.cpp:5288-5330 (dispatch :5474 Q4_K, :5476 Q6_K).
// GLU F32 has no remaining function here: it is already fully provided by
// inputs/cpu-goose-kernel.goose:155-174 glu() for all 7 GGML_GLU_OP_* values,
// and Apex model source uses ggml_silu+ggml_mul (inputs/apex-model.cpp:271-273)
// rather than GGML_OP_GLU, so GLU liveness is UNPROVEN and no new GLU port is
// added. The scalar GetRows path (F32/F16/BF16/I32 -> F32) is already covered
// by transforms() at inputs/cpu-goose-kernel.goose:297.

struct Tensor { ne:i64[4], nb:i64[4], kind:i64, bytes:i64 }
struct Node { x:Tensor, y:Tensor, z:Tensor, d:Tensor, op:i64, ith:i64, nth:i64, ip:i64[16], fp:f32[8] }

// ---- Exact ABI / import contract (all provided by the parent build) --------
// decode::super_kind(kind:i64) -> i64
//   >= 0 for a super-block type (Q4_K = 12, Q6_K = 14), < 0 otherwise.
// decode::tile_traits(kind:i64).bytes -> i64
//   bytes of one 256-element super-block row tile.
// decode::row_entry(kind:i64, row:const u8[:], out:f32[:],
//                   gxx:const u8[8][:], gxs:const u8[8][:], gs:const u8[8][:],
//                   g3x:const u8[4][:], g3s:const u8[4][:], g1:const i8[8][:],
//                   signs:const u8[:], palette:const i8[:]) -> bool
//   row.len must be exactly rowbytes; writes exactly `out.len` F32 values;
//   returns false on malformed input (caller must fall back to native).
// witness(op:i64, values:i64) -> void
//   instrumentation only, no semantic effect.
// bits(x:f32) -> u32
//   raw IEEE-754 bit pattern; used to store F32 without float stores.
import .codec.decode;
extern "fg143_witness" fn witness(op:i64,values:i64);
extern "fg143_bits" fn bits(x:f32)->u32;

fn mul_size(a:i64,b:i64)->i64 {assert(a>=0&&b>=0);assert(a==0||b<=0x7fffffffffffffff/a);return a*b;}
fn add_size(a:i64,b:i64)->i64 {assert(a>=0&&b>=0&&a<=0x7fffffffffffffff-b);return a+b;}
fn scalar_bytes(kind:i64)->i64 {
    if kind==0||kind==26 {return 4;}if kind==1||kind==30||kind==25 {return 2;}
    if kind==24 {return 1;}if kind==27||kind==28 {return 8;}return 0;
}
fn elements(t:const Tensor&)->i64 {var n:i64=1;for axis in 4 {n=mul_size(n,t.ne[axis]);}return n;}
fn rows(t:const Tensor&)->i64 {return mul_size(mul_size(t.ne[1],t.ne[2]),t.ne[3]);}
fn offset(t:const Tensor&,index:i64)->i64 {
    assert(index>=0&&index<elements(t));var n=index;var result:i64=0;
    for axis in 4 {assert(t.ne[axis]>0);result=add_size(result,mul_size(n%t.ne[axis],t.nb[axis]));n/=t.ne[axis];}
    return result;
}
fn validate(t:const Tensor&,available:i64) {
    assert(t.bytes>=0&&t.bytes<=available);let bytes=scalar_bytes(t.kind);assert(bytes>0);
    var last:i64=bytes;var empty:bool=false;
    for axis in 4 {assert(t.ne[axis]>=0&&t.nb[axis]>=0);if t.ne[axis]==0 {empty=true;}else {last=add_size(last,mul_size(t.ne[axis]-1,t.nb[axis]));}}
    assert(empty||last<=t.bytes);
}
fn read32(a:const u8[:],i:i64)->u32 {assert(i>=0&&i<=a.len-4);return (a[i] as u32)|((a[i+1] as u32)<<8)|((a[i+2] as u32)<<16)|((a[i+3] as u32)<<24);}
fn read_index(t:const Tensor&,a:const u8[:],i:i64)->i64 {
    let at=offset(t,i);if t.kind==26 {return (read32(a,at) as! i32) as i64;}
    assert(t.kind==27&&at>=0&&at<=a.len-8);var u:u64=0;
    for b in 8 {u=u|((a[at+b] as u64)<<((8*b) as u64));}return u as! i64;
}
fn store_f32(t:const Tensor&,a:u8[:],index:i64,value:f32) {
    assert(t.kind==0);let at=offset(t,index);assert(at>=0&&at<=a.len-4);
    let u=bits(value);a[at]=(u&255) as! u8;a[at+1]=((u>>8)&255) as! u8;
    a[at+2]=((u>>16)&255) as! u8;a[at+3]=((u>>24)&255) as! u8;
}

// Quantised row gather. Mirrors ggml_compute_forward_get_rows_q exactly:
//   i12 = i/(ne1*ne0); i11 = (i - i12*ne1*ne0)/ne0; i10 = remainder
//   id  = idx[i10 + ne0*(i11 + ne1*i12)]
//   src = id*nb01 + i11*nb02 + i12*nb03 ; dst = i10*nb1 + i11*nb2 + i12*nb3
// Threading matches the native dr block split over nr = nelements(src1).
fn get_rows_quant(p:const Node&, x:const u8[:], idx:const u8[:], dst:u8[:], scratch:f32[:],
                  gxx:const u8[8][:], gxs:const u8[8][:], gs:const u8[8][:],
                  g3x:const u8[4][:], g3s:const u8[4][:], g1:const i8[8][:],
                  signs:const u8[:], palette:const i8[:])->bool {
    if p.op!=40 {return false;}                                   // OP_GET_ROWS
    assert(p.nth>0&&p.ith>=0&&p.ith<p.nth);
    if p.d.kind!=0 {return false;}                                // output must be F32
    if scalar_bytes(p.x.kind)>0 {return false;}                   // scalar src: transforms()
    let kind=decode::super_kind(p.x.kind);
    if kind<0 {return false;}                                     // not a super-block quant
    let width=p.x.ne[0];
    assert(width>0&&width%256==0);
    assert(width==p.d.ne[0]);                                     // core: out.ne0 == src.ne0
    assert(p.x.ne[2]==p.y.ne[1]&&p.x.ne[3]==p.y.ne[2]&&p.y.ne[3]==1);
    validate(p.d,dst.len);validate(p.x,x.len);validate(p.y,idx.len);
    let rowbytes=mul_size(width/256,decode::tile_traits(kind).bytes);
    assert(rowbytes>0&&rowbytes<=p.x.bytes);
    assert(scratch.len>=mul_size(width,p.nth));
    let nr=elements(p.y);
    assert(rows(p.d)==nr);                                        // kernel: nrows(dst) == nr
    let dr=(nr+p.nth-1)/p.nth;
    let first=dr*p.ith;
    let last=if first+dr<nr {first+dr} else {nr};
    let temp=p.ith*width;
    var i=first;
    while i<last {
        let i12=i/(p.y.ne[1]*p.y.ne[0]);
        let i11=(i-i12*p.y.ne[1]*p.y.ne[0])/p.y.ne[0];
        let i10=i-i12*p.y.ne[1]*p.y.ne[0]-i11*p.y.ne[0];
        let id=read_index(p.y,idx,i10+p.y.ne[0]*(i11+p.y.ne[1]*i12));
        assert(id>=0&&id<p.x.ne[1]);
        let source=id*p.x.nb[1]+i11*p.x.nb[2]+i12*p.x.nb[3];
        assert(source>=0&&source<=p.x.bytes&&rowbytes<=p.x.bytes-source);
        let output=scratch[temp..temp+width];
        assert(decode::row_entry(p.x.kind,x[source..source+rowbytes],output,gxx,gxs,gs,g3x,g3s,g1,signs,palette));
        for col in width {store_f32(p.d,dst,i10+p.d.ne[0]*(i11+p.d.ne[1]*i12)+col,output[col]);}
        i+=1;
    }
    witness(40,nr);
    return true;
}

// ---- Test vectors (inputs only; expected values are generated at runtime by
// worker-4/cpu-apex-fixture.c, never hardcoded here) -------------------------
// V1  Q4_K w[256,8],  idx[6] = {5,0,5,2,7,0}, dst[256,6] F32
// V2  Q6_K w[256,8],  idx[5] = {7,7,1,0,3},   dst[256,5] F32
// V3  Q4_K w[256,6,2], idx = view[4,2,1] of [4,2,2]
//     used planes {4,0,4,3} and {1,5,2,1}; plane z=1 holds 99/-1 and must not
//     be read (a stray read trips the id bounds assert).
// Invariants: dst[col,i1,i2] equals the decoded src row idx[i1,i2]; results are
// byte-identical to the native kernel because both use the same row decoder.
// Deterministic input generators and seeds: see cpu-apex-fixture.md section 2.

fn main() {
    var p:Node=default<Node>();p.nth=1;
    let x:u8[4]=default<u8[4]>();var d:u8[4]=default<u8[4]>();var f:f32[1]=default<f32[1]>();
    let g8:u8[8][1]=default<u8[8][1]>();let g4:u8[4][1]=default<u8[4][1]>();
    let g1:i8[8][1]=default<i8[8][1]>();let signs:u8[128]=default<u8[128]>();let palette:i8[16]=default<i8[16]>();
    print(get_rows_quant(p,x,x,d,f,g8,g8,g8,g4,g4,g1,signs,palette));
}
