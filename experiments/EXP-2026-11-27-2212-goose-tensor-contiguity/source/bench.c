#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <time.h>
#include <string.h>
#include "ggml.h"
#include "dense.h"

enum { Count = 256, Rounds = 4000 };
static struct ggml_tensor nodes[Count], operands[Count][4];
static volatile int64_t sink;
static double now(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC_RAW, &t);
    return t.tv_sec + 1e-9*t.tv_nsec;
}
static double measure(uint8_t (*describe)(uint64_t,sl_i64)) {
    int64_t packet[71] = {0};
    double start = now();
    for (int r = 0; r < Rounds; ++r) for (int i = 0; i < Count; ++i) {
        uint8_t dense = describe((uintptr_t)&nodes[i], (sl_i64){packet,71});
        sink = packet[39] + dense;
    }
    return (now()-start)*1e9/(Rounds*Count);
}
static void setup(struct ggml_tensor *t, int i) {
    t->type = i%2 ? GGML_TYPE_F32 : GGML_TYPE_Q4_K;
    const int64_t block = ggml_blck_size(t->type);
    t->ne[0] = block * (i%13+1);
    t->ne[1] = i%7+1; t->ne[2] = i%5+1; t->ne[3] = i%3+1;
    t->nb[0] = ggml_type_size(t->type);
    t->nb[1] = t->nb[0] * (t->ne[0]/block) + i%3;
    t->nb[2] = t->nb[1] * t->ne[1] + i%5;
    t->nb[3] = t->nb[2] * t->ne[2] + i%7;
    t->data = t;
}
int main(void) {
    goose_init(0,NULL);
    for (int i=0;i<Count;++i) {
        setup(&nodes[i], i);
        nodes[i].op = i%2 ? GGML_OP_ADD : GGML_OP_TOP_K;
        for (int j=0;j<4;++j) {
            setup(&operands[i][j], i+j);
            if (j <= i%4) nodes[i].src[j] = &operands[i][j];
        }
        int64_t old[71]={0}, new[71]={0};
        if (ref_node((uintptr_t)&nodes[i], (sl_i64){old,71}) != goose_node((uintptr_t)&nodes[i], (sl_i64){new,71})) return 2;
        if (memcmp(old,new,sizeof(old))) return 1;
    }
    printf("%d packets matched\n",Count);
    for (int i=0;i<6;++i) {
        double c,g;
        if (i%2) { g=measure(goose_node); c=measure(ref_node); }
        else { c=measure(ref_node); g=measure(goose_node); }
        printf("C %.2f ns/node, Goose %.2f ns/node\n",c,g);
    }
}
