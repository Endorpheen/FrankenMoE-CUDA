from pathlib import Path
import re,json,subprocess,hashlib,shutil
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107');s=r/'source/moe-quantized-branchless';s.mkdir(parents=True,exist_ok=True)
parts=[]
for name in ['common','low','iq','dequant','entry']:
    src=r/'source/moe-quantized'/f'matrix-{name}.goose'
    dst=s/f'matrix-{name}.goose';shutil.copy2(src,dst)
    parts.append(dst.read_text())
code=''.join(parts)
old="""        positive[i]=(if w[i]<0 {-(w[i] as i16)} else {w[i] as i16}) as! u8;
        sy[i]=(if w[i]<0 {-(y[i] as i16)} else if w[i]==0 {0} else {y[i] as i16}) as! i8;"""
new="""        let wi=w[i] as i16;let yi=y[i] as i16;
        let mask=-(((w[i] as! u8)>>7) as i16);
        let nonzero=((wi|(-wi))>>15)&1;
        positive[i]=((wi^mask)-mask) as! u8;
        sy[i]=(((yi^mask)-mask)*nonzero) as! i8;"""
assert old in code;code=code.replace(old,new)
old='for i in 32 {out[i]=(x.q[part*8+i/4]>>(((i%4)*2) as u8))&3;}'
new='for i in 8 {let v=x.q[part*8+i];out[4*i]=v&3;out[4*i+1]=(v>>2)&3;out[4*i+2]=(v>>4)&3;out[4*i+3]=v>>6;}'
assert old in code;code=code.replace(old,new)
old='for i in 32 {lookup[i]=palette[i%16] as! u8;}'
new='assert(palette.len>=16);for i in 16 {let v=palette[i] as! u8;lookup[i]=v;lookup[i+16]=v;}'
assert old in code;code=code.replace(old,new)

code=code.replace('fn word32(bytes:u8[64],','fn word32(bytes:const u8[64]&,')
code=re.sub(r'(?<![\w.])\d+\.\d+(?!\w)',lambda m:'('+m[0]+' as f32)',code)
if (s/'matrix-generation.log').exists() and (s/'matrix-generation.log').read_text().strip() and (s/'matrix.goose').exists():
    n=1
    while (s/f'matrix-rejected-{n}.goose').exists():n+=1
    shutil.copy2(s/'matrix.goose',s/f'matrix-rejected-{n}.goose');shutil.copy2(s/'matrix-generation.log',s/f'matrix-rejected-{n}.log')
(s/'matrix.goose').write_text(code)
shutil.copy2(r/'source/cpu-moe/q4/packed_simd.goose',s/'packed_simd.goose')
producer=r/'compiler/stage3-7ab1a1ca1cdb'
assert hashlib.sha256(producer.read_bytes()).hexdigest()=='7ab1a1ca1cdb82bd45c447cafc9a8143cbcb23dab29bae3e813f66069c5d0846'
cmd=[str(producer),'-o',str(r/'generated/moe-matrix-branchless.c'),str(s/'matrix.goose')]
p=subprocess.run(cmd,cwd=r,capture_output=True,text=True)
(s/'matrix-generation.log').write_text(p.stdout+p.stderr);(s/'matrix-generation-command.json').write_text(json.dumps(cmd,indent=2)+'\n')
print('matrix generation',p.returncode,p.stdout[-1500:],p.stderr[-2500:],flush=True);p.check_returncode()
print('Goose lines',len(code.splitlines()),'generated C bytes',(r/'generated/moe-matrix-branchless.c').stat().st_size)
