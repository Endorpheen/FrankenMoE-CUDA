#include "bridge.h"
static bool fg153_first=true;
int fg153_enabled(void){const char *v=getenv("GGML_GOOSE_TENSOR_CORE");return !v||strcmp(v,"0")!=0;}
int64_t fg153_get(int64_t p,int64_t k,int64_t i){
 struct ggml_context *c=(void*)(intptr_t)p;struct ggml_object *o=(void*)(intptr_t)p;struct ggml_tensor *t=(void*)(intptr_t)p;
 switch(k){case 1:return c->mem_size;case 2:return (intptr_t)c->mem_buffer;case 3:return c->mem_buffer_owned;case 4:return c->no_alloc;case 5:return c->n_objects;case 6:return (intptr_t)c->objects_begin;case 7:return (intptr_t)c->objects_end;
 case 10:return o->offs;case 11:return o->size;case 12:return (intptr_t)o->next;case 13:return o->type;
 case 20:return t->type;case 23:return (intptr_t)t->view_src;case 24:return t->view_offs;case 25:return (intptr_t)t->data;
 case 26:GGML_ASSERT(i>=0&&i<4);return t->ne[i];case 27:GGML_ASSERT(i>=0&&i<4);return t->nb[i];
 case 30:GGML_ASSERT(i>=0&&i<GGML_TYPE_COUNT);return type_traits[i].type_size;case 31:GGML_ASSERT(i>=0&&i<GGML_TYPE_COUNT);return type_traits[i].blck_size;case 32:GGML_ASSERT(i>=0&&i<4);return ((int64_t*)(intptr_t)p)[i];
 case 40:return GGML_OBJECT_SIZE;case 41:return GGML_TENSOR_SIZE;case 42:return GGML_MEM_ALIGN;case 43:return GGML_TYPE_COUNT;case 44:return GGML_OBJECT_TYPE_TENSOR;case 47:return fg153_first;case 48:return sizeof(struct ggml_context);default:GGML_ABORT("bad fg153 read field");}}
void fg153_put(int64_t p,int64_t k,int64_t i,int64_t v){struct ggml_context *c=(void*)(intptr_t)p;struct ggml_object *o=(void*)(intptr_t)p;struct ggml_tensor*t=(void*)(intptr_t)p;
 switch(k){case 1:c->mem_size=v;return;case 2:c->mem_buffer=(void*)(intptr_t)v;return;case 3:c->mem_buffer_owned=v;return;case 4:c->no_alloc=v;return;case 5:GGML_ASSERT(v>=0&&v<=INT_MAX);c->n_objects=v;return;case 6:c->objects_begin=(void*)(intptr_t)v;return;case 7:c->objects_end=(void*)(intptr_t)v;return;
 case 10:o->offs=v;return;case 11:o->size=v;return;case 12:o->next=(void*)(intptr_t)v;return;case 13:o->type=v;return;
 case 20:t->type=v;return;case 23:t->view_src=(void*)(intptr_t)v;return;case 24:t->view_offs=v;return;case 25:t->data=(void*)(intptr_t)v;return;case 26:GGML_ASSERT(i>=0&&i<4);t->ne[i]=v;return;case 27:GGML_ASSERT(i>=0&&i<4);t->nb[i]=v;return;case 47:fg153_first=v;return;default:GGML_ABORT("bad fg153 write field");}}
int64_t fg153_service(int64_t op,int64_t a,int64_t b){switch(op){
 case 0:ggml_critical_section_start();return 0;case 1:ggml_critical_section_end();return 0;case 2:ggml_time_init();return 0;
 case 3:return (intptr_t)GGML_MALLOC((size_t)a);case 4:return (intptr_t)ggml_aligned_malloc((size_t)a);case 5:GGML_FREE((void*)(intptr_t)a);return 0;case 6:ggml_aligned_free((void*)(intptr_t)a,(size_t)b);return 0;
 case 7:memset((void*)(intptr_t)a,0,(size_t)b);return 0;case 8:GGML_LOG_WARN("fg153 context arena allocation failed\n");return 0;case 10:GGML_ABORT("Goose tensor invariant failed");case 9:return strcmp(((struct ggml_tensor*)(intptr_t)a)->name,(char*)(intptr_t)b)==0;default:GGML_ABORT("bad fg153 primitive");}}
