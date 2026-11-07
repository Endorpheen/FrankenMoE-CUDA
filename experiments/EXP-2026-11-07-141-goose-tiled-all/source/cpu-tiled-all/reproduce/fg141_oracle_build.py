from pathlib import Path
import json,subprocess
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107');s=r/'source/cpu-tiled-all';v=r/'variants/cpu-tiled-all';qsrc=Path('/home/end0/AI/LlamaServer/franken-qwen38-v31/source/ggml');native=r/'variants/qwen839a-shared/build';names=['q4_K','q5_K','q6_K','q3_K','q2_K','iq4_xs','iq2_xxs','iq2_xs','iq2_s','iq3_xxs','iq3_s','iq1_s','iq1_m'];subs=[32,32,16,16,16,32,32,16,16,32,32,32,16];bias=[0,0,32,4,0,128,128,128,128,128,128,128,128];act=[0,0,1,1,0,0,1,1,1,1,1,1,1]
code='''#define ggml_tiled_wdata_size fg141_oracle_wdata_size
#define ggml_compute_forward_mul_mat_tiled fg141_oracle_dense_tiled
#define ggml_compute_forward_mul_mat_id_tiled fg141_oracle_moe_tiled
#include "'''+str(qsrc/'src/ggml-cpu/tiled/tiled.cpp')+'''"
extern "C" void fg141_oracle_init(void){ggml_cpu_init();}
extern "C" size_t fg141_oracle_workspace_size(void){return sizeof(tiled_ws);}
extern "C" void fg141_oracle_unpack(int kind,const void*data,int64_t stride,int64_t first,int64_t start,int count,int nk,void*workspace){
 auto*w=(tiled_ws*)workspace;
 switch(kind){
'''
for i,name in enumerate(names):code+=f'case {i}:{{const auto*x=(const block_{name}*)((const char*)data+first*stride+start*sizeof(block_{name}));tiled_unpack_src0(x,stride/sizeof(block_{name}),count,&w->src0,nk);tiled_repack_src0<{subs[i]}>(&w->src0,count,nk,{bias[i]},{"true" if act[i] else "false"});break;}}\n'
code+='default:abort();}}\n';code+=Path('/tmp/fg141_oracle_extra.cpp').read_text();(s/'oracle.cpp').write_text(code)
cmd=['/usr/bin/g++-13','-O3','-march=native','-fPIC','-std=c++17','-I'+str(qsrc/'src'),'-I'+str(qsrc/'src/ggml-cpu'),'-I'+str(qsrc/'include'),'-shared',str(s/'oracle.cpp'),str(native/'ggml/src/CMakeFiles/ggml-cpu.dir/ggml-cpu/tiled/tiled-kernel.cpp.o'),'-L'+str(native/'bin'),'-lggml-cpu','-lggml-base','-lm','-lpthread','-Wl,-rpath,'+str(native/'bin'),'-o',str(v/'build/libnative-tiled-oracle.so')];p=subprocess.run(cmd,capture_output=True,text=True);(v/'build/oracle-compile.log').write_text(p.stdout+p.stderr);(v/'build/oracle-command.json').write_text(json.dumps(cmd,indent=2)+'\n');print('native oracle',p.returncode,p.stderr[-2200:],flush=True);p.check_returncode()
