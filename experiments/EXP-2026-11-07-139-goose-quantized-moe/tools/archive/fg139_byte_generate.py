from pathlib import Path
import re,json,subprocess,shutil,hashlib
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107');s=r/'source/moe-quantized-byte';s.mkdir(parents=True,exist_ok=True)
(s/'hypothesis.json').write_text(json.dumps({'experiment':'EXP-2026-11-07-139','hypothesis':'Use native-equivalent OR/shift/mask word expansion for Q2_0, and byte-width wrapping sign/nonzero masks for Q5_0/IQ4_NL. All compute remains Goose; retain checks, FP arithmetic and prior negative variants.','control':'Branchless matrix with same increased GCC inline budget','producer_sha256':'7ab1a1ca1cdb82bd45c447cafc9a8143cbcb23dab29bae3e813f66069c5d0846','compiler_changed':False},indent=2)+'\n')
code=(r/'source/moe-quantized-branchless/matrix.goose').read_text()
a=code.index('fn signed_product(');b=code.index('fn pair_scale(',a)
code=code[:a]+'''fn signed_product(w:i8[32],y:i8[32])->i32[8] {
    var positive:u8[32]=default<u8[32]>();var sy:i8[32]=default<i8[32]>();
    for i in 32 {
        let wb=w[i] as! u8;let yb=y[i] as! u8;
        let mask=((0 as u8)-(wb>>7)) as! u8;
        let neg=((0 as u8)-wb) as! u8;
        let nzmask=((0 as u8)-((wb|neg)>>7)) as! u8;
        positive[i]=((wb^mask)-mask) as! u8;
        sy[i]=((((yb^mask)-mask) as! u8)&nzmask) as! i8;
    }
    return packed_simd::madd16(packed_simd::splat16(1),packed_simd::maddubs(positive,sy));
}
''' + code[b:]
a=code.index('fn q2_codes(');b=code.index('fn q2_acc(',a)
code=code[:a]+'''fn q2_codes(x:const Q2&,part:i64)->u8[32] {
    var out:u8[32]=default<u8[32]>();
    for i in 8 {
        var v=x.q[part*8+i] as u32;v=v|(v<<12);v=v|(v<<6);v=v&0x03030303;
        out[4*i]=v as! u8;out[4*i+1]=(v>>8) as! u8;
        out[4*i+2]=(v>>16) as! u8;out[4*i+3]=(v>>24) as! u8;
    }
    return out;
}
''' + code[b:]
(s/'matrix.goose').write_text(code);shutil.copy2(r/'source/moe-quantized/packed_simd.goose',s/'packed_simd.goose');producer=r/'compiler/stage3-7ab1a1ca1cdb';assert hashlib.sha256(producer.read_bytes()).hexdigest()=='7ab1a1ca1cdb82bd45c447cafc9a8143cbcb23dab29bae3e813f66069c5d0846';cmd=[str(producer),'-o',str(r/'generated/moe-matrix-byte.c'),str(s/'matrix.goose')];p=subprocess.run(cmd,capture_output=True,text=True,cwd=r);(s/'generation.log').write_text(p.stdout+p.stderr);(s/'generation-command.json').write_text(json.dumps(cmd,indent=2)+'\n');print('generation',p.returncode,p.stdout[-500:],p.stderr[-1500:],flush=True);p.check_returncode()
