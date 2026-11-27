#include "probe.h"
#include "ggml.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    goose_init(0, NULL);
    unsigned cases=0;
    for (int capacity=1; capacity<=65; ++capacity) {
        for (int base=-1; base<=capacity+1; ++base) {
            for (int count=0; count<=4; ++count) {
                for (int emit=-1; emit<=count+1; ++emit) {
                    struct ggml_context *ctx = ggml_init((struct ggml_init_params){4096,NULL,true});
                    uint64_t tensors[5] = {0};
                    int64_t cells = giga_prepare_step_inputs((uintptr_t)ctx, capacity, base, count, emit, (sl_u64){tensors,5});
                    if (base < 0 || count < 1 || count > capacity-base || emit < 0 || emit > count) {
                        assert(cells==-1 && !ggml_get_first_tensor(ctx));
                    } else {
                        int live=base+count, padded=(live+31)&~31;
                        assert(cells==(padded<=capacity?padded:live));
                        const int dims[]={count,count,count,(int)cells,emit};
                        const enum ggml_type types[]={GGML_TYPE_I32,GGML_TYPE_I32,GGML_TYPE_I64,GGML_TYPE_F16,GGML_TYPE_I32};
                        struct ggml_tensor *next = ggml_get_first_tensor(ctx);
                        for (int i=0;i<5;++i) {
                            struct ggml_tensor *t=(void *)(uintptr_t)tensors[i];
                            if (i==4 && (emit==0 || emit==count)) { assert(!t && !next); break; }
                            assert(t && t==next && t->type==types[i] && t->ne[0]==dims[i]);
                            assert(t->ne[1]==(i==3?count:1) && t->flags==GGML_TENSOR_FLAG_INPUT);
                            next=ggml_get_next_tensor(ctx,t);
                        }
                        assert(!next);
                    }
                    ggml_free(ctx);
                    ++cases;
                }
            }
        }
    }
    printf("%u cases passed\n",cases);
}
