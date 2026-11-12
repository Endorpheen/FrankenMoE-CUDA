from pathlib import Path
import json,hashlib,subprocess,shutil
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107');parent=r/'source/cpu-backend';s=parent/'codec';s.mkdir(exist_ok=True);b=r/'variants/cpu-backend/build';shutil.copy2(r/'source/cpu-tiled-all/packed_simd.goose',s/'packed_simd.goose')
for name in ['common.goose','unpack.goose']:shutil.copy2(r/'source/cpu-tiled-all'/name,s/name)
shutil.copy2('/tmp/fg143_codec_entry.goose',s/'entry.goose');text='\n'.join((s/n).read_text() for n in ['common.goose','unpack.goose','entry.goose']);(s/'kernel.goose').write_text(text);producer=json.loads((parent/'producer.json').read_text());cmd=[producer['frozen'],'-o',str(r/'generated/cpu-backend-codec.c'),str(s/'kernel.goose')];p=subprocess.run(cmd,capture_output=True,text=True,timeout=90);(s/'generation.log').write_text(p.stdout+p.stderr);(s/'generation-command.json').write_text(json.dumps(cmd,indent=2)+'\n');print('common quant codec generation',p.returncode,(p.stdout+p.stderr)[-1500:],flush=True)
if p.returncode:raise SystemExit(p.returncode)
unit='''#define GGML_COMMON_DECL_C
#define GGML_COMMON_IMPL_C
#include "ggml-common.h"
#include "ggml-quants.h"
#include <immintrin.h>
#include <stdint.h>
#include <string.h>
#include <stdatomic.h>
#include <stdio.h>
static float fg141_half(uint16_t x){return _cvtsh_ss(x);}
static float fg143_codec_from_bits(uint32_t x){float f;memcpy(&f,&x,4);return f;}
static atomic_uint_fast64_t codec_calls[43];
static void fg141_witness(int64_t s,int64_t k,int64_t n){(void)s;(void)k;(void)n;}
#define main fg143_codec_unused_main
#include "../../../generated/cpu-backend-codec.c"
#undef main
uint8_t fg143_decode_row(int64_t type,const void*raw,int64_t bytes,float*out,int64_t n){uint8_t ok=gg_function_1(type,gg_view((void*)raw,bytes),gg_view(out,n),gg_view((void*)iq2xxs_grid,256),gg_view((void*)iq2xs_grid,512),gg_view((void*)iq2s_grid,1024),gg_view((void*)iq3xxs_grid,256),gg_view((void*)iq3s_grid,512),gg_view((void*)iq1s_grid,NGRID_IQ1S),gg_view((void*)ksigns_iq2xs,128),gg_view((void*)kvalues_iq4nl,16),0);if(ok&&type>=0&&type<43)atomic_fetch_add_explicit(codec_calls+type,1,memory_order_relaxed);return ok;}
__attribute__((destructor))static void fg143_codec_report(void){for(int t=0;t<43;++t){uint64_t n=atomic_load(codec_calls+t);if(n)fprintf(stderr,"FG143_CODEC type=%d calls=%llu\\n",t,(unsigned long long)n);}}
''';(s/'unit.c').write_text(unit);cmd=['/usr/bin/gcc-13','-O3','-march=native','-fPIC','-fno-strict-aliasing','-ffp-contract=off','-I'+str(r/'source/ggml/src'),'-I'+str(r/'source/ggml/include'),'-c',str(s/'unit.c'),'-o',str(b/'codec.o')];p=subprocess.run(cmd,capture_output=True,text=True);(b/'codec-build.log').write_text(p.stdout+p.stderr);(b/'codec-command.json').write_text(json.dumps(cmd,indent=2)+'\n');print('common quant codec build',p.returncode,(p.stdout+p.stderr)[-1500:],flush=True)
if p.returncode:raise SystemExit(p.returncode)
(s/'manifest.json').write_text(json.dumps({'status':'GENERATED_AND_BUILT_FUNCTIONAL_INTEGRATION_PENDING','producer':producer,'goose_sha256':hashlib.sha256((s/'kernel.goose').read_bytes()).hexdigest(),'reuse':'Same previously verified13format tile unpack source; complete compact legacy/FP4/Q8 block decoding added in Goose','native_compute_imports':False,'data_imports':'Immutable GGML grid/palette tables; primitive half/bitcast/math helpers only','performance_tests':False,'qwen_development_or_tests':False},indent=2)+'\n')
