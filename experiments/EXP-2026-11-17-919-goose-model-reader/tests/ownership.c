#include "runtime.h"
#include "ggml.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

_Static_assert(sizeof(void*)==8 && sizeof(struct ggml_init_params)==24,"pinned x86_64 init ABI");
_Static_assert(offsetof(struct ggml_init_params,mem_size)==0 && offsetof(struct ggml_init_params,mem_buffer)==8 && offsetof(struct ggml_init_params,no_alloc)==16,"pinned init offsets");
static void check(int ok,const char * text){if(!ok){fprintf(stderr,"FAIL %s\n",text);exit(3);}}
int main(void){
    goose_init(0,NULL);
    check(qo_context_new(0,0)==0 && qo_context_new(1024,2)==0,"invalid context input");
    for(int policy=0;policy<2;++policy){
        struct ggml_context * c=(void*)(intptr_t)qo_context_new(1024*1024,policy);
        check(c!=NULL && ggml_get_no_alloc(c)==(policy!=0),"native context ABI fields");
        struct ggml_tensor * t=ggml_new_tensor_1d(c,GGML_TYPE_F32,128);
        if(!policy){for(int i=0;i<128;++i)((float*)t->data)[i]=3.25f;
            check(qo_tensor_zero((int64_t)(intptr_t)t)==(int64_t)(intptr_t)t,"zero result");
            for(int i=0;i<128;++i)check(((float*)t->data)[i]==0.0f,"tensor clear");
        }else check(t->data==NULL,"metadata-only allocation");
        qo_context_free((int64_t)(intptr_t)c);
    }
    qo_context_free(0);check(qo_tensor_zero(0)==0,"null zero guard");
    puts("PASS native context ownership ABI; no new adapter; no model");
}
