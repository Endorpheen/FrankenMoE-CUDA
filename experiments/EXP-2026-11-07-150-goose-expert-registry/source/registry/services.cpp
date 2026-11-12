static void fg150_description(std::string&text){fg150_registry_request q={};q.description=&text;fg150_call(0,q);}
extern "C" int64_t fg150_registry_meta(int64_t c,int64_t field){auto&r=*(fg150_registry_request*)(uintptr_t)c;
 if(field>=10000){GGML_ASSERT(field-10000<(int64_t)r.description->size());return(uint8_t)(*r.description)[field-10000];}
 if(field>=100){GGML_ASSERT(field-100<(int64_t)strlen(r.line));return(uint8_t)r.line[field-100];}
 switch(field){case 0:return strlen(r.line);case 1:GGML_ASSERT(r.index<=INT64_MAX);return r.index;case 2:return GGML_BACKEND_DEVICE_TYPE_CPU;case 3:GGML_ASSERT(r.size<=INT64_MAX);return r.size;case 4:return ggml_backend_buft_get_alignment(r.pointer?(ggml_backend_buffer_type_t)r.pointer:ggml_backend_cpu_buffer_type());case 5:return(int64_t)(uintptr_t)r.pointer;case 6:
#ifdef __APPLE__
 return 1;
#elif defined(_WIN32)
 return 2;
#elif defined(__linux__)
 return 0;
#else
 return 3;
#endif
 default:GGML_ABORT("invalid registry metadata");}}
extern "C" int64_t fg150_registry_action(int64_t c,int64_t op,int64_t a,int64_t b){auto&r=*(fg150_registry_request*)(uintptr_t)c;switch(op){
 case 0:return(int64_t)(uintptr_t)fopen("/proc/cpuinfo","r");
 case 1:return fgets(r.line,sizeof(r.line),(FILE*)(uintptr_t)a)!=nullptr;
 case 2:GGML_ASSERT(a>=0&&b>=a&&b<=(int64_t)strlen(r.line));r.description->assign(r.line+a,(size_t)(b-a));return 0;
 case 3:fclose((FILE*)(uintptr_t)a);return 0;
 case 4:{static ggml_backend_cpu_device_context ctx;static ggml_backend_device device={ggml_backend_cpu_device_i,r.reg,&ctx};return(int64_t)(uintptr_t)&device;}
 case 5:ggml_cpu_init();return 0;
 case 6:{static ggml_backend_reg reg={GGML_BACKEND_API_VERSION,ggml_backend_cpu_reg_i,nullptr};return(int64_t)(uintptr_t)&reg;}
 case 7:switch(a){case 0:r.props->name=ggml_backend_cpu_device_get_name(r.dev);break;case 1:r.props->description=ggml_backend_cpu_device_get_description(r.dev);break;case 2:r.props->type=ggml_backend_cpu_device_get_type(r.dev);break;case 3:ggml_backend_cpu_device_get_memory(r.dev,&r.props->memory_free,&r.props->memory_total);break;default:GGML_ABORT("invalid CPU property");}return 0;
 case 8:GGML_ASSERT(b==0||b==1);switch(a){case 0:r.props->caps.async=b;break;case 1:r.props->caps.host_buffer=b;break;case 2:r.props->caps.buffer_from_host_ptr=b;break;case 3:r.props->caps.events=b;break;case 4:r.props->caps.mmap_support=b;break;default:GGML_ABORT("invalid CPU capability");}return 0;
 case 9:return(int64_t)(uintptr_t)"CPU";
 case 10:return(int64_t)(uintptr_t)ggml_backend_cpu_init();
 case 11:return(int64_t)(uintptr_t)ggml_backend_cpu_buffer_type();
 case 12:return(int64_t)(uintptr_t)ggml_backend_cpu_buffer_from_ptr(r.pointer,r.size);
 case 13:return(int64_t)(uintptr_t)((ggml_backend_cpu_device_context*)r.dev->context)->description.c_str();

#ifdef GGML_USE_CPU_HBM
 case 14:{void*pointer=nullptr;int status=hbw_posix_memalign(&pointer,(size_t)a,(size_t)b);if(status!=0){GGML_LOG_ERROR("failed to allocate HBM buffer of size %zu\n",(size_t)b);return 0;}return(int64_t)(uintptr_t)pointer;}
 case 15:return(int64_t)(uintptr_t)ggml_backend_cpu_buffer_from_ptr((void*)(uintptr_t)a,(size_t)b);
 case 16:hbw_free((void*)(uintptr_t)a);return 0;
 case 17:{auto*buffer=(ggml_backend_buffer_t)(uintptr_t)a;buffer->buft=(ggml_backend_buffer_type_t)r.pointer;buffer->iface.free_buffer=fg150_hbm_free;return 0;}
#endif

 case 19:GGML_ASSERT(a>=0);r.description->resize((size_t)a);return 0;
#ifdef __APPLE__
 case 18:{size_t size=0;return sysctlbyname("machdep.cpu.brand_string",nullptr,&size,nullptr,0)==0?(int64_t)size:0;}
 case 20:{size_t size=r.description->size();return sysctlbyname("machdep.cpu.brand_string",r.description->data(),&size,nullptr,0)==0;}
#endif
#ifdef _WIN32
 case 21:{HKEY key=nullptr;if(RegOpenKeyEx(HKEY_LOCAL_MACHINE,TEXT("HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0"),0,KEY_READ,&key)!=ERROR_SUCCESS)return 0;return(int64_t)(uintptr_t)key;}
 case 22:{DWORD size=0;return RegQueryValueExA((HKEY)(uintptr_t)a,"ProcessorNameString",nullptr,nullptr,nullptr,&size)==ERROR_SUCCESS?size:0;}
 case 23:{DWORD size=(DWORD)r.description->size();return RegQueryValueExA((HKEY)(uintptr_t)a,"ProcessorNameString",nullptr,nullptr,(LPBYTE)r.description->data(),&size)==ERROR_SUCCESS;}
 case 24:RegCloseKey((HKEY)(uintptr_t)a);return 0;
#endif
 default:GGML_ABORT("invalid registry primitive");}}

#ifdef GGML_USE_CPU_HBM
static void fg150_hbm_free(ggml_backend_buffer_t buffer){fg150_registry_request q={};q.pointer=buffer->context;fg150_call(11,q);}
extern "C" ggml_backend_buffer_t fg150_hbm_alloc(ggml_backend_buffer_type_t buft,size_t size){fg150_registry_request q={};q.pointer=buft;q.size=size;return(ggml_backend_buffer_t)(uintptr_t)fg150_call(10,q);}
#endif
