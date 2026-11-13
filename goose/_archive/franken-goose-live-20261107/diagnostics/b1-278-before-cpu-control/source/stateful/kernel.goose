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

fn vector_dot(a:const Tensor&,x:const u8[:],ax:i64,as0:i64,b:const Tensor&,y:const u8[:],by:i64,bs0:i64,count:i64)->f32 {
    assert(count>=0);var acc:f32[8][4]=default<f32[8][4]>();let end=(count/32)*32;var k:i64=0;
    while k<end {for group in 4 {for lane in 8 {let i=k+8*group+lane;let va=load_at(a,x,ax+i*as0);let vb=load_at(b,y,by+i*bs0);acc[group][lane]=if a.kind==30 {acc[group][lane]+va*vb} else {fma32(va,vb,acc[group][lane])};}}k+=32;}
    var v:f32[8]=default<f32[8]>();for lane in 8 {v[lane]=(acc[0][lane]+acc[2][lane])+(acc[1][lane]+acc[3][lane]);}
    let s0=v[0]+v[4];let s1=v[1]+v[5];let s2=v[2]+v[6];let s3=v[3]+v[7];
    if a.kind==0 {var result=(s0+s1)+(s2+s3);while k<count {result+=load_at(a,x,ax+k*as0)*load_at(b,y,by+k*bs0);k+=1;}return result;}
    var result=if a.kind==30 {((s0+s2)+(s1+s3)) as f64} else {((s0+s1)+(s2+s3)) as f64};
    while k<count {result+=(load_at(a,x,ax+k*as0)*load_at(b,y,by+k*bs0)) as f64;k+=1;}return result as f32;
}

struct ScanPlan { source:Tensor[7], ith:i64, nth:i64, snapshots:i64 }
fn softplus_state(x:f32)->f32 {return if x>(20 as f32) {x} else {log32((1 as f32)+exp32(x))};}
fn scan_entry(p:const ScanPlan&,initial:const u8[:],input:const u8[:],dt:const u8[:],decay:const u8[:],b:const u8[:],c:const u8[:],ids:const u8[:],dst:u8[:])->bool {
    assert(p.nth>0&&p.ith>=0&&p.ith<p.nth&&p.snapshots>0);for i in 6 {assert(p.source[i].kind==0);}assert(p.source[6].kind==26);
    let state_size=p.source[0].ne[0];let width=p.source[0].ne[1];let heads=p.source[1].ne[1];let tokens=p.source[1].ne[2];let seqs=p.source[1].ne[3];let groups=p.source[4].ne[1];assert(state_size>0&&width>0&&groups>0&&heads%groups==0);
    let state_stride=p.source[0].nb[3]/4;let output=mul_size(mul_size(mul_size(width,heads),tokens),seqs);assert(dst.len>=mul_size(add_size(output,mul_size(mul_size(state_stride,seqs),p.snapshots)),4));let first=(heads*p.ith)/p.nth;let last=(heads*(p.ith+1))/p.nth;
    for seq in seqs {
        let id=read32(ids,offset(p.source[6],seq)) as! i32;assert(id>=0&&(id as i64)<p.source[0].ne[3]);
        for token in tokens {
            for head_delta in last-first {
                let head=first+head_delta;let group=head/(heads/groups);let timestep=softplus_state(load_at(p.source[2],dt,head*p.source[2].nb[0]+token*p.source[2].nb[1]+seq*p.source[2].nb[2]));let a0=load_at(p.source[3],decay,head*p.source[3].nb[1]);let shared=p.source[3].ne[0]==1;let factor=exp32(timestep*a0);
                for lane in width {
                    let x=load_at(p.source[1],input,lane*p.source[1].nb[0]+head*p.source[1].nb[1]+token*p.source[1].nb[2]+seq*p.source[1].nb[3]);let scaled=x*timestep;var sum:f32=(0 as f32);
                    for state in state_size {
                        let position=state+(lane+head*width)*state_size;let target=output+seq*state_stride+position;
                        let previous=if token==0 {load_at(p.source[0],initial,state*p.source[0].nb[0]+lane*p.source[0].nb[1]+head*p.source[0].nb[2]+(id as i64)*p.source[0].nb[3])} else {value_at(dst,target)};
                        let ba=load_at(p.source[4],b,state*p.source[4].nb[0]+group*p.source[4].nb[1]+token*p.source[4].nb[2]+seq*p.source[4].nb[3]);let ca=load_at(p.source[5],c,state*p.source[5].nb[0]+group*p.source[5].nb[1]+token*p.source[5].nb[2]+seq*p.source[5].nb[3]);
                        let rate=if shared {factor} else {exp32(timestep*load_at(p.source[3],decay,state*p.source[3].nb[0]+head*p.source[3].nb[1]))};let value=previous*rate+ba*scaled;set_value(dst,target,value);sum+=value*ca;
                    }
                    set_value(dst,lane+width*(head+heads*(token+tokens*seq)),sum);
                }
                let slot=tokens-1-token;
                if p.snapshots>1&&slot>0&&slot<p.snapshots {for i in state_size*width {let pos=i+head*state_size*width;set_value(dst,output+(slot*seqs+seq)*state_stride+pos,value_at(dst,output+seq*state_stride+pos));}}
            }
        }
    }
    witness(77,output);return true;
}

struct StatePlan { source:Tensor[7], dst:Tensor, ith:i64, nth:i64, op:i64, snapshots:i64, fp:f32[4] }
fn value_at(a:const u8[:],index:i64)->f32 {return from_bits(read32(a,mul_size(index,4)));}
fn set_value(a:u8[:],index:i64,v:f32){write32(a,mul_size(index,4),bits(v));}
fn gdn(p:const StatePlan&,q:const u8[:],k:const u8[:],v:const u8[:],g:const u8[:],beta:const u8[:],initial:const u8[:],dst:u8[:])->bool {
    let width=p.source[2].ne[0];let heads=p.source[2].ne[1];let tokens=p.source[2].ne[2];let seqs=p.source[2].ne[3];let snapshots=p.snapshots;
    assert(width>0&&heads>0&&tokens>=0&&seqs>0&&snapshots>=1&&p.nth>0&&p.ith>=0&&p.ith<p.nth);assert(p.dst.kind==0);for source in 6 {assert(p.source[source].kind==0);}
    assert(p.source[0].ne[0]==width&&p.source[1].ne[0]==width&&p.source[0].ne[1]>0&&p.source[1].ne[1]>0&&heads%p.source[0].ne[1]==0&&heads%p.source[1].ne[1]==0&&p.source[0].ne[3]>0&&p.source[1].ne[3]>0&&seqs%p.source[0].ne[3]==0&&seqs%p.source[1].ne[3]==0);
    assert((p.source[3].ne[0]==1||p.source[3].ne[0]==width)&&p.source[4].ne[0]==1);assert(p.source[5].ne[0]==width&&p.source[5].ne[1]==width&&p.source[5].ne[2]==heads&&p.source[5].ne[3]>=seqs);
    let square=mul_size(width,width);let scores=mul_size(mul_size(mul_size(width,heads),tokens),seqs);let snapshot_values=mul_size(mul_size(square,heads),seqs);assert(mul_size(add_size(scores,mul_size(snapshot_values,snapshots)),4)<=dst.len);
    let scale=(1 as f32)/sqrt32(width as f32);var task=p.ith;
    while task<mul_size(heads,seqs) {
        let head=task%heads;let seq=task/heads;let qhead=head%p.source[0].ne[1];let khead=head%p.source[1].ne[1];let qseq=seq/(seqs/p.source[0].ne[3]);let kseq=seq/(seqs/p.source[1].ne[3]);let statebase=scores+task*square;
        if tokens>0||snapshots==1 {for j in width {for i in width {let at=i*p.source[5].nb[0]+j*p.source[5].nb[1]+head*p.source[5].nb[2]+seq*p.source[5].nb[3];set_value(dst,statebase+j*width+i,load_at(p.source[5],initial,at));}}}
        for token in tokens {
            let qb=qhead*p.source[0].nb[1]+token*p.source[0].nb[2]+qseq*p.source[0].nb[3];let kb=khead*p.source[1].nb[1]+token*p.source[1].nb[2]+kseq*p.source[1].nb[3];let vb=head*p.source[2].nb[1]+token*p.source[2].nb[2]+seq*p.source[2].nb[3];let gb=head*p.source[3].nb[1]+token*p.source[3].nb[2]+seq*p.source[3].nb[3];let bb=head*p.source[4].nb[1]+token*p.source[4].nb[2]+seq*p.source[4].nb[3];let rate=load_at(p.source[4],beta,bb);
            for j in width {for i in width {let decay=exp32(load_at(p.source[3],g,gb+(if p.source[3].ne[0]==1 {0} else {i*p.source[3].nb[0]})));let at=statebase+j*width+i;set_value(dst,at,value_at(dst,at)*decay);}}
            for j in width {
                let predicted=vector_dot(p.dst,dst,(statebase+j*width)*4,4,p.source[1],k,kb,p.source[1].nb[0],width);let delta=(load_at(p.source[2],v,vb+j*p.source[2].nb[0])-predicted)*rate;
                for i in width {let at=statebase+j*width+i;let key=load_at(p.source[1],k,kb+i*p.source[1].nb[0]);set_value(dst,at,fma32(key,delta,value_at(dst,at)));}
            }
            for j in width {let result=vector_dot(p.dst,dst,(statebase+j*width)*4,4,p.source[0],q,qb,p.source[0].nb[0],width);set_value(dst,(seq*tokens*heads+token*heads+head)*width+j,result*scale);}
            let slot=tokens-1-token;
            if snapshots>1&&slot>0&&slot<snapshots {for i in square {set_value(dst,scores+slot*snapshot_values+task*square+i,value_at(dst,statebase+i));}}
        }
        task+=p.nth;
    }
    witness(86,scores);return true;
}
fn recurrent_attention(p:const StatePlan&,a0:const u8[:],a1:const u8[:],a2:const u8[:],a3:const u8[:],a4:const u8[:],a5:const u8[:],a6:const u8[:],dst:u8[:])->bool {
    let v7=p.op==84;let linear=p.op==83;
    if !v7&&!linear&&p.op!=82 {return false;}
    let state_index=if v7 {6} else {if linear {4} else {5}};
    let width=p.source[1].ne[0];let heads=p.source[1].ne[1];let tokens=p.source[1].ne[2];let seqs=p.source[state_index].ne[1];
    assert(width>0&&heads>0&&tokens>=0&&seqs>0&&tokens%seqs==0&&p.nth>0&&p.ith>=0&&p.ith<p.nth);
    assert(p.dst.kind==0);validate(p.dst,dst.len);
    for input in state_index+1 {
        let shape=p.source[input];assert(shape.kind==0);var stride:i64=4;
        for axis in 4 {assert(shape.nb[axis]==stride);stride=mul_size(stride,shape.ne[axis]);}
        assert(shape.bytes>=stride);
    }
    validate(p.source[0],a0.len);validate(p.source[1],a1.len);validate(p.source[2],a2.len);validate(p.source[3],a3.len);validate(p.source[4],a4.len);
    if !linear {validate(p.source[5],a5.len);}if v7 {validate(p.source[6],a6.len);assert(width%32==0);}
    let channels=mul_size(width,heads);let scores=mul_size(channels,tokens);let square=mul_size(width,width);let state_values=mul_size(mul_size(square,heads),seqs);
    assert(elements(p.source[state_index])==state_values&&mul_size(add_size(scores,state_values),4)<=dst.len);
    for input in state_index {
        if !v7&&input==3&&!linear {assert(elements(p.source[input])>=channels);}else {assert(elements(p.source[input])>=scores);}
    }
    let begin=heads*p.ith/p.nth;let finish=heads*(p.ith+1)/p.nth;let simd_end=width/8*8;
    for head in finish-begin {
        let h=begin+head;
        for token in tokens {
            let seq=token/(tokens/seqs);let output=token*channels+h*width;let state_base=scores+(seq*heads+h)*square;
            let initial=(seq*heads+h)*square;
            if token%(tokens/seqs)==0 {
                for i in square {let value=if v7 {value_at(a6,initial+i)}else {if linear {value_at(a4,initial+i)}else {value_at(a5,initial+i)}};set_value(dst,state_base+i,value);}
            }
            if v7 {
                for row in width {
                    let base=state_base+row*width;
                    let sa=vector_dot(p.source[4],a4,output*4,4,p.dst,dst,base*4,4,width);
                    let value=value_at(a3,output+row);
                    for col in width {
                        let at=output+col;let kv=value*value_at(a2,at);let decayed=fma32(value_at(dst,base+col),value_at(a1,at),kv);
                        set_value(dst,base+col,fma32(sa,value_at(a5,at),decayed));
                    }
                    set_value(dst,output+row,vector_dot(p.dst,dst,base*4,4,p.source[0],a0,output*4,4,width));
                }
            }else {
                for col in width {set_value(dst,output+col,0 as f32);}
                for row in width {
                    let key=value_at(a0,output+row);let query=value_at(a2,output+row)*(if linear {p.fp[0]}else {1 as f32});
                    let decay=if linear {value_at(a3,output+row)}else {value_at(a4,output+row)};
                    let factor=if linear {0 as f32}else {value_at(a3,h*width+row)};
                    for col in width {
                        let at=state_base+row*width+col;let kv=value_at(a1,output+col)*key;let previous=value_at(dst,at);
                        let next=if col<simd_end {fma32(previous,decay,kv)}else {previous*decay+kv};
                        let weighted=if linear {next}else {if col<simd_end {fma32(kv,factor,previous)}else {kv*factor+previous}};
                        let result=if col<simd_end {fma32(weighted,query,value_at(dst,output+col))}else {value_at(dst,output+col)+weighted*query};
                        set_value(dst,at,next);set_value(dst,output+col,result);
                    }
                }
            }
        }
    }
    witness(p.op,scores);return true;
}

fn state_entry(p:const StatePlan&,a0:const u8[:],a1:const u8[:],a2:const u8[:],a3:const u8[:],a4:const u8[:],a5:const u8[:],a6:const u8[:],dst:u8[:])->bool {
    if recurrent_attention(p,a0,a1,a2,a3,a4,a5,a6,dst) {return true;}
    validate(p.dst,dst.len);validate(p.source[0],a0.len);validate(p.source[1],a1.len);validate(p.source[2],a2.len);validate(p.source[3],a3.len);validate(p.source[4],a4.len);validate(p.source[5],a5.len);
    if p.op==86 {return gdn(p,a0,a1,a2,a3,a4,a5,dst);}
    if p.op==77 {validate(p.source[6],a6.len);var scan:ScanPlan=default<ScanPlan>();scan.source=p.source;scan.ith=p.ith;scan.nth=p.nth;scan.snapshots=p.snapshots;assert(p.source[3].ne[0]==1||p.snapshots==1);return scan_entry(scan,a0,a1,a2,a3,a4,a5,a6,dst);}
    return false;
}
fn main(){var p:StatePlan=default<StatePlan>();let a:u8[1]=default<u8[1]>();var d:u8[1]=default<u8[1]>();let ok=state_entry(p,a,a,a,a,a,a,a,d);print(ok);}
