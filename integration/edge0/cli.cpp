// Diagnostic host for the first complete Edge0 inference path, not a server.
#include "llama.h"
#include "ggml-backend.h"
#include <chrono>
#include <fstream>
#include <iterator>
#include <vector>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <sys/resource.h>
#include <dlfcn.h>
#include "pack.h"
static void log_io(const char * phase) {
    std::ifstream in("/proc/self/io");std::string key;unsigned long long v;
    std::fprintf(stderr,"EDGE0_IO phase=%s",phase);
    while(in>>key>>v)std::fprintf(stderr," %s%llu",key.c_str(),v);
    std::fprintf(stderr,"\n");
}
static double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
static void check(bool ok,const char * msg) { if(!ok) throw std::runtime_error(msg); }
int main(int argc,char ** argv) {
 try {
    check(argc==4,"usage: edge0-cli PACK PROMPT_FILE MAX_NEW_TOKENS");
    setenv("FRANKEN_EDGE0_PACK",argv[1],1);
    std::ifstream f(argv[2]); check(f.good(),"prompt file unavailable");
    std::string prompt((std::istreambuf_iterator<char>(f)),{});
    int limit=std::stoi(argv[3]); check(limit>0 && limit<=256,"output limit 1..256");
    ggml_backend_load_all(); llama_backend_init(); double load_start=now();
    auto mp=llama_model_default_params(); mp.n_gpu_layers=48; if (const char * ngl=std::getenv("FRANKEN_EDGE0_NGL")) mp.n_gpu_layers=std::stoi(ngl); mp.load_mode=LLAMA_LOAD_MODE_MMAP; mp.check_tensors=false;
    std::string model_path=std::string(argv[1])+"/model-skeleton.gguf";
    auto * m=llama_model_load_from_file(model_path.c_str(),mp); check(m!=nullptr,"model load failed");
    double loaded=now(); const auto * vocab=llama_model_get_vocab(m);
    auto cp=llama_context_default_params(); cp.n_ctx=512; cp.n_batch=1; cp.n_ubatch=1; cp.n_seq_max=1;
    cp.n_threads=8; cp.n_threads_batch=8; cp.flash_attn_type=LLAMA_FLASH_ATTN_TYPE_DISABLED;
    auto * c=llama_init_from_model(m,cp); check(c!=nullptr,"context initialization failed");
    std::vector<llama_token> tokens(prompt.size()+16);
    int n=llama_tokenize(vocab,prompt.data(),prompt.size(),tokens.data(),tokens.size(),true,true);
    check(n>0 && n+limit<=512,"prompt tokens do not fit context"); tokens.resize(n);
    auto * sampler=llama_sampler_init_greedy(); double t0=now(),first=0,decode_start=0; int generated=0,decode_steps=0; bool eos=false;
    std::fprintf(stderr,"EDGE0_BEGIN load_s=%.6f context_s=%.6f input_tokens=%d max_new=%d\n",loaded-load_start,t0-loaded,n,limit);
    log_io("prefill-start");franken_edge0_profile_phase(1);
    for (int i=0;i<n;++i) {
        auto tok=tokens[i]; check(llama_decode(c,llama_batch_get_one(&tok,1))==0,"prefill failed");
        std::fprintf(stderr,"EDGE0_PREFILL token=%d/%d elapsed_s=%.3f\n",i+1,n,now()-t0);
    }
    llama_synchronize(c); double prefill_end=now();
    log_io("decode-start");franken_edge0_profile_phase(2);
    const bool profile=std::getenv("FRANKEN_EDGE0_PROFILE")!=nullptr;
    using profiler_fn=int(*)();
    auto profiler_start=(profiler_fn)dlsym(RTLD_DEFAULT,"cudaProfilerStart");
    auto profiler_stop=(profiler_fn)dlsym(RTLD_DEFAULT,"cudaProfilerStop");
    if(profile && profiler_start) std::fprintf(stderr,"EDGE0_CUDA_PROFILE_START status=%d\n",profiler_start());
    for(int i=0;i<limit;++i) {
        auto tok=llama_sampler_sample(sampler,c,-1); llama_sampler_accept(sampler,tok);
        if(llama_vocab_is_eog(vocab,tok)) { eos=true; break; }
        char buf[1024]; int len=llama_token_to_piece(vocab,tok,buf,sizeof(buf),0,true); check(len>=0,"token piece buffer too short");
        if(!generated) { first=now(); decode_start=first; }
        std::fwrite(buf,1,len,stdout); std::fflush(stdout); ++generated;
        std::fprintf(stderr,"EDGE0_TOKEN index=%d id=%d elapsed_s=%.3f\n",generated,tok,now()-t0);
        if (i+1<limit) {
            double step=now(); check(llama_decode(c,llama_batch_get_one(&tok,1))==0,"decode failed"); double enqueued=now();
            llama_synchronize(c); double complete=now(); ++decode_steps;
            if(profile && decode_steps==16 && profiler_stop) std::fprintf(stderr,"EDGE0_CUDA_PROFILE_STOP16 status=%d\n",profiler_stop());
            if(profile) std::fprintf(stderr,"EDGE0_STEP index=%d decode_call_s=%.9f synchronize_s=%.9f total_s=%.9f\n",decode_steps,enqueued-step,complete-enqueued,complete-step);
        }
    }
    double end=now();log_io("decode-end");
    if(profile && decode_steps<16 && profiler_stop) std::fprintf(stderr,"EDGE0_CUDA_PROFILE_STOP status=%d\n",profiler_stop());
    franken_edge0_profile_phase(3);
    struct rusage u{}; getrusage(RUSAGE_SELF,&u);
    std::fprintf(stderr,"EDGE0_RESULT input_tokens=%d generated_tokens=%d load_s=%.6f prefill_s=%.6f prefill_tps=%.6f ttft_s=%.6f decode_s=%.6f decode_steps=%d decode_tps=%.6f eos=%d maxrss_KiB=%ld\n",n,generated,loaded-load_start,prefill_end-t0,n/(prefill_end-t0),first?first-t0:0,generated?end-decode_start:0,decode_steps,decode_steps>0?decode_steps/(end-decode_start):0,eos,u.ru_maxrss);
    llama_sampler_free(sampler); llama_free(c); llama_model_free(m); llama_backend_free(); return 0;
 } catch(const std::exception & e) { std::fprintf(stderr,"EDGE0_ERROR %s\n",e.what()); return 1; }
}
