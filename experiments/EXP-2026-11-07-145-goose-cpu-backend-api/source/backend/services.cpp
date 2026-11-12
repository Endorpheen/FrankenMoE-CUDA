
static fg145_request fg145_request_backend(ggml_backend_t backend){fg145_request q={};q.backend=backend;q.cpu=(ggml_backend_cpu_context*)backend->context;return q;}
static ggml_cplan&fg145_plan(fg145_request&r){return r.plan?r.plan->cplan:r.temporary;}
static std::vector<ggml_backend_buffer_type_t>&fg145_extras(){static std::vector<ggml_backend_buffer_type_t> value=[](){std::vector<ggml_backend_buffer_type_t> out;fg145_request q={};q.extras=&out;fg145_call(13,q);return out;}();return value;}
static ggml_backend_buffer_type_t*fg145_extra_copy(){static std::vector<ggml_backend_buffer_type_t> value=[](){std::vector<ggml_backend_buffer_type_t> out;fg145_request q={};q.extras=&out;fg145_call(14,q);return out;}();return value.data();}
static ggml_backend_feature*fg145_features(){static std::vector<ggml_backend_feature> value=[](){ggml_cpu_init();std::vector<ggml_backend_feature> out;fg145_request q={};q.features=&out;fg145_call(18,q);return out;}();return value.data();}
extern "C" int64_t fg145_meta(int64_t context,int64_t field){auto&r=*(fg145_request*)(uintptr_t)context;const ggml_tensor*t=r.op;const ggml_tensor*x=t?t->src[0]:nullptr;const ggml_tensor*y=t?t->src[1]:nullptr;
 if(field>=100&&field<104){auto*source=t->src[field-100];return source&&source->buffer?(int64_t)(uintptr_t)source->buffer->buft:0;}
 if(field>=200&&field<300){auto&v=fg145_extras();size_t i=(size_t)(field-200);GGML_ASSERT(i<v.size());return(int64_t)(uintptr_t)v[i];}
 if(field>=300&&field<400){GGML_ASSERT(field-300<r.name_len);return(uint8_t)r.name[field-300];}
 if(field>=400){switch(field-400){case 0:return ggml_cpu_has_sse3();
case 1:return ggml_cpu_has_ssse3();
case 2:return ggml_cpu_has_avx();
case 3:return ggml_cpu_has_avx_vnni();
case 4:return ggml_cpu_has_avx2();
case 5:return ggml_cpu_has_f16c();
case 6:return ggml_cpu_has_fma();
case 7:return ggml_cpu_has_bmi2();
case 8:return ggml_cpu_has_avx512();
case 9:return ggml_cpu_has_avx512_vbmi();
case 10:return ggml_cpu_has_avx512_vnni();
case 11:return ggml_cpu_has_avx512_bf16();
case 12:return ggml_cpu_has_amx_int8();
case 13:return ggml_cpu_has_neon();
case 14:return ggml_cpu_has_arm_fma();
case 15:return ggml_cpu_has_fp16_va();
case 16:return ggml_cpu_has_matmul_int8();
case 17:return ggml_cpu_has_sve();
case 18:return ggml_cpu_has_dotprod();
case 19:return ggml_cpu_get_sve_cnt();
case 20:return ggml_cpu_has_sme();
case 21:return ggml_cpu_has_sme2();
case 22:return ggml_cpu_has_riscv_v();
case 23:return ggml_cpu_get_rvv_vlen();
case 24:return ggml_cpu_has_vsx();
case 25:return ggml_cpu_has_vxe();
case 26:return ggml_cpu_has_wasm_simd();
case 27:return ggml_cpu_has_llamafile();
case 28:
#ifdef GGML_USE_ACCELERATE
return 1;
#else
return 0;
#endif

case 29:
#ifdef GGML_USE_CPU_HBM
return 1;
#else
return 0;
#endif

case 30:
#ifdef GGML_USE_OPENMP
return 1;
#else
return 0;
#endif

case 31:
#ifdef GGML_USE_CPU_KLEIDIAI
return 1;
#else
return 0;
#endif

case 32:
#ifdef GGML_USE_CPU_REPACK
return 1;
#else
return 0;
#endif
 default:GGML_ABORT("invalid CPU feature");}}
 switch(field){case 0:return ggml_backend_is_cpu(r.backend);case 1:return GGML_DEFAULT_N_THREADS;case 2:return r.cpu->n_threads;case 3:return(int64_t)(uintptr_t)r.cpu->threadpool;case 4:return(int64_t)(uintptr_t)r.cpu->work_data;case 5:GGML_ASSERT(r.cpu->work_size<=INT64_MAX);return(int64_t)r.cpu->work_size;case 6:return(int64_t)(uintptr_t)r.cpu->abort_callback;case 7:return(int64_t)(uintptr_t)r.cpu->abort_callback_data;case 8:return r.cpu->use_ref;case 9:return r.value;case 10:return(int64_t)(uintptr_t)r.pointer;case 11:return(int64_t)(uintptr_t)r.data;case 12:return(int64_t)(uintptr_t)r.plan;case 13:GGML_ASSERT(fg145_plan(r).work_size<=INT64_MAX);return(int64_t)fg145_plan(r).work_size;case 14:return(int64_t)(uintptr_t)fg145_plan(r).work_data;
 case 20:return t->op;case 21:return t->type;case 22:return x?x->type:-1;case 23:return y?y->type:-1;case 24:return ggml_get_type_traits_cpu(x->type)->vec_dot_type;case 25:return x?ggml_is_quantized(x->type):0;case 26:return x?ggml_is_contiguous(x):0;case 27:return ggml_get_op_params_i32(t,0);case 28:return ggml_get_op_params_i32(t,1);case 29:return GGML_HINT_SRC0_IS_HADAMARD;case 30:return t->src[3]?t->src[3]->ne[0]:0;case 31:return x?x->ne[2]:0;case 32:return y?y->ne[2]:0;case 33:return x?x->ne[3]:0;case 34:return y?y->ne[3]:0;case 40:return(int64_t)fg145_extras().size();case 41:return ggml_backend_buft_is_host(r.buft);case 42:return(int64_t)(uintptr_t)r.buft;case 43:return r.name_len;
 case 44:
#ifdef _WIN32
 return 1;
#else
 return 0;
#endif
 case 45:
#ifdef _WIN32
 {MEMORYSTATUSEX status;status.dwLength=sizeof(status);GlobalMemoryStatusEx(&status);return(int64_t)status.ullTotalPhys;}
#else
 return sysconf(_SC_PHYS_PAGES);
#endif
 case 46:
#ifdef _WIN32
 {MEMORYSTATUSEX status;status.dwLength=sizeof(status);GlobalMemoryStatusEx(&status);return(int64_t)status.ullAvailPhys;}
#else
 return sysconf(_SC_PAGE_SIZE);
#endif
 case 47:return GGML_STATUS_ALLOC_FAILED;case 51:return 33;default:GGML_ABORT("invalid CPU backend metadata");}
}
extern "C" float fg145_float(int64_t context,int64_t field){auto&r=*(fg145_request*)(uintptr_t)context;GGML_ASSERT(field==0);float result;std::memcpy(&result,(const char*)r.op->op_params+sizeof(float),sizeof(result));return result;}
extern "C" int64_t fg145_action(int64_t context,int64_t op,int64_t a,int64_t b){auto&r=*(fg145_request*)(uintptr_t)context;switch(op){
 case 0:r.cpu=new(std::nothrow)ggml_backend_cpu_context;return(int64_t)(uintptr_t)r.cpu;
 case 1:switch(a){case 0:GGML_ASSERT(b>=INT32_MIN&&b<=INT32_MAX);r.cpu->n_threads=(int)b;break;case 1:r.cpu->threadpool=(ggml_threadpool_t)(uintptr_t)b;break;case 2:r.cpu->work_data=(uint8_t*)(uintptr_t)b;break;case 3:GGML_ASSERT(b>=0);r.cpu->work_size=(size_t)b;break;case 4:r.cpu->abort_callback=(ggml_abort_callback)(uintptr_t)b;break;case 5:r.cpu->abort_callback_data=(void*)(uintptr_t)b;break;case 6:r.cpu->use_ref=b!=0;break;default:GGML_ABORT("invalid CPU context write");}return 0;
 case 2:{auto*backend=new(std::nothrow)ggml_backend{ggml_backend_cpu_guid(),ggml_backend_cpu_i,ggml_backend_reg_dev_get(ggml_backend_cpu_reg(),0),r.cpu};return(int64_t)(uintptr_t)backend;}
 case 3:delete r.cpu;r.cpu=nullptr;return 0;case 4:delete r.backend;r.backend=nullptr;return 0;
 case 5:GGML_ASSERT(a>=0);return(int64_t)(uintptr_t)new(std::nothrow)uint8_t[(size_t)a];
 case 6:delete[](uint8_t*)(uintptr_t)a;return 0;
 case 7:ggml_threadpool_pause((ggml_threadpool_t)(uintptr_t)a);return 0;
 case 8:r.plan=new(std::nothrow)ggml_backend_plan_cpu;return(int64_t)(uintptr_t)r.plan;
 case 9:delete r.plan;r.plan=nullptr;return 0;
 case 10:fg145_plan(r)=ggml_graph_plan(r.graph,r.cpu->n_threads,r.cpu->threadpool);return 0;
 case 11:r.plan->cgraph=*r.graph;return 0;
 case 12:switch(a){case 0:fg145_plan(r).work_data=(uint8_t*)(uintptr_t)b;break;case 1:fg145_plan(r).abort_callback=(ggml_abort_callback)(uintptr_t)b;break;case 2:fg145_plan(r).abort_callback_data=(void*)(uintptr_t)b;break;case 3:fg145_plan(r).use_ref=b!=0;break;default:GGML_ABORT("invalid CPU plan write");}return 0;
 case 13:GGML_ASSERT(a==0||a==1);return a?ggml_graph_compute(&r.plan->cgraph,&r.plan->cplan):ggml_graph_compute(r.graph,&r.temporary);
 case 14:{GGML_ASSERT(a>=0&&a<4);auto*extra=(ggml::cpu::extra_buffer_type*)r.op->src[a]->buffer->buft->context;return extra->supports_op(r.dev,r.op);}
 case 15:r.extras->push_back((ggml_backend_buffer_type_t)(uintptr_t)a);return 0;
 case 16:switch(a){case 0:
#if defined(__AMX_INT8__) && defined(__AVX512VNNI__)
 return(int64_t)(uintptr_t)ggml_backend_amx_buffer_type();
#else
 return 0;
#endif
 case 1:
#ifdef GGML_USE_CPU_RISCV64_SPACEMIT
 return(int64_t)(uintptr_t)ggml_backend_cpu_riscv64_spacemit_buffer_type();
#else
 return 0;
#endif
 case 2:
#ifdef GGML_USE_CPU_KLEIDIAI
 return(int64_t)(uintptr_t)ggml_backend_cpu_kleidiai_buffer_type();
#else
 return 0;
#endif
 case 3:
#ifdef GGML_USE_CPU_REPACK
 return(int64_t)(uintptr_t)ggml_backend_cpu_repack_buffer_type();
#else
 return 0;
#endif
 default:GGML_ABORT("invalid extra buffer provider");}
 case 17:{static void*const addresses[]={(void*)ggml_backend_cpu_set_n_threads,(void*)ggml_backend_cpu_device_get_extra_buffers_type,(void*)ggml_backend_cpu_get_features,(void*)ggml_backend_cpu_set_abort_callback,(void*)ggml_numa_init,(void*)ggml_is_numa,(void*)ggml_backend_cpu_set_use_ref,(void*)ggml_threadpool_new,(void*)ggml_threadpool_free,(void*)ggml_backend_cpu_set_threadpool};GGML_ASSERT(a>=0&&(size_t)a<sizeof(addresses)/sizeof(*addresses));return(int64_t)(uintptr_t)addresses[a];}
 case 18:GGML_ASSERT(b>=0);if(a==0)*r.free_mem=(size_t)b;else{GGML_ASSERT(a==1);*r.total_mem=(size_t)b;}return 0;
 case 19:{if(a==-1){r.features->push_back({nullptr,nullptr});return 0;}static const char*const names[]={"SSE3","SSSE3","AVX","AVX_VNNI","AVX2","F16C","FMA","BMI2","AVX512","AVX512_VBMI","AVX512_VNNI","AVX512_BF16","AMX_INT8","NEON","ARM_FMA","FP16_VA","MATMUL_INT8","SVE","DOTPROD","SVE_CNT","SME","SME2","RISCV_V","RVV_VLEN","VSX","VXE","WASM_SIMD","LLAMAFILE","ACCELERATE","CPU_HBM","OPENMP","KLEIDIAI","REPACK"};GGML_ASSERT(a>=0&&(size_t)a<sizeof(names)/sizeof(*names));const char*value="1";if(a==19){static std::string sve_count=std::to_string(ggml_cpu_get_sve_cnt());value=sve_count.c_str();}if(a==23){static std::string rvv_count=std::to_string(ggml_cpu_get_rvv_vlen());value=rvv_count.c_str();} r.features->push_back({names[a],value});return 0;}
 default:GGML_ABORT("invalid CPU backend primitive");}}
