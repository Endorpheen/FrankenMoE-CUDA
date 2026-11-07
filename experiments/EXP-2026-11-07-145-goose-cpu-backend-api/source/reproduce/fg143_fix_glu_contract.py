from pathlib import Path
import hashlib
import json
import shutil
import struct

r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107')
s=r/'source/cpu-backend'
archive=r/'diagnostics/b1-326-before-glu-numeric-contract'
archive.mkdir(parents=True,exist_ok=True)
for name in ['pointwise.goose','native-bridge.cpp']:
    shutil.copy2(s/name,archive/name)
for name in ['cpu-backend-composite-summary.json','cpu-backend-apex-contract-summary.json','cpu-backend-composite-sanitizer-summary.json']:
    shutil.copy2(r/'results'/name,archive/name)
fragment=Path('/tmp/fg143_simd_exp.goose').read_text()
constants={'C_LOG2E':'0x1.715476p+0','C_LN2_HI':'0x1.62e4p-1','C_LN2_LO':'0x1.7f7d1cp-20',
           'C_P0':'0x1.0e4020p-7','C_P1':'0x1.573e2ep-5','C_P2':'0x1.555e66p-3',
           'C_P3':'0x1.fffdb6p-2','C_P4':'0x1.ffffecp-1'}
for name,value in constants.items():
    bits=struct.unpack('<I',struct.pack('<f',float.fromhex(value)))[0]
    fragment=fragment.replace(name,str(bits))
p=s/'pointwise.goose';text=p.read_text();assert 'fn avx2_exp_compat(' not in text
marker='fn unary_value(';assert text.count(marker)==1;text=text.replace(marker,fragment+'\n'+marker)
old='else if op==GLU_OP_SWIGLU {value=silu(a)*b;}'
assert text.count(old)==1
text=text.replace(old,'else if op==GLU_OP_SWIGLU {value=silu_compat(p,a,col,width)*b;}')
p.write_text(text)
p=s/'native-bridge.cpp';text=p.read_text();old='p.ip[13]=1;'
assert text.count(old)==1
text=text.replace(old,old+'''\n#if defined(__AVX2__) && defined(__FMA__)
 p.ip[12]=1;
#else
 p.ip[12]=0;
#endif
''')
p.write_text(text)
(s/'glu-numerical-contract.json').write_text(json.dumps({'status':'IMPLEMENTED_FUNCTIONAL_GATE_PENDING',
    'purpose':'Preserve original AVX2 F32 SWIGLU numerical contract,not manual speed tuning',
    'native_source':'ggml/src/ggml-cpu/vec.h AVX2 ggml_v_expf/ggml_v_silu;vec.cpp ggml_vec_swiglu_f32',
    'native_vec_sha256':hashlib.sha256((r/'source/ggml/src/ggml-cpu/vec.h').read_bytes()).hexdigest(),
    'method':'Same FMA polynomial/float-bit exponent construction for full8lane chunks,scalar expf tail; all arithmetic Goose. Metadata selects pinned AVX2/FMA contract.',
    'scope':'SWIGLU finite F32 precision compatibility. Other activations,NaN payloads,ISA modes and whole Apex probability drift remain open.',
    'no_model_run':True,'no_microbenchmarks':True,'compiler_changed':False,'production_changed':False},indent=2)+'\n')
