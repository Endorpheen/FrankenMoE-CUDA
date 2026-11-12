extern "C" __attribute__((visibility("default"))) ggml_backend_reg_t fg165_probe_best(const char*n,bool silent,const char*p){
#ifdef FG165_CANDIDATE
 if(fg165_enabled())return(ggml_backend_reg_t)(uintptr_t)fg165_call(20,FG165_A(n),FG165_A(silent),FG165_A(p));
#endif
 return ggml_backend_load_best(n,silent,p);}

#ifdef FG165_CANDIDATE
extern "C" __attribute__((visibility("default"))) void fg165_probe_finish(){fg165_call(30,(uintptr_t)&fg165_owner_get().value);}
#endif
