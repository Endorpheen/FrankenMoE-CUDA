from pathlib import Path
import re,json,subprocess,hashlib,shutil
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107')
s=r/'source/moe-quantized';s.mkdir(exist_ok=True)
base=(r/'source/q8k-fixed/kernel.goose').read_text().split('fn main()')[0]
extra='''struct Float32Block { values:f32[32] }
struct Q8Zero { d:u16, qs:i8[32] }
extern "fg139_float_half" fn to_half(x:f32)->u16;
extern "fg139_float_from_bits" fn from_bits(x:u32)->f32;
extern "fg139_quant_witness" fn witness(kind:i64,blocks:i64);
fn maximum(a:f32,b:f32)->f32 {return if a>b {a} else {b};}
fn absolute(x:f32)->f32 {return from_bits(float_bits(x)&0x7fffffff);}
fn nearest_saturated(x:f32)->i8 {
    if (float_bits(x)&0x7fffffff)>=0x4f000000 {return -128 as i8;}
    var whole:i32=x as i32;
    let fraction=x-(whole as f32);
    if fraction>0.5||(fraction==0.5&&(whole&1)!=0) {whole+=1;}
    if fraction<(-0.5)||(fraction==(-0.5)&&(whole&1)!=0) {whole-=1;}
    return (if whole>127 {127} else if whole<(-128) {-128} else {whole}) as! i8;
}
fn quantize_zero(x:const Float32Block[:],y:Q8Zero[:]) {
    assert(x.len>=y.len);
    for ib in y.len {
        let input=&x[ib];let out:Q8Zero& = &y[ib];
        var lanes:f32[8]=default<f32[8]>();
        for lane in 8 {
            let a=maximum(absolute(input.values[lane]),absolute(input.values[8+lane]));
            let b=maximum(a,absolute(input.values[16+lane]));
            lanes[lane]=maximum(b,absolute(input.values[24+lane]));
        }
        let a=maximum(maximum(lanes[4],lanes[0]),maximum(lanes[6],lanes[2]));
        let b=maximum(maximum(lanes[5],lanes[1]),maximum(lanes[7],lanes[3]));
        let magnitude=maximum(a,b);
        out.d=to_half(magnitude/127.0);
        let inverse:f32=if magnitude!=0.0 {127.0/magnitude} else {0.0};
        for lane in 32 {out.qs[lane]=nearest_saturated(input.values[lane]*inverse);}
    }
}
fn activation(kind:i64,x32:const Float32Block[:],x256:const FloatBlock[:],y0:Q8Zero[:],yk:Q8Block[:]) {
    if kind==0 {quantize_zero(x32,y0);witness(0,y0.len);}
    else {assert(kind==1);goose_quantize_q8k(x256,yk);witness(1,yk.len);}
}
fn main() {
    let x32:Float32Block[1]=default<Float32Block[1]>();let x256:FloatBlock[1]=default<FloatBlock[1]>();
    var y0:Q8Zero[1]=default<Q8Zero[1]>();var yk:Q8Block[1]=default<Q8Block[1]>();
    activation(0,x32,x256,y0,yk);print(y0[0].d);
}
'''
code=base+extra
if (s/'activation-generation.log').exists() and 'defined wrapping float-to-integer conversion' in (s/'activation-generation.log').read_text() and not (s/'activation-v1-rejected-cast.goose').exists():
    shutil.copy2(s/'activation.goose',s/'activation-v1-rejected-cast.goose')
    shutil.copy2(s/'activation-generation.log',s/'activation-v1-rejected-cast.log')
if (s/'activation-generation.log').exists() and 'defined wrapping float-to-integer conversion' in (s/'activation-generation.log').read_text() and 'x as! i32' in (s/'activation.goose').read_text() and not (s/'activation-v2-rejected-castbang.goose').exists():
    shutil.copy2(s/'activation.goose',s/'activation-v2-rejected-castbang.goose')
    shutil.copy2(s/'activation-generation.log',s/'activation-v2-rejected-castbang.log')
truncation='''fn trunc_valid(x:f32)->i32 {
    let raw=float_bits(x);
    let exponent=(((raw>>23)&255) as i32)-127;
    if exponent<0 {return 0;}
    let mantissa=(raw&0x007fffff)|0x00800000;
    let value=if exponent>=23 {mantissa<<((exponent-23) as u32)} else {mantissa>>((23-exponent) as u32)};
    return if (raw&0x80000000)!=0 {-(value as i32)} else {value as i32};
}
'''
code=code.replace('fn nearest_saturated(',truncation+'fn nearest_saturated(',1)
code=code.replace('var whole:i32=x as i32;','var whole:i32=trunc_valid(x);')
code=re.sub(r'(?<![\w.])\d+\.\d+(?!\w)',lambda m:'('+m[0]+' as f32)',code)
(s/'activation.goose').write_text(code)
producer=r/'compiler/stage3-7ab1a1ca1cdb';sha=hashlib.sha256(producer.read_bytes()).hexdigest()
assert sha=='7ab1a1ca1cdb82bd45c447cafc9a8143cbcb23dab29bae3e813f66069c5d0846'
cmd=[str(producer),'-o',str(r/'generated/moe-activation.c'),str(s/'activation.goose')]
p=subprocess.run(cmd,cwd=r,capture_output=True,text=True)
(s/'activation-generation.log').write_text(p.stdout+p.stderr)
(s/'activation-generation-command.json').write_text(json.dumps(cmd,indent=2)+'\n')
print('activation generation',p.returncode,p.stdout[-1000:],p.stderr[-2000:],flush=True)
p.check_returncode()
