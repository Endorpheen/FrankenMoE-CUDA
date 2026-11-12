from pathlib import Path
import shutil

r = Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107')
s = r / 'source/cpu-backend'
archive = s / 'before-recurrent-extension'
archive.mkdir(exist_ok=True)
for relative in ['stateful/stateful.goose', 'native-bridge.cpp', 'composite-fixture.c']:
    target = archive / relative
    target.parent.mkdir(exist_ok=True)
    shutil.copy2(s / relative, target)
for name in ['cpu-backend-composite-summary.json', 'cpu-backend-composite-sanitizer-summary.json', 'cpu-backend-composite-candidate.log']:
    shutil.copy2(r / 'results' / name, archive / name)

path = Path('/tmp/fg143_stateful.goose')
text = path.read_text()
marker = 'fn state_entry('
assert text.count(marker) == 1 and 'fn recurrent_attention(' not in text
text = text.replace(marker, Path('/tmp/fg143_wkv.goose').read_text() + '\n' + marker)
signature = 'dst:u8[:])->bool {\n    validate(p.dst,dst.len);'
assert text.count(signature) == 1
text = text.replace(signature, 'dst:u8[:])->bool {\n    if recurrent_attention(p,a0,a1,a2,a3,a4,a5,a6,dst) {return true;}\n    validate(p.dst,dst.len);')
path.write_text(text)

path = s / 'native-bridge.cpp'
text = path.read_text()
old = 'if(t->op==GGML_OP_GATED_DELTA_NET||t->op==GGML_OP_SSM_SCAN)'
new = 'if(t->op==GGML_OP_GATED_DELTA_NET||t->op==GGML_OP_SSM_SCAN||t->op==GGML_OP_RWKV_WKV6||t->op==GGML_OP_RWKV_WKV7||t->op==GGML_OP_GATED_LINEAR_ATTN)'
assert text.count(old) == 1
text = text.replace(old, new)
old = 'state.snapshots=t->op_params[0];return fg143_state_compute'
new = 'state.snapshots=t->op_params[0];for(int i=0;i<4;++i)state.fp[i]=fg143_float(t,i);return fg143_state_compute'
assert text.count(old) == 1
path.write_text(text.replace(old, new))

path = Path('/tmp/fg143_fixture.c')
text = path.read_text()
marker = ' const enum ggml_type compact[]='
assert text.count(marker) == 1
fixture = '''
 for(int op=0;op<3;++op)for(int variant=0;variant<4;++variant){begin();int width=op==2?(variant&1?64:32):(variant&1?17:8),heads=3,ns=variant&2?2:1,nt=variant&2?6:2;
 struct ggml_tensor*inputs[7];for(int i=0;i<7;++i){inputs[i]=make(GGML_TYPE_F32,width,heads,nt,1);fill(inputs[i],0);for(int64_t j=0;j<ggml_nelements(inputs[i]);++j)((float*)inputs[i]->data)[j]*=.15f;}
 struct ggml_tensor*state=make(GGML_TYPE_F32,width*width*heads,ns,1,1);fill(state,0);
 struct ggml_tensor*t;
 if(op==0){struct ggml_tensor*tf=make(GGML_TYPE_F32,width,heads,1,1);fill(tf,0);t=ggml_rwkv_wkv6(ctx,inputs[0],inputs[1],inputs[2],tf,inputs[4],state);}
 else if(op==1)t=ggml_gated_linear_attn(ctx,inputs[0],inputs[1],inputs[2],inputs[3],state,.375f);
 else t=ggml_rwkv_wkv7(ctx,inputs[0],inputs[1],inputs[2],inputs[3],inputs[4],inputs[5],state);
 memset(t->data,0,ggml_nbytes(t));char name[100];snprintf(name,sizeof(name),"recurrent-op%d-width%d-seqs%d-t%d",op,width,ns,threads);run(name,t,threads);end();}
'''
path.write_text(text.replace(marker, fixture + marker))
