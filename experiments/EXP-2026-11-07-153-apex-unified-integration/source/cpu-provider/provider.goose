// APX013 worker-6 CPU numerical provider.
// Closes the remaining reachable normal-Apex CPU numerical path:
//   * GET_ROWS for Q4_K/Q5_K/Q6_K (256-element super-blocks, arbitrary nb[0])
//   * GET_ROWS for F32/F16/BF16 scalar rows and the I32 raw-copy exception
//   * GLU for all seven ggml_glu_op values, split and fused, swapped layouts
//   * CPY/DUP/CONT scalar conversions and quantised -> F32 conversion
// The ABI is the existing fg143 node/tensor layout. Control and dispatch stay
// in provider-dispatch.c; the arithmetic here is Goose.
import .codec.decode;

struct Tensor { ne:i64[4], nb:i64[4], kind:i64, bytes:i64 }
struct Node { x:Tensor, y:Tensor, z:Tensor, d:Tensor, op:i64, ith:i64, nth:i64, ip:i64[16], fp:f32[8] }

extern "fg143_bits" fn bits(x:f32)->u32;
extern "fg143_from_bits" fn from_bits(x:u32)->f32;
extern "fg143_half" fn half(x:u16)->f32;
extern "fg143_to_half" fn to_half(x:f32)->u16;
extern "fabsf" fn abs32(x:f32)->f32;
extern "expf" fn exp32(x:f32)->f32;
extern "expm1f" fn expm132(x:f32)->f32;
extern "logf" fn log32(x:f32)->f32;
extern "tanhf" fn tanh32(x:f32)->f32;
extern "sqrtf" fn sqrt32(x:f32)->f32;
extern "sinf" fn sin32(x:f32)->f32;
extern "cosf" fn cos32(x:f32)->f32;
extern "floorf" fn floor32(x:f32)->f32;
extern "ceilf" fn ceil32(x:f32)->f32;
extern "roundf" fn round32(x:f32)->f32;
extern "truncf" fn trunc32(x:f32)->f32;
extern "fminf" fn min32(a:f32,b:f32)->f32;
extern "fmaxf" fn max32(a:f32,b:f32)->f32;
extern "fmaf" fn fma32(a:f32,b:f32,c:f32)->f32;
extern "erff" fn erf32(x:f32)->f32;
extern "fg143_witness" fn witness(op:i64,values:i64);

fn low(a:i64,b:i64)->i64 {return if a<b {a} else {b};}
fn add_size(a:i64,b:i64)->i64 {assert(a>=0&&b>=0&&a<=0x7fffffffffffffff-b);return a+b;}
fn mul_size(a:i64,b:i64)->i64 {assert(a>=0&&b>=0);assert(a==0||b<=0x7fffffffffffffff/a);return a*b;}
fn scalar_bytes(kind:i64)->i64 {if kind==0||kind==26 {return 4;}if kind==1||kind==30||kind==25 {return 2;}if kind==24 {return 1;}if kind==27||kind==28 {return 8;}return 0;}
fn scalar_type(t:const Tensor&)->bool {return scalar_bytes(t.kind)>0;}
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
fn read16(a:const u8[:],i:i64)->u16 {assert(i>=0&&i<=a.len-2);return (a[i] as u16)|((a[i+1] as u16)<<8);}
fn read32(a:const u8[:],i:i64)->u32 {assert(i>=0&&i<=a.len-4);return (a[i] as u32)|((a[i+1] as u32)<<8)|((a[i+2] as u32)<<16)|((a[i+3] as u32)<<24);}
fn write16(a:u8[:],i:i64,v:u16) {assert(i>=0&&i<=a.len-2);a[i]=v as! u8;a[i+1]=(v>>8) as! u8;}
fn write32(a:u8[:],i:i64,v:u32) {assert(i>=0&&i<=a.len-4);a[i]=v as! u8;a[i+1]=(v>>8) as! u8;a[i+2]=(v>>16) as! u8;a[i+3]=(v>>24) as! u8;}
fn load_at(t:const Tensor&,a:const u8[:],i:i64)->f32 {
    if t.kind==0 {return from_bits(read32(a,i));}
    if t.kind==1 {return half(read16(a,i));}
    if t.kind==30 {return from_bits((read16(a,i) as u32)<<16);}
    if t.kind==26 {return (read32(a,i) as! i32) as f32;}
    if t.kind==25 {return (read16(a,i) as! i16) as f32;}
    assert(t.kind==24&&i>=0&&i<a.len);return (a[i] as! i8) as f32;
}
fn trunc_i32(x:f32)->i32 {
    let u=bits(x);let magnitude=u&0x7fffffff;let exponent=((magnitude>>23)&255) as i64;
    if exponent<127 {return 0;}
    assert(exponent<=158);var v:u32=(magnitude&0x7fffff)|0x800000;
    if exponent>=150 {v=v<<((exponent-150) as u32);}else {v=v>>((150-exponent) as u32);}
    if u>>31!=0 {assert(v<=0x80000000);return ((0 as u32)-v) as! i32;}
    assert(v<=0x7fffffff);return v as i32;
}
fn store_at(t:const Tensor&,a:u8[:],i:i64,x:f32) {
    if t.kind==0 {write32(a,i,bits(x));}
    else if t.kind==1 {write16(a,i,to_half(x));}
    else if t.kind==30 {
        let u=bits(x);let v=if (u&0x7fffffff)>0x7f800000 {((u>>16)|64) as! u16} else {((u+(32767+((u>>16)&1)))>>16) as! u16};write16(a,i,v);
    }else if t.kind==26 {write32(a,i,trunc_i32(x) as! u32);}
    else if t.kind==25 {write16(a,i,trunc_i32(x) as! u16);}
    else {assert(t.kind==24&&i>=0&&i<a.len);a[i]=trunc_i32(x) as! u8;}
}
fn load(t:const Tensor&,a:const u8[:],index:i64)->f32 {return load_at(t,a,offset(t,index));}
fn store(t:const Tensor&,a:u8[:],index:i64,value:f32) {store_at(t,a,offset(t,index),value);}
fn thread_first(p:const Node&)->i64 {assert(p.nth>0&&p.ith>=0&&p.ith<p.nth);return p.ith;}
fn same_shape(a:const Tensor&,b:const Tensor&)->bool {for i in 4 {if a.ne[i]!=b.ne[i] {return false;}}return true;}
fn read_index(t:const Tensor&,a:const u8[:],i:i64)->i64 {
    let at=offset(t,i);if t.kind==26 {return (read32(a,at) as! i32) as i64;}
    assert(t.kind==27&&at>=0&&at<=a.len-8);var u:u64=0;for b in 8 {u=u|((a[at+b] as u64)<<((8*b) as u64));}return u as! i64;
}

// ------------------------------- GET_ROWS ----------------------------------
fn get_rows_i32_raw(p:const Node&,x:const u8[:],ids:const u8[:],dst:u8[:])->bool {
    if p.d.kind!=26||p.x.kind!=26 {return false;}
    let width=p.x.ne[0];
    if width<=0||p.d.ne[0]!=width {return false;}
    if p.x.ne[2]!=p.y.ne[1]||p.x.ne[3]!=p.y.ne[2]||p.y.ne[3]!=1||elements(p.y)!=rows(p.d) {return false;}
    if p.x.nb[0]!=4||p.d.nb[0]!=4 {return false;}
    validate(p.d,dst.len);validate(p.x,x.len);validate(p.y,ids.len);
    var row=thread_first(p);
    while row<rows(p.d) {
        let i1=row%p.y.ne[0];let i2=(row/p.y.ne[0])%p.y.ne[1];let i3=row/(p.y.ne[0]*p.y.ne[1]);
        let id=read_index(p.y,ids,row);assert(id>=0&&id<p.x.ne[1]);
        let source=id*p.x.nb[1]+i2*p.x.nb[2]+i3*p.x.nb[3];
        for col in width {
            let s=source+col*4;let t=offset(p.d,row*width+col);
            assert(s>=0&&s+4<=x.len&&t>=0&&t+4<=dst.len);
            for b in 4 {dst[t+b]=x[s+b];}
        }
        row+=p.nth;
    }
    witness(40,rows(p.d));return true;
}

fn get_rows_scalar(p:const Node&,x:const u8[:],ids:const u8[:],dst:u8[:])->bool {
    if p.d.kind!=0 {return false;}
    if p.x.kind!=0&&p.x.kind!=1&&p.x.kind!=30 {return false;}
    let width=p.x.ne[0];let bytes=scalar_bytes(p.x.kind);
    if width<=0||p.d.ne[0]!=width {return false;}
    if p.x.ne[2]!=p.y.ne[1]||p.x.ne[3]!=p.y.ne[2]||p.y.ne[3]!=1||elements(p.y)!=rows(p.d) {return false;}
    if p.x.nb[0]!=bytes {return false;}
    validate(p.d,dst.len);validate(p.x,x.len);validate(p.y,ids.len);
    var row=thread_first(p);
    while row<rows(p.d) {
        let i1=row%p.y.ne[0];let i2=(row/p.y.ne[0])%p.y.ne[1];let i3=row/(p.y.ne[0]*p.y.ne[1]);
        let id=read_index(p.y,ids,row);assert(id>=0&&id<p.x.ne[1]);
        let source=id*p.x.nb[1]+i2*p.x.nb[2]+i3*p.x.nb[3];
        for col in width {
            let value=load_at(p.x,x,source+col*bytes);
            store(p.d,dst,row*width+col,value);
        }
        row+=p.nth;
    }
    witness(40,rows(p.d));return true;
}

fn get_rows_quant(p:const Node&,x:const u8[:],ids:const u8[:],dst:u8[:],scratch:f32[:],raw:u8[:])->bool {
    if p.d.kind!=0 {return false;}
    let kind=decode::super_kind(p.x.kind);
    if kind<0 {return false;}
    let f=decode::tile_traits(kind);if f.bytes<=0 {return false;}
    let qk=256;let width=p.x.ne[0];
    if width<=0||width%qk!=0||p.d.ne[0]!=width {return false;}
    if p.x.ne[2]!=p.y.ne[1]||p.x.ne[3]!=p.y.ne[2]||p.y.ne[3]!=1||elements(p.y)!=rows(p.d) {return false;}
    let blocks=width/qk;let rowbytes=mul_size(blocks,f.bytes);
    if p.x.bytes<=0||p.x.bytes>x.len {return false;}
    if scratch.len<mul_size(width,p.nth)||raw.len<mul_size(rowbytes,p.nth) {return false;}
    validate(p.d,dst.len);validate(p.y,ids.len);
    var row=thread_first(p);
    while row<rows(p.d) {
        let i1=row%p.y.ne[0];let i2=(row/p.y.ne[0])%p.y.ne[1];let i3=row/(p.y.ne[0]*p.y.ne[1]);
        let id=read_index(p.y,ids,row);assert(id>=0&&id<p.x.ne[1]);
        let source=id*p.x.nb[1]+i2*p.x.nb[2]+i3*p.x.nb[3];
        let out=scratch[(p.ith*width)..((p.ith+1)*width)];
        if p.x.nb[0]==f.bytes {
            assert(source>=0&&source+rowbytes<=x.len);
            assert(decode::row_entry(p.x.kind,x[source..source+rowbytes],out));
        } else {
            let temp=raw[(p.ith*rowbytes)..((p.ith+1)*rowbytes)];
            for b in blocks {
                let base=source+b*p.x.nb[0];assert(base>=0&&base+f.bytes<=x.len);
                for k in f.bytes {temp[b*f.bytes+k]=x[base+k];}
            }
            assert(decode::row_entry(p.x.kind,temp,out));
        }
        for col in width {store(p.d,dst,row*width+col,out[col]);}
        row+=p.nth;
    }
    witness(40,rows(p.d));return true;
}

fn get_rows(p:const Node&,x:const u8[:],ids:const u8[:],dst:u8[:],scratch:f32[:],raw:u8[:])->bool {
    if p.op!=40 {return false;}
    if get_rows_i32_raw(p,x,ids,dst) {return true;}
    if get_rows_quant(p,x,ids,dst,scratch,raw) {return true;}
    return get_rows_scalar(p,x,ids,dst);
}

// ---------------------------------- GLU ------------------------------------
fn gelu(x:f32)->f32 {return (0.5 as f32)*x*((1 as f32)+tanh32((0.7978845608028654 as f32)*x*((1 as f32)+(0.044715 as f32)*x*x)));}
fn gelu_compat(p:const Node&,x:f32)->f32 {
    if p.x.kind==0&&p.ip[13]!=0 {if x<=-(10 as f32) {return (0 as f32);}if x>=(10 as f32) {return x;}return half(to_half(gelu(half(to_half(x)))));}
    if p.x.kind==1 {return half(to_half(gelu(x)));}return gelu(x);
}
fn gelu_erf(x:f32)->f32 {return (0.5 as f32)*x*((1 as f32)+erf32(x*(0.7071067811865475 as f32)));}
fn gelu_quick(x:f32)->f32 {return x*((1 as f32)/((1 as f32)+exp32(-(1.702 as f32)*x)));}
fn silu(x:f32)->f32 {return x/((1 as f32)+exp32(-x));}
fn avx2_exp_compat(x:f32)->f32 {
    let z=fma32(x,from_bits(1069066811),12582912 as f32);
    let n=z-(12582912 as f32);
    let b=fma32(-n,from_bits(901758606),fma32(-n,from_bits(1060205056),x));
    let e=bits(z)<<23;
    let k=from_bits(((e as u64)+(0x3f800000 as u64)) as! u32);
    let u=b*b;
    let j=fma32(fma32(fma32(from_bits(1007099920),b,from_bits(1026268951)),u,fma32(from_bits(1042984755),b,from_bits(1056964315))),u,from_bits(1065353206)*b);
    if !(abs32(n)>(126 as f32)) {return fma32(j,k,k);}
    let g:u32=if n<=(0 as f32) {0x82000000 as u32}else {0 as u32};
    let s1=from_bits(((g as u64)+(0x7f000000 as u64)) as! u32);
    let s2=from_bits(((e as u64)+(0x100000000 as u64)-(g as u64)) as! u32);
    if abs32(n)>(192 as f32) {return s1*s1;}
    return fma32(s2,j,s2)*s1;
}
fn silu_compat(p:const Node&,x:f32,col:i64,width:i64)->f32 {
    if p.ip[12]!=0&&p.x.kind==0&&p.d.kind==0&&col<(width/8)*8 {
        return x/((1 as f32)+avx2_exp_compat((0 as f32)-x));
    }
    return silu(x);
}
fn gelu_quick_compat(p:const Node&,x:f32,col:i64,width:i64)->f32 {
    if p.ip[12]!=0&&p.x.kind==0&&p.d.kind==0&&col<(width/8)*8 {
        return x/((1 as f32)+avx2_exp_compat((-(1.702 as f32))*x));
    }
    return gelu_quick(x);
}
fn cpp_min(a:f32,b:f32)->f32 {return if b<a {b}else {a};}
fn cpp_max(a:f32,b:f32)->f32 {return if a<b {b}else {a};}

fn glu(p:const Node&,x:const u8[:],y:const u8[:],dst:u8[:])->bool {
    if p.op!=100 {return false;}
    let width=p.d.ne[0];let kind=p.x.kind;let bytes=scalar_bytes(kind);
    if width<=0||bytes==0||p.d.kind!=kind||p.x.nb[0]!=bytes||p.d.nb[0]!=bytes {return false;}
    if rows(p.x)!=rows(p.d) {return false;}
    let separate=p.ip[2]!=0;let swapped=p.ip[1]!=0;let op=p.ip[0];
    if op<0||op>=7 {return false;}
    if separate {
        if p.y.kind!=kind||!same_shape(p.x,p.y)||!same_shape(p.x,p.d)||p.y.nb[0]!=bytes {return false;}
    } else {
        if p.x.ne[0]!=2*width {return false;}
    }
    validate(p.d,dst.len);validate(p.x,x.len);if separate {validate(p.y,y.len);}
    var row=thread_first(p);
    while row<rows(p.d) {
        for col in width {
            let a=load(p.x,x,row*p.x.ne[0]+col+(if separate||!swapped {0} else {width}));
            let b=if separate {load(p.y,y,row*p.y.ne[0]+col)} else {load(p.x,x,row*p.x.ne[0]+col+(if swapped {0} else {width}))};
            var value:f32=(0 as f32);
            if op==0 {value=if a>(0 as f32) {a*b} else {(0 as f32)};}
            else if op==1 {value=gelu_compat(p,a)*b;}else if op==4 {value=gelu_erf(a)*b;}else if op==5 {value=gelu_quick_compat(p,a,col,width)*b;}
            else if op==2 {value=silu_compat(p,a,col,width)*b;}
            else if op==3 {let aa=cpp_min(a,p.fp[1]);let bb=cpp_max(cpp_min(b,p.fp[1]),-p.fp[1]);value=(aa/((1 as f32)+exp32(-p.fp[0]*aa)))*((1 as f32)+bb);}
            else {assert(op==6);let aa=cpp_min(a,p.fp[1]);let bb=cpp_min(cpp_max(b,-p.fp[1]),p.fp[1]);value=silu(aa)*bb;}
            store(p.d,dst,row*width+col,value);
        }
        row+=p.nth;
    }
    witness(100,rows(p.d));return true;
}

// ------------------------- CONVERSION / STRIDE -----------------------------
fn conversion_scalar(p:const Node&,x:const u8[:],dst:u8[:])->bool {
    if !scalar_type(p.x)||!scalar_type(p.d) {return false;}
    if elements(p.x)!=elements(p.d) {return false;}
    validate(p.d,dst.len);validate(p.x,x.len);
    var index=thread_first(p);let count=elements(p.d);
    if p.x.kind==p.d.kind {
        let bytes=scalar_bytes(p.x.kind);
        while index<count {let s=offset(p.x,index);let t=offset(p.d,index);for b in bytes {dst[t+b]=x[s+b];}index+=p.nth;}
        return true;
    }
    while index<count {store(p.d,dst,index,load_at(p.x,x,offset(p.x,index)));index+=p.nth;}
    return true;
}

fn conversion_from_q(p:const Node&,x:const u8[:],dst:u8[:],scratch:f32[:],raw:u8[:])->bool {
    let kind=decode::super_kind(p.x.kind);
    if kind<0||p.d.kind!=0||p.d.nb[0]!=4 {return false;}
    let f=decode::tile_traits(kind);if f.bytes<=0 {return false;}
    let qk=256;let total=elements(p.d);
    if total<=0||total%qk!=0||p.x.ne[0]%qk!=0 {return false;}
    if p.x.bytes<=0||p.x.bytes>x.len {return false;}
    if scratch.len<mul_size(qk,p.nth)||raw.len<mul_size(f.bytes,p.nth) {return false;}
    validate(p.d,dst.len);
    let blocks=total/qk;let dr=(blocks+p.nth-1)/p.nth;let first=dr*p.ith;let last=low(first+dr,blocks);
    let xne0=p.x.ne[0];let xne1=p.x.ne[1];let xne2=p.x.ne[2];
    var bi=first;
    while bi<last {
        let at=bi*qk;
        let i03=at/(xne0*xne1*xne2);let i02=(at-i03*xne0*xne1*xne2)/(xne0*xne1);let i01=(at-i03*xne0*xne1*xne2-i02*xne0*xne1)/xne0;let i00=at-i03*xne0*xne1*xne2-i02*xne0*xne1-i01*xne0;
        let source=(i00/qk)*p.x.nb[0]+i01*p.x.nb[1]+i02*p.x.nb[2]+i03*p.x.nb[3];
        assert(source>=0&&source+f.bytes<=x.len);
        let tile=raw[(p.ith*f.bytes)..((p.ith+1)*f.bytes)];
        for k in f.bytes {tile[k]=x[source+k];}
        let out=scratch[(p.ith*qk)..((p.ith+1)*qk)];
        assert(decode::row_entry(p.x.kind,tile,out));
        for lane in qk {store(p.d,dst,at+lane,out[lane]);}
        bi+=1;
    }
    witness(34,blocks);return true;
}

fn conversion(p:const Node&,x:const u8[:],dst:u8[:],scratch:f32[:],raw:u8[:])->bool {
    if p.op!=1&&p.op!=34&&p.op!=35 {return false;}
    if elements(p.x)!=elements(p.d) {return false;}
    if conversion_scalar(p,x,dst) {return true;}
    if p.d.kind==0&&scalar_bytes(p.x.kind)==0 {return conversion_from_q(p,x,dst,scratch,raw);}
    return false;
}

// ------------------------------ DISPATCH -----------------------------------
fn workspace(p:const Node&,nth:i64,info:i64[:]) {
    assert(nth>0&&info.len>=4);
    var nf:i64=0;var raw:i64=0;
    if p.op==40&&p.d.kind==0&&scalar_bytes(p.x.kind)==0 {
        let kind=decode::super_kind(p.x.kind);
        if kind>=0&&p.x.ne[0]>0&&p.x.ne[0]%256==0 {
            let f=decode::tile_traits(kind);
            if f.bytes>0 {nf=mul_size(p.x.ne[0],nth);raw=mul_size(mul_size(p.x.ne[0]/256,f.bytes),nth);}
        }
    }
    if (p.op==1||p.op==34||p.op==35)&&p.d.kind==0&&scalar_bytes(p.x.kind)==0 {
        let kind=decode::super_kind(p.x.kind);
        if kind>=0 {let f=decode::tile_traits(kind);if f.bytes>0 {nf=mul_size(256,nth);raw=mul_size(f.bytes,nth);}}
    }
    let floats=mul_size(nf,4);let aligned=mul_size(add_size(floats,7)/8,8);
    info[0]=nf;info[1]=raw;info[2]=aligned;info[3]=add_size(aligned,raw);
}

fn check_ids(p:const Node&,ids:const u8[:])->bool {
    if p.op!=40||p.y.kind!=26||p.x.ne[1]<=0 {return false;}
    for i in elements(p.y) {let id=read_index(p.y,ids,i);if id<0||id>=p.x.ne[1] {return false;}}
    return true;
}

fn entry(p:const Node&,x:const u8[:],y:const u8[:],z:const u8[:],dst:u8[:],scratch:f32[:],raw:u8[:],indices:i64[:])->bool {
    assert(p.nth>0&&p.ith>=0&&p.ith<p.nth);
    if p.ip[15]==1464816203 {workspace(p,p.nth,indices);return true;}
    if p.ip[15]==1464816204 {return check_ids(p,y);}
    if p.op==40 {return get_rows(p,x,y,dst,scratch,raw);}
    if p.op==100 {return glu(p,x,y,dst);}
    if p.op==1||p.op==34||p.op==35 {return conversion(p,x,dst,scratch,raw);}
    return false;
}

fn main() {
    var p:Node=default<Node>();p.nth=1;
    let x:u8[4]=default<u8[4]>();var d:u8[4]=default<u8[4]>();var f:f32[1]=default<f32[1]>();var r:u8[1]=default<u8[1]>();var i:i64[4]=default<i64[4]>();
    print(entry(p,x,x,x,d,f,r,i));
}
