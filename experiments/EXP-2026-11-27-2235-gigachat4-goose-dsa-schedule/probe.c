#include "schedule.h"
#include "reference-dispatch.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct call {
    int kernel;
    int64_t columns, rows, threads, count;
    uint64_t args[10];
} expected[128];
static int cursor, used, fail_at;
static uint64_t wanted_zero;

static int remember_zero(void *ctx, uint64_t address, size_t bytes) {
    assert(bytes == 16);
    wanted_zero=address;
    return 0;
}

static int remember_launch(void *ctx, enum dsa_kernel kernel,
        unsigned columns,unsigned rows,unsigned threads,void **args) {
    static const int counts[]={6,6,5,10,3,9,9,6,7,4};
    struct call *r=&expected[used++];
    *r=(struct call){kernel,columns,rows,threads,counts[kernel],{0}};
    for(int i=0;i<r->count;++i) {
        if ((kernel==DSA_PACK || kernel==DSA_METADATA) && i==5)
            r->args[i]=*(const uint32_t *)args[i];
        else
            memcpy(&r->args[i],args[i],sizeof(uint64_t));
    }
    return 0;
}

int32_t giga_dsa_schedule_zero(uint64_t address,uint64_t bytes) {
    assert(address==wanted_zero && bytes==16);
    return 0;
}

int32_t giga_dsa_schedule_launch(int32_t kernel,int64_t columns,int64_t rows,
        int64_t threads,sl_u64 args) {
    assert(cursor<used);
    const struct call *r=&expected[cursor++];
    assert(r->kernel==kernel && r->columns==columns && r->rows==rows &&
           r->threads==threads && r->count==args.len);
    for(int i=0;i<r->count;++i) assert(args.data[i]==r->args[i]);
    return cursor==fail_at ? -2 : 0;
}

int main(void) {
    const int64_t cells[]={4112,8192,32768};
    const int64_t rows[]={1,128,511,512,513,1024,4096};
    unsigned total=0;
    goose_init(0,NULL);
    for(int ci=0;ci<3;++ci) for(int ri=0;ri<7;++ri) {
        struct dsa_io io={.q=0x10000000,.k=0x20000000,.weights=0x30000000,.mask=0x40000000,
            .scores=0x50000000,.ids=0x60000000,.config=0x70000000,.workspace=0x80000000,
            .cells=cells[ci],.rows=rows[ri],.key_stride=262,.query_stride=32768,
            .weight_stride=256,.mask_stride=cells[ci]*2};
        io.workspace_bytes=dsa_workspace_bytes(io.cells,io.rows);
        int64_t config[]={io.cells,io.rows,io.key_stride,io.query_stride,io.weight_stride,io.mask_stride};
        uint64_t buffers[]={io.q,io.k,io.weights,io.mask,io.scores,io.ids,io.config,io.workspace};
        assert(giga_dsa_schedule_workspace(io.cells,io.rows)==io.workspace_bytes);
        cursor=used=fail_at=0;
        assert(dsa_dispatch((struct dsa_calls){0,remember_zero,remember_launch},io)==1);
        assert(giga_dsa_schedule((sl_i64){config,6},(sl_u64){buffers,8},io.workspace_bytes)==1);
        assert(cursor==used);
        total+=cursor;
        cursor=0; fail_at=(used+1)/2;
        assert(giga_dsa_schedule((sl_i64){config,6},(sl_u64){buffers,8},io.workspace_bytes)==-1);
        assert(cursor==fail_at);
        cursor=0;
        assert(giga_dsa_schedule((sl_i64){config,6},(sl_u64){buffers,8},io.workspace_bytes-1)==0);
        assert(cursor==0);
    }
    printf("%u launches exactly matched across 21 shapes\n",total);
}
