#include "ggml-backend-impl.h"
#include <cstdio>
#include <cstdlib>
#ifndef PLUGIN_SCORE
#define PLUGIN_SCORE 1
#endif
#ifndef PLUGIN_NAME
#define PLUGIN_NAME "plugin"
#endif
#ifndef PLUGIN_API
#define PLUGIN_API GGML_BACKEND_API_VERSION
#endif
static void event(const char*s){const char*p=getenv("FG165_PLUGIN_EVENTS");if(p){FILE*f=fopen(p,"a");if(f){fprintf(f,"%s:%s\n",PLUGIN_NAME,s);fclose(f);}}}
__attribute__((constructor))static void loaded(){event("load");}
__attribute__((destructor))static void unloaded(){event("close");}
static ggml_backend_reg reg{};static ggml_backend_device device{};
#ifndef PLUGIN_NO_SCORE
extern "C" __attribute__((visibility("default"))) int ggml_backend_score(){return PLUGIN_SCORE;}
#endif
#ifndef PLUGIN_NO_INIT
extern "C" __attribute__((visibility("default"))) ggml_backend_reg_t ggml_backend_init(){event("init");
#ifdef PLUGIN_NULL
return nullptr;
#else
reg.api_version=PLUGIN_API;reg.iface.get_name=[](ggml_backend_reg_t){return PLUGIN_NAME;};reg.iface.get_device_count=[](ggml_backend_reg_t){return size_t(1);};reg.iface.get_device=[](ggml_backend_reg_t,size_t i){return i==0?&device:nullptr;};device.reg=&reg;device.iface.get_name=[](ggml_backend_dev_t){return PLUGIN_NAME;};device.iface.get_description=[](ggml_backend_dev_t){return PLUGIN_NAME;};device.iface.get_type=[](ggml_backend_dev_t){return GGML_BACKEND_DEVICE_TYPE_CPU;};return &reg;
#endif
}
#endif
