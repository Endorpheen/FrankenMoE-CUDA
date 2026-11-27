#include "probe.h"
#include "ggml.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int reset_calls, reset_status;
int64_t fg_cuda_reset_model_state(void) { ++reset_calls; return reset_status; }

int main(void) {
    goose_init(0, NULL);
    assert(!giga_request_bytes(0) && !giga_request_bytes(1048577));
    const int64_t capacities[] = {1, 31, 32768, 1048576};
    for (size_t i = 0; i < sizeof capacities / sizeof capacities[0]; ++i) {
        uint64_t expected = 66 * ggml_tensor_overhead() +
            24u * (3u * 8192u + 128u * 128u * 32u) * sizeof(float) +
            (uint64_t)capacities[i] * 9u * (576u + 128u) * sizeof(uint16_t);
        assert(giga_request_bytes(capacities[i]) == expected);
    }
    const int64_t capacity = 33;
    struct ggml_context *ctx = ggml_init((struct ggml_init_params){giga_request_bytes(capacity),NULL,false});
    assert(ctx);
    int64_t kinds[32] = {0};
    uint64_t state[128] = {0}, draft[3] = {0};
    for (int i=3; i<32; i+=4) kinds[i]=1;
    kinds[31]=2;
    assert(giga_request_allocate_state((uintptr_t)ctx,capacity,(sl_i64){kinds,32},
        (sl_u64){state,128},(sl_u64){draft,3})==-1);
    assert(!ggml_get_first_tensor(ctx));
    kinds[31]=1;
    assert(giga_request_allocate_state((uintptr_t)ctx,capacity,(sl_i64){kinds,32},
        (sl_u64){state,128},(sl_u64){draft,3})==0);
    struct ggml_tensor *cursor=ggml_get_first_tensor(ctx);
    for (int layer=0; layer<=32; ++layer) {
        const int mla=layer==32 || kinds[layer]==1;
        const uint64_t *t=layer==32?draft:state+layer*4;
        assert((uintptr_t)cursor==t[0] && cursor->type==(mla?GGML_TYPE_F16:GGML_TYPE_F32));
        assert(cursor->ne[0]==(mla?576:3) && cursor->ne[1]==(mla?capacity:8192));
        cursor=ggml_get_next_tensor(ctx,cursor);
        assert((uintptr_t)cursor==t[1] && cursor->type==(mla?GGML_TYPE_F16:GGML_TYPE_F32));
        assert(cursor->ne[0]==128 && cursor->ne[1]==(mla?capacity:128) && cursor->ne[2]==(mla?1:32));
        cursor=ggml_get_next_tensor(ctx,cursor);
        if (!mla) assert(t[2]==t[0] && t[3]==t[1]);
    }
    assert(!cursor && !draft[2]);
    for (cursor=ggml_get_first_tensor(ctx); cursor; cursor=ggml_get_next_tensor(ctx,cursor))
        memset(cursor->data, 0x7b, ggml_nbytes(cursor));
    assert(giga_request_clear_state((uintptr_t)ctx,0)==0 && reset_calls==0);
    for (cursor=ggml_get_first_tensor(ctx); cursor; cursor=ggml_get_next_tensor(ctx,cursor)) {
        const unsigned char *data=cursor->data;
        for (size_t i=0; i<ggml_nbytes(cursor); ++i) assert(data[i]==0);
    }
    assert(giga_request_clear_state((uintptr_t)ctx,1)==0 && reset_calls==1);
    reset_status=-1;
    assert(giga_request_clear_state((uintptr_t)ctx,1)==-1 && reset_calls==2);
    ggml_free(ctx);
    puts("66 tensor descriptors and full state reset verified");
}
