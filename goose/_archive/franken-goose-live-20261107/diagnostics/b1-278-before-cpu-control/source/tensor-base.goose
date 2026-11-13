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
fn high(a:i64,b:i64)->i64 {return if a>b {a} else {b};}
fn add_size(a:i64,b:i64)->i64 {assert(a>=0&&b>=0&&a<=0x7fffffffffffffff-b);return a+b;}
fn mul_size(a:i64,b:i64)->i64 {assert(a>=0&&b>=0);assert(a==0||b<=0x7fffffffffffffff/a);return a*b;}
fn scalar_bytes(kind:i64)->i64 {if kind==0||kind==26 {return 4;}if kind==1||kind==30||kind==25 {return 2;}if kind==24 {return 1;}if kind==27||kind==28 {return 8;}return 0;}
fn scalar_type(t:const Tensor&)->bool {return scalar_bytes(t.kind)>0;}
fn elements(t:const Tensor&)->i64 {var n:i64=1;for axis in 4 {n=mul_size(n,t.ne[axis]);}return n;}
fn rows(t:const Tensor&)->i64 {return mul_size(mul_size(t.ne[1],t.ne[2]),t.ne[3]);}
fn validate(t:const Tensor&,available:i64) {
    assert(t.bytes>=0&&t.bytes<=available);let bytes=scalar_bytes(t.kind);assert(bytes>0);
    var last:i64=bytes;var empty:bool=false;
    for axis in 4 {assert(t.ne[axis]>=0&&t.nb[axis]>=0);if t.ne[axis]==0 {empty=true;}else {last=add_size(last,mul_size(t.ne[axis]-1,t.nb[axis]));}}
    assert(empty||last<=t.bytes);
}
fn offset(t:const Tensor&,index:i64)->i64 {
    assert(index>=0&&index<elements(t));var n=index;var result:i64=0;
    for axis in 4 {assert(t.ne[axis]>0);result=add_size(result,mul_size(n%t.ne[axis],t.nb[axis]));n/=t.ne[axis];}
    return result;
}
fn broadcast(t:const Tensor&,d:const Tensor&,index:i64)->i64 {
    var n=index;var result:i64=0;
    for axis in 4 {assert(t.ne[axis]>0&&d.ne[axis]>0&&d.ne[axis]%t.ne[axis]==0);let c=n%d.ne[axis];n/=d.ne[axis];result=add_size(result,mul_size(c%t.ne[axis],t.nb[axis]));}
    return result;
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
