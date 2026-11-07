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

fn gelu(x:f32)->f32 {return (0.5 as f32)*x*((1 as f32)+tanh32((0.7978845608028654 as f32)*x*((1 as f32)+(0.044715 as f32)*x*x)));}
fn gelu_compat(p:const Node&,x:f32)->f32 {
    if p.x.kind==0&&p.ip[13]!=0 {if x<=-(10 as f32) {return (0 as f32);}if x>=(10 as f32) {return x;}return half(to_half(gelu(half(to_half(x)))));}
    if p.x.kind==1 {return half(to_half(gelu(x)));}return gelu(x);
}
fn gelu_erf(x:f32)->f32 {return (0.5 as f32)*x*((1 as f32)+erf32(x*(0.7071067811865475 as f32)));}
fn gelu_quick(x:f32)->f32 {return x/((1 as f32)+exp32(-(1.702 as f32)*x));}
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

fn unary_value(p:const Node&,op:i64,x:f32)->f32 {
    if op==0 {return abs32(x);}
    if op==1 {return if x>(0.0 as f32) {(1.0 as f32)} else if x<(0.0 as f32) {-(1.0 as f32)} else {(0.0 as f32)};}
    if op==2 {return -x;}
    if op==3 {return if x>(0.0 as f32) {(1.0 as f32)} else {(0.0 as f32)};}
    if op==4 {return tanh32(x);}
    if op==5 {return if x>(0.0 as f32) {x} else {expm132(x)};}
    if op==6 {return if x>(0.0 as f32) {x} else {(0.0 as f32)};}
    if op==7 {return (1.0 as f32)/((1.0 as f32)+exp32(-x));}
    if op==12 {return min32((1.0 as f32),max32((0.0 as f32),(x+(3.0 as f32))/(6.0 as f32)));}
    if op==13 {return exp32(x);}
    if op==11 {return x*min32((1.0 as f32),max32((0.0 as f32),(x+(3.0 as f32))/(6.0 as f32)));}
    if op==14 {return exp32(x)-(1.0 as f32);}
    if op==15 {return if x>(20.0 as f32) {x} else {log32((1.0 as f32)+exp32(x))};}
    if op==18 {return floor32(x);}
    if op==19 {return ceil32(x);}
    if op==20 {return round32(x);}
    if op==21 {return trunc32(x);}
    if op==17 {return if x>(0.0 as f32) {fma32(p.fp[1]*x,x,p.fp[2]*x)} else {fma32(expm132(min32(x,p.fp[3]))-x,p.fp[0],p.fp[2]*x)};}
    if op==8 {return gelu_compat(p,x);}if op==16 {return gelu_erf(x);}if op==9 {return gelu_quick(x);}assert(op==10);return silu(x);
}
fn pointwise(p:const Node&,x:const u8[:],y:const u8[:],dst:u8[:])->bool {
    if p.op!=2&&p.op!=4&&p.op!=6&&p.op!=7&&p.op!=8&&p.op!=32&&p.op!=9&&p.op!=10&&p.op!=12&&p.op!=13&&p.op!=11&&p.op!=50&&p.op!=71&&p.op!=91&&p.op!=23 {return false;}
    var i=thread_first(p);let count=elements(p.d);
    while i<count {
        let a=load_at(p.x,x,broadcast(p.x,p.d,i));var value:f32=(0 as f32);
        if p.op==2||p.op==4||p.op==6||p.op==7||p.op==8||p.op==23 {
            let b=load_at(p.y,y,broadcast(p.y,p.d,i));
            if p.op==2||p.op==4 {value=a+b;}
            else if p.op==6 {value=a-b;}else if p.op==7 {value=a*b;}else if p.op==8 {value=a/b;}
            else {let sig=(1 as f32)/((1 as f32)+exp32(-b));value=a*sig*((1 as f32)+b*((1 as f32)-sig));}
        }else if p.op==32 {value=a*p.fp[0]+p.fp[1];}
        else if p.op==9 {value=a*a;}else if p.op==10 {value=sqrt32(a);}
        else if p.op==12 {value=sin32(a);}else if p.op==13 {value=cos32(a);}else if p.op==11 {value=log32(a);}
        else if p.op==50 {value=min32(max32(a,p.fp[0]),p.fp[1]);}
        else if p.op==71 {value=if a>(0 as f32) {a} else {a*p.fp[0]};}
        else {value=unary_value(p,p.ip[0],a);}
        store(p.d,dst,i,value);i+=p.nth;
    }
    return true;
}
fn cpp_min(a:f32,b:f32)->f32 {return if b<a {b}else {a};}
fn cpp_max(a:f32,b:f32)->f32 {return if a<b {b}else {a};}
fn glu(p:const Node&,x:const u8[:],y:const u8[:],dst:u8[:])->bool {
    if p.op!=100 {return false;}let width=p.d.ne[0];assert(width>0&&rows(p.x)==rows(p.d));let separate=p.ip[2]!=0;let swapped=p.ip[1]!=0;
    if separate {assert(same_shape(p.x,p.y)&&same_shape(p.x,p.d));}else {assert(p.x.ne[0]==2*width);}
    var row=thread_first(p);
    while row<rows(p.d) {
        for col in width {
            let a=load(p.x,x,row*p.x.ne[0]+col+(if separate||!swapped {0} else {width}));
            let b=if separate {load(p.y,y,row*p.y.ne[0]+col)} else {load(p.x,x,row*p.x.ne[0]+col+(if swapped {0} else {width}))};
            let op=p.ip[0];var value:f32=(0 as f32);
            if op==0 {value=(if a>(0 as f32) {a} else {(0 as f32)})*b;}
            else if op==1 {value=gelu_compat(p,a)*b;}else if op==4 {value=gelu_erf(a)*b;}else if op==5 {value=gelu_quick(a)*b;}
            else if op==2 {value=silu_compat(p,a,col,width)*b;}
            else if op==3 {let aa=cpp_min(a,p.fp[1]);let bb=cpp_max(cpp_min(b,p.fp[1]),-p.fp[1]);value=(aa/((1 as f32)+exp32(-p.fp[0]*aa)))*((1 as f32)+bb);}
            else {assert(op==6);let aa=cpp_min(a,p.fp[1]);let bb=cpp_min(cpp_max(b,-p.fp[1]),p.fp[1]);value=silu(aa)*bb;}
            store(p.d,dst,row*width+col,value);
        }
        row+=p.nth;
    }
    return true;
}

fn reduction(p:const Node&,x:const u8[:],y:const u8[:],dst:u8[:])->bool {
    if p.op!=14&&p.op!=15&&p.op!=17&&p.op!=16&&p.op!=18&&p.op!=19 {return false;}
    if p.op==14 {
        if p.ith==0 {var total:f64=(0 as f64);for row in rows(p.x) {var value:f64=(0 as f64);for col in p.x.ne[0] {value+=load(p.x,x,row*p.x.ne[0]+col) as f64;}total+=value;}store(p.d,dst,0,total as f32);}return true;
    }
    if p.op==19 {
        assert(p.x.kind==26&&p.y.kind==26&&same_shape(p.x,p.y));
        if p.ith==0 {var total:i64=0;for i in elements(p.x) {if read32(x,offset(p.x,i))==read32(y,offset(p.y,i)) {total+=1;}}assert(p.d.bytes>=8);let u=total as u64;for i in 8 {dst[i]=(u>>((8*i) as u64)) as! u8;}}return true;
    }
    var row=thread_first(p);let width=p.x.ne[0];
    while row<rows(p.x) {
        if p.op==18 {assert(width>0);var at:i64=0;var best=load(p.x,x,row*width);for col in width {let v=load(p.x,x,row*width+col);if v>best {best=v;at=col;}}assert(p.d.kind==26&&at<=0x7fffffff);write32(dst,offset(p.d,row),at as u32);}
        else if p.op==16 {var value:f32=(0 as f32);for col in width {value+=load(p.x,x,row*width+col);store(p.d,dst,row*width+col,value);}}
        else {var total:f64=(0 as f64);for col in width {total+=load(p.x,x,row*width+col) as f64;}let value=if p.op==17 {(total/(width as f64)) as f32} else {total as f32};store(p.d,dst,row,value);}
        row+=p.nth;
    }
    return true;
}
fn normalize(p:const Node&,x:const u8[:],y:const u8[:],dst:u8[:])->bool {
    if p.op!=24&&p.op!=25&&p.op!=28&&p.op!=26 {return false;}
    assert(same_shape(p.x,p.d));let width=p.x.ne[0];assert(width>0);var row=thread_first(p);
    while row<rows(p.x) {
        var sum:f64=(0 as f64);var sq:f64=(0 as f64);var cross:f64=(0 as f64);
        for col in width {let v=load(p.x,x,row*width+col);sum+=v as f64;if p.op==26 {let a=load(p.y,y,row*width+col);sq+=(a*a) as f64;cross+=(a*v) as f64;}}
        let mean=if p.op==24 {(sum/(width as f64)) as f32} else {(0 as f32)};
        if p.op!=26 {for col in width {let v=load(p.x,x,row*width+col)-mean;sq+=(v*v) as f64;}}
        let variance=if p.op==28 {sq as f32} else if p.op==26 {(sq as f32)/(width as f32)} else {(sq/(width as f64)) as f32};
        let scale=if p.op==28 {(1 as f32)/max32(sqrt32(variance),p.fp[0])} else {(1 as f32)/sqrt32(variance+p.fp[0])};
        for col in width {
            let v=load(p.x,x,row*width+col);let value=if p.op==26 {(v-load(p.y,y,row*width+col)*(cross as f32)/((sq as f32)+p.fp[0]*(width as f32)))*scale} else {(v-mean)*scale};
            let result=if p.ip[14]!=0&&p.op==25 {value*load_at(p.y,y,broadcast(p.y,p.d,row*width+col))} else {value};store(p.d,dst,row*width+col,result);
        }
        row+=p.nth;
    }
    return true;
}
fn sort_rows(p:const Node&,x:const u8[:],dst:u8[:],indices:i64[:])->bool {
    if p.op!=69&&p.op!=70 {return false;}let n=p.x.ne[0];assert(p.d.kind==26&&p.d.ne[0]<=n);assert(indices.len>=mul_size(n,p.nth));var row=thread_first(p);let base=p.ith*n;
    while row<rows(p.x) {
        for i in n {indices[base+i]=i;}
        for i in n {var j=i;while j>0 {
            let ia=indices[base+j-1];let ib=indices[base+j];let a=load(p.x,x,row*n+ia);let b=load(p.x,x,row*n+ib);
            let ordered=if p.ip[0]==0 {a<=b} else {a>=b};if ordered {break;}indices[base+j-1]=ib;indices[base+j]=ia;j-=1;
        }}
        for i in p.d.ne[0] {assert(indices[base+i]<=0x7fffffff);write32(dst,offset(p.d,row*p.d.ne[0]+i),indices[base+i] as u32);}
        row+=p.nth;
    }
    return true;
}

extern "powf" fn pow32(a:f32,b:f32)->f32;
fn group_normalize(p:const Node&,x:const u8[:],dst:u8[:])->bool {
    if p.op!=27 {return false;}assert(same_shape(p.x,p.d));let groups=p.ip[0];assert(groups>0);let channels=p.x.ne[2];let per=(channels+groups-1)/groups;var group=thread_first(p);
    while group<groups {
        let first=group*per;let last=low(first+per,channels);let count=(last-first)*p.x.ne[0]*p.x.ne[1];assert(count>0);
        for batch in p.x.ne[3] {
            var sum:f64=(0 as f64);
            for ch in last-first {for row in p.x.ne[1] {var part:f64=(0 as f64);for col in p.x.ne[0] {let index=col+p.x.ne[0]*(row+p.x.ne[1]*(ch+first+channels*batch));part+=load(p.x,x,index) as f64;}sum+=part;}}
            let mean=(sum/(count as f64)) as f32;var sq:f64=(0 as f64);
            for ch in last-first {for row in p.x.ne[1] {var part:f64=(0 as f64);for col in p.x.ne[0] {let index=col+p.x.ne[0]*(row+p.x.ne[1]*(ch+first+channels*batch));let v=load(p.x,x,index)-mean;part+=(v*v) as f64;store(p.d,dst,index,v);}sq+=part;}}
            let scale=(1 as f32)/sqrt32((sq/(count as f64)) as f32+p.fp[0]);
            for ch in last-first {for row in p.x.ne[1] {for col in p.x.ne[0] {let index=col+p.x.ne[0]*(row+p.x.ne[1]*(ch+first+channels*batch));store(p.d,dst,index,load(p.d,dst,index)*scale);}}}
        }
        group+=p.nth;
    }
    return true;
}
fn softmax(p:const Node&,x:const u8[:],mask:const u8[:],sinks:const u8[:],dst:u8[:],scratch:f32[:])->bool {
    if p.op!=46&&p.op!=47 {return false;}let width=p.x.ne[0];assert(width>0&&same_shape(p.x,p.d));assert(scratch.len>=mul_size(width,p.nth));let base=p.ith*width;
    var row=thread_first(p);
    while row<rows(p.x) {
        if p.op==47 {
            assert(same_shape(p.x,p.y));var dot:f64=(0 as f64);for col in width {let index=row*width+col;dot+=(load(p.x,x,index)*load(p.y,mask,index)) as f64;}
            for col in width {let index=row*width+col;let grad=load(p.x,x,index);let prob=load(p.y,mask,index);store(p.d,dst,index,p.fp[0]*prob*(grad-dot as f32));}
        }else {
            let r1=row%p.x.ne[1];let head=(row/p.x.ne[1])%p.x.ne[2];let batch=row/(p.x.ne[1]*p.x.ne[2]);var head_power:i64=1;while head_power<=p.x.ne[2]/2 {head_power*=2;}
            var slope:f32=(1 as f32);
            if p.fp[1]>(0 as f32) {let m=pow32((2 as f32),-p.fp[1]/((if head<head_power {head_power} else {2*head_power}) as f32));slope=pow32(m,(if head<head_power {head+1} else {2*(head-head_power)+1}) as f32);}
            var maximum=from_bits(0xff800000);
            for col in width {
                var v=load(p.x,x,row*width+col)*p.fp[0];
                if p.ip[0]!=0 {assert(p.y.ne[0]>=width&&p.y.ne[1]>r1);let off=col*p.y.nb[0]+r1*p.y.nb[1]+(head%p.y.ne[2])*p.y.nb[2]+(batch%p.y.ne[3])*p.y.nb[3];v+=slope*load_at(p.y,mask,off);}
                scratch[base+col]=v;if v>maximum {maximum=v;}
            }
            if p.ip[1]!=0 {let v=load(p.z,sinks,head);if v>maximum {maximum=v;}}
            var sum:f64=(0 as f64);
            for col in width {let v=exp32(scratch[base+col]-maximum);scratch[base+col]=v;sum+=v as f64;}
            if p.ip[1]!=0 {sum+=exp32(load(p.z,sinks,head)-maximum) as f64;}
            assert(sum>(0 as f64));let scale=((1 as f64)/sum) as f32;
            for col in width {store(p.d,dst,row*width+col,scratch[base+col]*scale);}
        }
        row+=p.nth;
    }
    return true;
}

fn transforms(p:const Node&,x:const u8[:],y:const u8[:],dst:u8[:])->bool {
    let op=p.op;
    if op!=1&&op!=34&&op!=35&&op!=20&&op!=22&&op!=64&&op!=65&&op!=66&&op!=67&&op!=73&&op!=43&&op!=44&&op!=45&&op!=40&&op!=42 {return false;}
    var index=thread_first(p);let count=elements(p.d);
    if (op==1||op==34||op==35)&&p.x.kind==p.d.kind {
        assert(count==elements(p.x));let bytes=scalar_bytes(p.x.kind);
        while index<count {let source=offset(p.x,index);let target=offset(p.d,index);for byte in bytes {dst[target+byte]=x[source+byte];}index+=p.nth;}
        return true;
    }

    while index<count {
        var n=index;var c:i64[4]=default<i64[4]>();for axis in 4 {c[axis]=n%p.d.ne[axis];n/=p.d.ne[axis];}
        var value:f32=(0 as f32);var target=index;
        if op==73 {value=p.fp[0];}
        else if op==67 {value=p.fp[0]+p.fp[2]*(index as f32);}
        else if op==20 {value=load_at(p.x,x,broadcast(p.x,p.d,index));}
        else if op==22 {
            let axis=p.ip[0];assert(axis>=0&&axis<4);var source=c;
            if c[axis]<p.x.ne[axis] {var offset:i64=0;for k in 4 {offset=add_size(offset,mul_size(source[k],p.x.nb[k]));}value=load_at(p.x,x,offset);}
            else {source[axis]-=p.x.ne[axis];var offset:i64=0;for k in 4 {offset=add_size(offset,mul_size(source[k],p.y.nb[k]));}value=load_at(p.y,y,offset);}
        }else if op==64 {var valid:bool=true;var offset:i64=0;for axis in 4 {var at=c[axis]-p.ip[2*axis];if p.ip[8]!=0 {assert(p.x.ne[axis]>0);at=((at%p.x.ne[axis])+p.x.ne[axis])%p.x.ne[axis];}else if at<0||at>=p.x.ne[axis] {valid=false;}if at>=0&&at<p.x.ne[axis] {offset=add_size(offset,mul_size(at,p.x.nb[axis]));}}if valid {value=load_at(p.x,x,offset);}}
        else if op==65 {let left=p.ip[0];let at=c[0]-left;let reflected=if at<0 {-at} else if at>=p.x.ne[0] {2*(p.x.ne[0]-1)-at} else {at};assert(reflected>=0&&reflected<p.x.ne[0]);value=load_at(p.x,x,reflected*p.x.nb[0]+c[1]*p.x.nb[1]+c[2]*p.x.nb[2]+c[3]*p.x.nb[3]);}
        else if op==66 {var offset:i64=0;for axis in 4 {let size=p.x.ne[axis];assert(size>0);let at=((c[axis]-p.ip[axis])%size+size)%size;offset=add_size(offset,mul_size(at,p.x.nb[axis]));}value=load_at(p.x,x,offset);}
        else if op==43 {if c[0]==c[1] {value=load(p.x,x,c[0]+p.x.ne[0]*(c[2]+p.x.ne[2]*c[3]));}}
        else if op==44||op==45 {value=load(p.x,x,index);if c[0]>p.ip[0]+c[1] {value=if op==44 {from_bits(0xff800000)} else {(0 as f32)};}}
        else if op==40 {let row=index/p.d.ne[0];let id=read32(y,offset(p.y,row)) as! i32;assert(id>=0&&(id as i64)<p.x.ne[1]);let ix=c[0]*p.x.nb[0]+(id as i64)*p.x.nb[1]+c[2]*p.x.nb[2]+c[3]*p.x.nb[3];value=load_at(p.x,x,ix);}
        else if op==42 {return false;}
        else {assert(count==elements(p.x));value=load(p.x,x,index);}
        store(p.d,dst,target,value);index+=p.nth;
    }
    return true;
}

fn vector_dot(a:const Tensor&,x:const u8[:],ax:i64,as0:i64,b:const Tensor&,y:const u8[:],by:i64,bs0:i64,count:i64)->f32 {
    assert(count>=0);var acc:f32[8][4]=default<f32[8][4]>();let end=(count/32)*32;var k:i64=0;
    while k<end {for group in 4 {for lane in 8 {let i=k+8*group+lane;let va=load_at(a,x,ax+i*as0);let vb=load_at(b,y,by+i*bs0);acc[group][lane]=if a.kind==30 {acc[group][lane]+va*vb} else {fma32(va,vb,acc[group][lane])};}}k+=32;}
    var v:f32[8]=default<f32[8]>();for lane in 8 {v[lane]=(acc[0][lane]+acc[2][lane])+(acc[1][lane]+acc[3][lane]);}
    let s0=v[0]+v[4];let s1=v[1]+v[5];let s2=v[2]+v[6];let s3=v[3]+v[7];
    if a.kind==0 {var result=(s0+s1)+(s2+s3);while k<count {result+=load_at(a,x,ax+k*as0)*load_at(b,y,by+k*bs0);k+=1;}return result;}
    var result=if a.kind==30 {((s0+s2)+(s1+s3)) as f64} else {((s0+s1)+(s2+s3)) as f64};
    while k<count {result+=(load_at(a,x,ax+k*as0)*load_at(b,y,by+k*bs0)) as f64;k+=1;}return result as f32;
}
fn matrix(p:const Node&,x:const u8[:],y:const u8[:],ids:const u8[:],dst:u8[:])->bool {
    if p.op!=29&&p.op!=30&&p.op!=31 {return false;}
    if p.x.kind!=0&&p.x.kind!=1&&p.x.kind!=30 {return false;}if p.y.kind!=0&&p.y.kind!=1&&p.y.kind!=30 {return false;}
    let d0=p.d.ne[0];assert(d0>0&&p.d.kind==0);var row=thread_first(p);
    while row<rows(p.d) {
        let i1=row%p.d.ne[1];let i2=(row/p.d.ne[1])%p.d.ne[2];let i3=row/(p.d.ne[1]*p.d.ne[2]);
        for i0 in d0 {
            var xa:i64=0;var ya:i64=0;var xs:i64=0;var ys:i64=0;var count:i64=0;
            if p.op==29 {
                assert(p.x.ne[0]==p.y.ne[0]&&p.x.ne[1]==d0&&p.x.ne[2]>0&&p.x.ne[3]>0&&p.y.ne[2]%p.x.ne[2]==0&&p.y.ne[3]%p.x.ne[3]==0);
                let x2=i2/(p.y.ne[2]/p.x.ne[2]);let x3=i3/(p.y.ne[3]/p.x.ne[3]);xa=i0*p.x.nb[1]+x2*p.x.nb[2]+x3*p.x.nb[3];ya=i1*p.y.nb[1]+i2*p.y.nb[2]+i3*p.y.nb[3];xs=p.x.nb[0];ys=p.y.nb[0];count=p.x.ne[0];
            }else if p.op==30 {
                assert(p.z.kind==26&&p.x.ne[0]==p.y.ne[0]&&p.x.ne[1]==d0&&p.y.ne[1]>0);let id=read32(ids,offset(p.z,i1+p.z.ne[0]*i2)) as! i32;assert(id>=0&&(id as i64)<p.x.ne[2]);xa=i0*p.x.nb[1]+(id as i64)*p.x.nb[2];ya=(i1%p.y.ne[1])*p.y.nb[1]+i2*p.y.nb[2];xs=p.x.nb[0];ys=p.y.nb[0];count=p.x.ne[0];
            }else {
                assert(p.x.ne[1]==p.y.ne[1]&&p.x.ne[0]==d0&&p.y.ne[0]==p.d.ne[1]);let x2=i2%p.x.ne[2];let x3=i3%p.x.ne[3];let y2=i2%p.y.ne[2];let y3=i3%p.y.ne[3];xa=i0*p.x.nb[0]+x2*p.x.nb[2]+x3*p.x.nb[3];ya=i1*p.y.nb[0]+y2*p.y.nb[2]+y3*p.y.nb[3];xs=p.x.nb[1];ys=p.y.nb[1];count=p.x.ne[1];
            }
            store(p.d,dst,row*d0+i0,vector_dot(p.x,x,xa,xs,p.y,y,ya,ys,count));
        }
        row+=p.nth;
    }
    return true;
}

struct ScanPlan { source:Tensor[7], ith:i64, nth:i64, snapshots:i64 }
fn ssm_conv(p:const Node&,x:const u8[:],weight:const u8[:],dst:u8[:])->bool {
    if p.op!=76 {return false;}let kernel=p.y.ne[0];let channels=p.x.ne[1];let tokens=p.d.ne[1];let seqs=p.d.ne[2];assert(kernel>0&&p.x.ne[0]==kernel-1+tokens&&p.y.ne[1]==channels&&p.d.ne[0]==channels);var channel=thread_first(p);
    while channel<channels {
        for seq in seqs {for token in tokens {var sum:f32=(0 as f32);for tap in kernel {let v=load_at(p.x,x,(token+tap)*p.x.nb[0]+channel*p.x.nb[1]+seq*p.x.nb[2]);let w=load_at(p.y,weight,tap*p.y.nb[0]+channel*p.y.nb[1]);sum+=v*w;}store(p.d,dst,channel+channels*(token+tokens*seq),sum);}}
        channel+=p.nth;
    }
    return true;
}
fn softplus_state(x:f32)->f32 {return if x>(20 as f32) {x} else {log32((1 as f32)+exp32(x))};}
fn scan_entry(p:const ScanPlan&,initial:const u8[:],input:const u8[:],dt:const u8[:],decay:const u8[:],b:const u8[:],c:const u8[:],ids:const u8[:],dst:f32[:])->bool {
    assert(p.nth>0&&p.ith>=0&&p.ith<p.nth&&p.snapshots>0);for i in 6 {assert(p.source[i].kind==0);}assert(p.source[6].kind==26);
    let state_size=p.source[0].ne[0];let width=p.source[0].ne[1];let heads=p.source[1].ne[1];let tokens=p.source[1].ne[2];let seqs=p.source[1].ne[3];let groups=p.source[4].ne[1];assert(state_size>0&&width>0&&groups>0&&heads%groups==0);
    let state_stride=p.source[0].nb[3]/4;let output=mul_size(mul_size(mul_size(width,heads),tokens),seqs);assert(dst.len>=add_size(output,mul_size(mul_size(state_stride,seqs),p.snapshots)));let first=(heads*p.ith)/p.nth;let last=(heads*(p.ith+1))/p.nth;
    for seq in seqs {
        let id=read32(ids,offset(p.source[6],seq)) as! i32;assert(id>=0&&(id as i64)<p.source[0].ne[3]);
        for token in tokens {
            for head_delta in last-first {
                let head=first+head_delta;let group=head/(heads/groups);let timestep=softplus_state(load_at(p.source[2],dt,head*p.source[2].nb[0]+token*p.source[2].nb[1]+seq*p.source[2].nb[2]));let a0=load_at(p.source[3],decay,head*p.source[3].nb[1]);let shared=p.source[3].ne[0]==1;let factor=exp32(timestep*a0);
                for lane in width {
                    let x=load_at(p.source[1],input,lane*p.source[1].nb[0]+head*p.source[1].nb[1]+token*p.source[1].nb[2]+seq*p.source[1].nb[3]);let scaled=x*timestep;var sum:f32=(0 as f32);
                    for state in state_size {
                        let position=state+(lane+head*width)*state_size;let target=output+seq*state_stride+position;
                        let previous=if token==0 {load_at(p.source[0],initial,state*p.source[0].nb[0]+lane*p.source[0].nb[1]+head*p.source[0].nb[2]+(id as i64)*p.source[0].nb[3])} else {dst[target]};
                        let ba=load_at(p.source[4],b,state*p.source[4].nb[0]+group*p.source[4].nb[1]+token*p.source[4].nb[2]+seq*p.source[4].nb[3]);let ca=load_at(p.source[5],c,state*p.source[5].nb[0]+group*p.source[5].nb[1]+token*p.source[5].nb[2]+seq*p.source[5].nb[3]);
                        let rate=if shared {factor} else {exp32(timestep*load_at(p.source[3],decay,state*p.source[3].nb[0]+head*p.source[3].nb[1]))};let value=previous*rate+ba*scaled;dst[target]=value;sum+=value*ca;
                    }
                    dst[lane+width*(head+heads*(token+tokens*seq))]=sum;
                }
                let slot=tokens-1-token;
                if p.snapshots>1&&slot>0&&slot<p.snapshots {for i in state_size*width {let pos=i+head*state_size*width;dst[output+(slot*seqs+seq)*state_stride+pos]=dst[output+seq*state_stride+pos];}}
            }
        }
    }
    witness(77,output);return true;
}

fn yarn_pair(theta:f32,scale:f32,corr:f32[2],i:i64,ext:f32,amplitude:f32)->f32[2] {
    var angle=theta*scale;var magnitude=amplitude;
    if ext!=(0 as f32) {let ramp=(1 as f32)-min32((1 as f32),max32((0 as f32),((i/2) as f32-corr[0])/max32((0.001 as f32),corr[1]-corr[0])));let mix=ramp*ext;angle=angle*((1 as f32)-mix)+theta*mix;magnitude*=(1 as f32)+(0.1 as f32)*log32((1 as f32)/scale);}
    var out:f32[2]=default<f32[2]>();out[0]=cos32(angle)*magnitude;out[1]=sin32(angle)*magnitude;return out;
}
fn rope(p:const Node&,x:const u8[:],positions:const u8[:],factors:const u8[:],dst:u8[:],cache:f32[:])->bool {
    if p.op!=48&&p.op!=49 {return false;}assert(same_shape(p.x,p.d)&&p.y.kind==26&&(p.x.kind==0||p.x.kind==1));let dims=p.ip[0];let mode=p.ip[1];let original=p.ip[2];let offs=p.ip[3];let width=p.x.ne[0];assert(dims>0&&dims%2==0&&offs>=0&&offs%2==0&&offs+dims<=width&&width%2==0);assert(cache.len>=mul_size(width,p.nth));assert(p.fp[0]>(0 as f32)&&p.fp[1]>(0 as f32));let vision=mode==24;let multiple=(mode&8)!=0;let interleave=mode==40;
    if vision {assert(dims==width/2&&offs==0);}assert(mode==0||mode==2||mode==8||mode==24||mode==40);
    let theta_scale=pow32(p.fp[0],-(2 as f32)/(dims as f32));let sign=if p.op==48 {(1 as f32)} else {-(1 as f32)};
    let pi2=(6.283185307179586 as f32);var corr:f32[2]=default<f32[2]>();let divisor=(2 as f32)*log32(p.fp[0]);corr[0]=max32((0 as f32),floor32((dims as f32)*log32((original as f32)/(p.fp[4]*pi2))/divisor));corr[1]=min32((dims-1) as f32,ceil32((dims as f32)*log32((original as f32)/(p.fp[5]*pi2))/divisor));
    let base=p.ith*width;var row=thread_first(p);
    while row<rows(p.x) {
        let token=(row/p.x.ne[1])%p.x.ne[2];var initial:f32[4]=default<f32[4]>();var theta:f32[4]=default<f32[4]>();
        for axis in 4 {let at=token+(if multiple {axis*p.x.ne[2]} else {0});initial[axis]=(read32(positions,offset(p.y,at)) as! i32) as f32;theta[axis]=initial[axis];}
        let total=p.ip[4]+p.ip[5]+p.ip[6]+p.ip[7];if multiple {assert(total>0&&total<=width);}
        for pair in width/2 {
            let i=2*pair;var angle=theta[0];
            if multiple {
                let sector=pair%total;let secw=p.ip[4]+p.ip[5];let sece=secw+p.ip[6];
                if vision {if sector==0 {theta[0]=initial[0];}else if sector==p.ip[4] {theta[1]=initial[1];}else if sector==secw {theta[2]=initial[2];}else if sector==sece {theta[3]=initial[3];}}
                if interleave {if sector%3==1&&sector<3*p.ip[5] {angle=theta[1];}else if sector%3==2&&sector<3*p.ip[6] {angle=theta[2];}else if sector%3==0&&sector<3*p.ip[4] {angle=theta[0];}else {angle=theta[3];}}
                else {let axis=if sector<p.ip[4] {0} else if sector<secw {1} else if sector<sece {2} else {3};angle=theta[axis];}
            }
            let ff=if p.ip[8]!=0 {load(p.z,factors,pair)} else {(1 as f32)};let rotation=yarn_pair(angle/ff,p.fp[1],corr,i,p.fp[2],p.fp[3]);cache[base+i]=rotation[0];cache[base+i+1]=rotation[1]*sign;for axis in 4 {theta[axis]*=theta_scale;}
        }
        let rotated=if vision {width} else {dims};let distance=if mode==0 {1} else if vision {dims} else {dims/2};let stride=if mode==0 {1} else {2};
        for pair in rotated/2 {let i=2*pair;let index=offs+i/stride;let a=load(p.x,x,row*width+index);let b=load(p.x,x,row*width+index+distance);let c=cache[base+i];let s=cache[base+i+1];store(p.d,dst,row*width+index,a*c-b*s);store(p.d,dst,row*width+index+distance,a*s+b*c);}
        if !vision {let bytes=scalar_bytes(p.x.kind);for col in width {if col<offs||col>=offs+dims {let source=offset(p.x,row*width+col);let target=offset(p.d,row*width+col);for byte in bytes {dst[target+byte]=x[source+byte];}}}}
        row+=p.nth;
    }
    return true;
}

fn read_index(t:const Tensor&,a:const u8[:],i:i64)->i64 {
    let at=offset(t,i);if t.kind==26 {return (read32(a,at) as! i32) as i64;}
    assert(t.kind==27&&at>=0&&at<=a.len-8);var u:u64=0;for b in 8 {u=u|((a[at+b] as u64)<<((8*b) as u64));}return u as! i64;
}
fn indexed(p:const Node&,x:const u8[:],y:const u8[:],dst:u8[:])->bool {
    if p.op!=42&&p.op!=41&&p.op!=21&&p.op!=3&&p.op!=5&&p.op!=33&&p.op!=72 {return false;}
    if p.op==72 {var index=thread_first(p);while index<elements(p.d) {let col=index%p.d.ne[0];let row=(index/p.d.ne[0])%p.d.ne[1];let keep=if p.ip[0]!=0 {col>=row+p.ip[1]} else {col<=row+p.ip[1]};store(p.d,dst,index,if keep {load(p.x,x,index)} else {(0 as f32)});index+=p.nth;}return true;}
    if p.op==21 {
        if p.ith!=0 {return true;}for i in elements(p.d) {store(p.d,dst,i,(0 as f32));}
        for i in elements(p.x) {var n=i;var target:i64=0;var stride:i64=1;for axis in 4 {let at=n%p.x.ne[axis];n/=p.x.ne[axis];target+=(at%p.d.ne[axis])*stride;stride*=p.d.ne[axis];}store(p.d,dst,target,load(p.d,dst,target)+load(p.x,x,i));}return true;
    }
    if p.op==41 {
        if p.ith!=0 {return true;}for i in elements(p.d) {store(p.d,dst,i,(0 as f32));}
        let width=p.x.ne[0];for row in rows(p.x) {let id=read_index(p.y,y,row);assert(id>=0&&id<p.d.ne[1]);for col in width {let index=col+id*width;store(p.d,dst,index,load(p.d,dst,index)+load(p.x,x,row*width+col));}}return true;
    }
    if p.op==42 {
        if p.ith!=0 {return true;}let width=p.x.ne[0];assert(width==p.d.ne[0]);
        for row in rows(p.x) {let i1=row%p.x.ne[1];let i2=(row/p.x.ne[1])%p.x.ne[2];let i3=row/(p.x.ne[1]*p.x.ne[2]);let id=read_index(p.y,y,i1+p.y.ne[0]*((i2%p.y.ne[1])+p.y.ne[1]*(i3%p.y.ne[2])));assert(id>=0&&id<p.d.ne[1]);for col in width {let target=col*p.d.nb[0]+id*p.d.nb[1]+i2*p.d.nb[2]+i3*p.d.nb[3];store_at(p.d,dst,target,load(p.x,x,row*width+col));}}return true;
    }
    if p.op==3 {return false;}
    if p.ith!=0 {return true;}assert(elements(p.x)==elements(p.d));
    if p.ip[4]==0 {for i in elements(p.x) {let a=offset(p.x,i);let b=offset(p.d,i);for byte in scalar_bytes(p.x.kind) {dst[b+byte]=x[a+byte];}}}
    for i in elements(p.y) {var n=i;var target=p.ip[3];for axis in 4 {let at=n%p.y.ne[axis];n/=p.y.ne[axis];let stride=if axis==0 {p.d.nb[0]} else {p.ip[axis-1]};target=add_size(target,mul_size(at,stride));}let value=load(p.y,y,i);store_at(p.d,dst,target,if p.op==5 {load_at(p.d,dst,target)+value} else {value});}return true;
}

fn image_ops(p:const Node&,x:const u8[:],y:const u8[:],dst:u8[:])->bool {
    if p.op!=52&&p.op!=60&&p.op!=61&&p.op!=78&&p.op!=79 {return false;}
    var i=thread_first(p);let count=elements(p.d);
    while i<count {
        let o0=i%p.d.ne[0];let o1=(i/p.d.ne[0])%p.d.ne[1];let o2=(i/(p.d.ne[0]*p.d.ne[1]))%p.d.ne[2];let o3=i/(p.d.ne[0]*p.d.ne[1]*p.d.ne[2]);var value:f32=(0 as f32);
        if p.op==52 {
            let is2=p.ip[6]!=0;let kw=p.x.ne[0];let kh=if is2 {p.x.ne[1]} else {1};let channel=o0/(kw*kh);let tap=o0%(kw*kh);let xx=o1*p.ip[0]+(tap%kw)*p.ip[4]-p.ip[2];let yy=if is2 {o2*p.ip[1]+(tap/kw)*p.ip[5]-p.ip[3]} else {0};let input_h=if is2 {p.y.ne[1]} else {1};let batch=if is2 {o3} else {o2};
            if xx>=0&&xx<p.y.ne[0]&&yy>=0&&yy<input_h {let off=xx*p.y.nb[0]+(if is2 {yy*p.y.nb[1]+channel*p.y.nb[2]+batch*p.y.nb[3]} else {channel*p.y.nb[1]+batch*p.y.nb[2]});value=load_at(p.y,y,off);}
        }else if p.op==60||p.op==61 {
            let is2=p.op==61;let k0=p.ip[1];let k1=if is2 {p.ip[2]} else {1};let s0=if is2 {p.ip[3]} else {p.ip[2]};let s1=if is2 {p.ip[4]} else {1};let pad0=if is2 {p.ip[5]} else {p.ip[3]};let pad1=if is2 {p.ip[6]} else {0};assert(k0>0&&k1>0);let average=p.ip[0]==0;value=if average {(0 as f32)} else {from_bits(0xff7fffff)};
            for ky in k1 {for kx in k0 {let xx=o0*s0-pad0+kx;let yy=if is2 {o1*s1-pad1+ky} else {o1};if xx>=0&&xx<p.x.ne[0]&&yy>=0&&yy<p.x.ne[1] {let v=load_at(p.x,x,xx*p.x.nb[0]+yy*p.x.nb[1]+o2*p.x.nb[2]+o3*p.x.nb[3]);value=if average {value+v} else {max32(v,value)};}}}
            if average {value/=(k0*k1) as f32;}
        }else if p.op==78 {
            let window=p.ip[0];let nx=p.ip[1];assert(window>0&&nx>0);let wx=o3%nx;let wy=o3/nx;let xx=wx*window+o1;let yy=wy*window+o2;if xx<p.x.ne[1]&&yy<p.x.ne[2] {value=load_at(p.x,x,o0*p.x.nb[0]+xx*p.x.nb[1]+yy*p.x.nb[2]);}
        }else {
            let window=p.ip[0];let nx=(p.d.ne[1]+window-1)/window;let which=(o2/window)*nx+(o1/window);value=load_at(p.x,x,o0*p.x.nb[0]+(o1%window)*p.x.nb[1]+(o2%window)*p.x.nb[2]+which*p.x.nb[3]);
        }
        store(p.d,dst,i,value);i+=p.nth;
    }
    return true;
}

import .codec.decode;
fn codec_transform(p:const Node&,x:const u8[:],ids:const u8[:],dst:u8[:],scratch:f32[:],gxx:const u8[8][:],gxs:const u8[8][:],gs:const u8[8][:],g3x:const u8[4][:],g3s:const u8[4][:],g1:const i8[8][:],signs:const u8[:],palette:const i8[:])->bool {
    if p.op!=1&&p.op!=34&&p.op!=35&&p.op!=40 {return false;}if p.d.kind!=0||scalar_type(p.x) {return false;}
    let kind=decode::super_kind(p.x.kind);let block_width=if kind>=0 {256} else {decode::compact_width(p.x.kind)};let bytes=if kind>=0 {decode::tile_traits(kind).bytes} else {decode::compact_bytes(p.x.kind)};if bytes==0 {return false;}
    let width=p.x.ne[0];assert(width>0&&width%block_width==0);let rowbytes=mul_size(width/block_width,bytes);assert(p.x.bytes<=x.len&&scratch.len>=mul_size(width,p.nth));validate(p.d,dst.len);let temp=p.ith*width;
    if p.op==40 {assert(p.y.kind==26&&p.d.ne[0]==width);validate(p.y,ids.len);}else {assert(elements(p.x)==elements(p.d));}
    let nrows=if p.op==40 {rows(p.d)} else {rows(p.x)};var row=thread_first(p);
    while row<nrows {
        var source:i64=0;
        if p.op==40 {let i0=row%p.y.ne[0];let i1=(row/p.y.ne[0])%p.y.ne[1];let i2=row/(p.y.ne[0]*p.y.ne[1]);let id=read_index(p.y,ids,row);assert(id>=0&&id<p.x.ne[1]);source=id*p.x.nb[1]+i1*p.x.nb[2]+i2*p.x.nb[3];}
        else {let i1=row%p.x.ne[1];let i2=(row/p.x.ne[1])%p.x.ne[2];let i3=row/(p.x.ne[1]*p.x.ne[2]);source=i1*p.x.nb[1]+i2*p.x.nb[2]+i3*p.x.nb[3];}
        assert(source>=0&&source<=p.x.bytes&&rowbytes<=p.x.bytes-source);let output=scratch[temp..temp+width];assert(decode::row_entry(p.x.kind,x[source..source+rowbytes],output,gxx,gxs,gs,g3x,g3s,g1,signs,palette));
        for col in width {store(p.d,dst,row*width+col,output[col]);}row+=p.nth;
    }
    return true;
}

fn entry(p:const Node&,x:const u8[:],y:const u8[:],z:const u8[:],dst:u8[:],scratch:f32[:],indices:i64[:],gxx:const u8[8][:],gxs:const u8[8][:],gs:const u8[8][:],g3x:const u8[4][:],g3s:const u8[4][:],g1:const i8[8][:],signs:const u8[:],palette:const i8[:])->bool {
    assert(p.nth>0&&p.ith>=0&&p.ith<p.nth);
    if p.ip[15]==1464816203 {
        assert(indices.len>=4);var nf:i64=0;var ni:i64=0;
        if p.op==46||p.op==47||p.op==48||p.op==49 {nf=mul_size(p.x.ne[0],p.nth);}
        if p.op==69||p.op==70 {ni=mul_size(p.x.ne[0],p.nth);}
        if (p.op==1||p.op==34||p.op==35||p.op==40)&&!scalar_type(p.x)&&p.d.kind==0 {nf=mul_size(p.x.ne[0],p.nth);}
        let byte=mul_size(nf,4);let aligned=mul_size(add_size(byte,7)/8,8);indices[0]=nf;indices[1]=ni;indices[2]=aligned;indices[3]=add_size(aligned,mul_size(ni,8));return true;
    }
    if codec_transform(p,x,y,dst,scratch,gxx,gxs,gs,g3x,g3s,g1,signs,palette) {witness(p.op,elements(p.d));return true;}
    if !scalar_type(p.x)||!scalar_type(p.d) {return false;}
    validate(p.d,dst.len);validate(p.x,x.len);
    if p.y.bytes>0 {validate(p.y,y.len);}if p.z.bytes>0 {validate(p.z,z.len);}
    if transforms(p,x,y,dst) {witness(p.op,elements(p.d));return true;}
    if p.x.kind==27||p.x.kind==28||p.d.kind==27||p.d.kind==28 {if p.op!=19 {return false;}}
    let handled=indexed(p,x,y,dst)||image_ops(p,x,y,dst)||rope(p,x,y,z,dst,scratch)||matrix(p,x,y,z,dst)||ssm_conv(p,x,y,dst)||pointwise(p,x,y,dst)||glu(p,x,y,dst)||reduction(p,x,y,dst)||normalize(p,x,y,dst)||group_normalize(p,x,dst)||softmax(p,x,y,z,dst,scratch)||sort_rows(p,x,dst,indices);
    if handled {witness(p.op,elements(p.d));}return handled;
}
fn main() {
    var p:Node=default<Node>();p.nth=1;
    let x:u8[4]=default<u8[4]>();var d:u8[4]=default<u8[4]>();var f:f32[1]=default<f32[1]>();var i:i64[1]=default<i64[1]>();
    let g8:u8[8][1]=default<u8[8][1]>();let g4:u8[4][1]=default<u8[4][1]>();let g1:i8[8][1]=default<i8[8][1]>();let signs:u8[128]=default<u8[128]>();let palette:i8[16]=default<i8[16]>();let ok=entry(p,x,x,x,d,f,i,g8,g8,g8,g4,g4,g1,signs,palette);print(ok);
}
