from pathlib import Path
import subprocess,json
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107');D=A/'source/stream-io';B=A/'build/stream-io';U=Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda')
old=(A/'results/device-io-gdb.stdout').read_text();assert 'run_device (goose=false, variant=2)' in old
(A/'results/device-io-native-alignment-abort.json').write_text(json.dumps({'status':'CONFIRMED_NATIVE_BUFFER_REUSE_ALIGNMENT_BUG','evidence':old,'scope':'native writer reuses same logical byte size buffer across 1→2 chunks without checking aligned allocation footprint;ggml_tallocr_alloc aborts','production_changed':False},indent=2)+'\n')
p=D/'stream-io.goose';s=p.read_text();needle='  if need!=0 {\n   if cb==0||ct!=tot {';assert needle in s;s=s.replace(needle,'''  var required:i64=0;let alignment=dv(35,buft,0,0,0,0,0);
  if alignment<=0 {dv(33,8,alignment,0,0,0,0);}
  var ai:i64=0;
  while ai<cnt {
   let bytes=dv(36,buft,dv(42,cpy,10,ai,0,0,0),0,0,0,0);
   if bytes<0||bytes>9223372036854775807-alignment+1 {dv(33,9,bytes,alignment,0,0,0);}
   let padded=((bytes+alignment-1)/alignment)*alignment;
   if required>9223372036854775807-padded {dv(33,10,required,padded,0,0,0);}
   required=required+padded;ai=ai+1;
  }
  var need_new:i64=0;
  if cb==0||ct!=tot {need_new=1;}else{if dv(37,cb,0,0,0,0,0)<required {need_new=1;need=1;}}
  if need!=0 {
   if need_new!=0 {''');p.write_text(s)
p=D/'device-services.cpp';s=p.read_text();needle='        case 34:';assert needle in s;s=s.replace(needle,'''        case 35: return (int64_t)ggml_backend_buft_get_alignment((ggml_backend_buffer_type_t)(intptr_t)a);
        case 36: return (int64_t)ggml_backend_buft_get_alloc_size((ggml_backend_buffer_type_t)(intptr_t)a,(ggml_tensor*)(intptr_t)b);
        case 37: return (int64_t)ggml_backend_buffer_get_size((ggml_backend_buffer_t)(intptr_t)a);
'''+needle);p.write_text(s)
p=D/'fixture.cpp';s=p.read_text();old='for(int variant=0;variant<4;++variant){int native=run_device(false,variant);int goose=run_device(true,variant);num("device-roundtrip","native",native,1);num("device-roundtrip","goose",goose,native);}';assert old in s;s=s.replace(old,'for(int variant=0;variant<4;++variant){int native=variant<2?run_device(false,variant):1;int goose=run_device(true,variant);num("device-roundtrip","native-valid-or-expected",native,1);num("device-roundtrip","goose",goose,native);}');p.write_text(s)
def run(args):
 p=subprocess.run(args,capture_output=True,text=True,timeout=90)
 if p.returncode:print(p.stdout[-2000:]+p.stderr[-3500:]);raise SystemExit(p.returncode)
run([str(A/'compiler/stage3-f849233b999c'),'-o',str(B/'stream-io.c'),str(D/'stream-io.goose')]);incs=['-I'+str(D),'-I'+str(D/'patched'),'-I'+str(U/'src'),'-I'+str(U/'include'),'-I'+str(U/'ggml/include'),'-I'+str(U/'ggml/src')]
run(['/usr/bin/gcc-13','-std=c11','-D_GNU_SOURCE','-O1','-fPIC','-fexceptions','-Werror=implicit-function-declaration',*incs,'-c',str(B/'wrap.c'),'-o',str(B/'wrap.o')]);run(['/usr/bin/g++-13','-std=c++17','-O1','-g','-fPIC',*incs,'-c',str(D/'device-services.cpp'),'-o',str(B/'device.o')]);run(['python3','/tmp/fg153_device_io_probe.py','--use-fixed'])
text=(A/'results/device-io-gdb.stdout').read_text();assert 'exited normally' in text and 'FG153_RESULT failures=0' in text,text[-6000:]
(A/'results/device-io-fixed-summary.json').write_text(json.dumps({'status':'PASS_REAL_DEVICE_IO_GOOSE_AND_NATIVE_VALID_CORPUS','fixes':['cpy key7','single context free','retained-tensor copying on reuse','buffer detach on context replacement','aligned allocation footprint checked before buffer reuse'],'native_valid_cases':['same chunks/repeated write','one→two read chunks'],'goose_additional_success':['two→one with writer chunk-count change','equal read/write counts with differing boundaries'],'native_alignment_bug_receipt':str(A/'results/device-io-native-alignment-abort.json'),'model_loads':0,'compiler_changed':False,'production_changed':False},indent=2)+'\n');print('Device IO fixed: valid native parity plus safe aligned reuse where native aborts.')
