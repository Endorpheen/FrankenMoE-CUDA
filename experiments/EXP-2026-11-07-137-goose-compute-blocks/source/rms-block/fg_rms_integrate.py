from pathlib import Path
import re,json,subprocess,hashlib
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107'); s=r/'source/rms-block'; c=(r/'generated/rms-block.c').read_text()
def extract(name):
 m=re.search(r'static [^\n;]*\b'+name+r'\([^;\n]*\) \{',c); assert m,name
 i=m.end();depth=1
 while depth:
  depth+=(c[i]=='{')-(c[i]=='}');i+=1
 return c[m.start():i]
def device(body):return body.replace('static ','static __device__ __forceinline__ ',1)
types=c[c.index('typedef struct gg_type_10'):c.index('static void gg_quote')]
header='''#pragma once
#include <stdint.h>
#include <limits.h>
#include <string.h>
typedef struct { void *data; int64_t len; } gg_slice;
static __device__ __forceinline__ void gg_fail(const char *) { asm volatile("trap;"); }
'''+types
for n in ['gg_index','gg_div','gg_mod','gg_double_bits']:header+=device(extract(n))+'\n'
header+='''static_assert(sizeof(gg_type_10)==sizeof(float4), "F4 ABI");
static __device__ __forceinline__ gg_type_10 fg_rms_load4(gg_slice x, int32_t i) {
    const float4 v = ((const float4 *)x.data)[i];
    return {v.x,v.y,v.z,v.w};
}
static __device__ __forceinline__ void fg_rms_store4(gg_slice x, int32_t i, gg_type_10 v) {
    ((float4 *)x.data)[i] = make_float4(v.f0,v.f1,v.f2,v.f3);
}
static __device__ __forceinline__ float fg_rms_shuffle(float x, int32_t offset) { return __shfl_xor_sync(0xffffffff,x,offset,32); }
static __device__ __forceinline__ void fg_rms_barrier() { __syncthreads(); }
static __device__ __forceinline__ float fg_rms_rsqrt(float x) { return rsqrtf(x); }
static __device__ __forceinline__ float fg_rms_fma(float x,float y,float z) { return fmaf(x,y,z); }
'''
for n in ['gg_function_4','gg_function_3','gg_function_1']:
 body=extract(n);assert not any(x in body for x in ['gg_get_stack','gg_allocate','gg_stacks']);header+=device(body)+'\n'
(r/'generated/goose_rms_block.cuh').write_text(header)
base=(r/'source/ggml/src/ggml-cuda/norm.cu').read_text()
start=base.index('template <int block_size, int vpt, bool do_vec');end=base.index('template <int block_size>\nstatic __global__ void rms_norm_back',start)
kernel=base[start:end].replace('void rms_norm_f32(', 'void rms_norm_goose_f32(',1)
a=kernel.index('            if constexpr (do_vec)'); b=kernel.index('\n        }\n    }\n}',a)
kernel=kernel[:a]+'''            gg_function_1({(void *)xc,ncols}, {dstc,ncols}, {s_sum,32},
                {(void *)mulc,do_multiply ? mul_ncols_packed.z : 0},
                {(void *)addc,do_add ? add_ncols_packed.z : 0},
                ncols,tid,block_size,vpt,do_vec,do_multiply,do_add,do_scale,
                mul_ncols_packed.z,add_ncols_packed.z,eps,scale_out,0);
#ifdef FG_RMS_TRACE
            if (tid==0) {
                atomicAdd(&fg_rms_counts[0],1ULL);
                if constexpr (do_vec) atomicAdd(&fg_rms_counts[1],1ULL);
                if constexpr (do_multiply) atomicAdd(&fg_rms_counts[2],1ULL);
                if constexpr (do_add) atomicAdd(&fg_rms_counts[3],1ULL);
                if constexpr (do_scale) atomicAdd(&fg_rms_counts[4],1ULL);
            }
#endif
'''+kernel[b:]
prefix='''#include "'''+str(r/'generated/goose_rms_block.cuh')+'''"
#include <cstdlib>
#include <cstring>
static bool fg_rms_enabled() {
    static const bool enabled = !std::getenv("GGML_GOOSE_RMS") || std::strcmp(std::getenv("GGML_GOOSE_RMS"),"0");
    return enabled;
}
#ifdef FG_RMS_TRACE
static __device__ unsigned long long fg_rms_counts[5];
extern "C" int fg_rms_read_counters(unsigned long long *out) {
    return (int)cudaMemcpyFromSymbol(out,fg_rms_counts,5*sizeof(*out));
}
#include <atomic>
#include <thread>
#include <chrono>
#include <cstdio>
static std::atomic<bool> fg_rms_sampling{true};
static std::thread *fg_rms_thread=nullptr;
__attribute__((constructor)) static void fg_rms_start() {
    fg_rms_thread=new std::thread([] {
        while(fg_rms_sampling.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            if(!fg_rms_sampling.load()) break;
            const char *path="/home/end0/AI/LlamaServer/franken-goose-live-20261107/results/read-rms-block-counter.request";
            FILE *f=fopen(path,"r");if(!f)continue;fclose(f);remove(path);
            unsigned long long out[5]={};
            const auto status=cudaMemcpyFromSymbol(out,fg_rms_counts,sizeof(out));
            fprintf(stderr,"FG_SELFHOST_RMS_BLOCK rows=%llu vec=%llu mul=%llu add=%llu scale=%llu CUDA_STATUS=%d\\n",out[0],out[1],out[2],out[3],out[4],(int)status);fflush(stderr);
        }
    });
}
__attribute__((destructor)) static void fg_rms_stop() {
    fg_rms_sampling.store(false);if(fg_rms_thread){fg_rms_thread->join();delete fg_rms_thread;}
}
#endif
'''
new=base[:start]+prefix+base[start:end]+kernel+base[end:]
# Preserve the exact native launch and provide an independent native rollback.
a=new.index('#define RMS_NORM_LAUNCH');b=new.index('\ntemplate <bool do_multiply',a);macro=new[a:b]
callstart=macro.index('        ggml_cuda_kernel_launch(');callend=macro.index('    } while (0)')
call=macro[callstart:callend]
replacement=macro[:callstart]+'        if (fg_rms_enabled()) { \\\n'+call.replace('rms_norm_f32<','rms_norm_goose_f32<')+'        } else { \\\n'+call+'        } \\\n'+macro[callend:]
new=new[:a]+replacement+new[b:]
(s/'norm-goose-block.cu').write_text(new)
args=json.loads((r/'build/compile-norm.json').read_text());args[args.index('-o')+1]=str(s/'goose-rms.o');args[args.index('-c')+1]=str(s/'norm-goose-block.cu');args+=['-I'+str(r/'source/ggml/src/ggml-cuda')]
for trace in [False,True]:
 out=s/('goose-rms-trace.o' if trace else 'goose-rms.o');cmd=args.copy();cmd[cmd.index('-o')+1]=str(out)
 if trace:cmd+=['-DFG_RMS_TRACE=1']
 (s/('compile-trace.json' if trace else 'compile.json')).write_text(json.dumps(cmd,indent=2));p=subprocess.run(cmd,cwd=r,capture_output=True,text=True);(s/('compile-trace.log' if trace else 'compile.log')).write_text(p.stdout+p.stderr);print('compile',trace,p.returncode,(p.stdout+p.stderr)[-2000:],flush=True);p.check_returncode()
 lib=r/'build'/('libfranken-goose-rms-block-trace.so' if trace else 'libfranken-goose-rms-block.so')
 link=['/usr/bin/nvcc','-shared',str(out),'-L'+str(r/'run'),'-lggml-cuda','-lggml-base','-Xlinker','-rpath','-Xlinker',str(r/'run'),'-o',str(lib)]
 p=subprocess.run(link,capture_output=True,text=True);(s/('link-trace.log' if trace else 'link.log')).write_text(p.stdout+p.stderr);p.check_returncode();print('linked',lib,flush=True)
(s/'generation-manifest.json').write_text(json.dumps({'producer':json.loads((r/'compiler/current-producer.json').read_text()),'sha256':{str(p.relative_to(r)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [s/'kernel.goose',r/'generated/rms-block.c',r/'generated/goose_rms_block.cuh',s/'norm-goose-block.cu']},'adaptations':'Extract reachable emitted C bodies unchanged; add device forceinline qualifiers; trap for failure, CUDA load4/store4/shuffle/barrier/rsqrt intrinsics; preserve native launch and outer row/channel/sample address computation. No original C++ computational function called by Goose.'},indent=2)+'\n')
