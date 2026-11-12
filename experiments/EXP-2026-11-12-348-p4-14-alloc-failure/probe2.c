#include "ggml.h"
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
static void once(struct ggml_context * c, int n, size_t * used){
    for (int i=0;i<n;i++) ggml_new_tensor_1d(c, GGML_TYPE_F32, 256);
    *used = ggml_used_mem(c);
}
struct arg { int id; size_t used; int ok; };
static void * worker(void * p){
    struct arg * a = p;
    struct ggml_init_params ip = { 2u*1024u*1024u, NULL, false };
    struct ggml_context * c = ggml_init(ip);
    if (!c){ a->ok=0; return NULL; }
    size_t u1; once(c, 20, &u1);
    ggml_reset(c);
    size_t u2; once(c, 20, &u2);
    for (int r=0;r<3;r++){ ggml_reset(c); size_t u3; once(c, 20, &u3); if (u3!=u2) a->ok=0; }
    a->used = u2;
    a->ok = 1;
    ggml_free(c);
    return NULL;
}
int main(void){
    setvbuf(stdout,NULL,_IONBF,0);
    struct arg a[2] = {{0,0,0},{1,0,0}};
    pthread_t t[2];
    for (int i=0;i<2;i++) pthread_create(&t[i], NULL, worker, &a[i]);
    for (int i=0;i<2;i++) pthread_join(t[i], NULL);
    printf("reuse used=%zu %zu ok=%d %d\n", a[0].used, a[1].used, a[0].ok, a[1].ok);
    return (a[0].ok && a[1].ok && a[0].used==a[1].used) ? 0 : 1;
}
