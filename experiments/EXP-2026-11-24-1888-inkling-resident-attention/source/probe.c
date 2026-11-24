#include "main.h"
#ifdef I2_NUMERIC_MODULE
void i2_candidate_init(int argc, char **argv);
int64_t inkling_row_major_i2_situ(sl_f32 gs_export_arg0, sl_f32 gs_export_arg1, float gs_export_arg2, float gs_export_arg3, sl_f32 gs_export_arg4);
#define i2_situ inkling_row_major_i2_situ
int64_t inkling_row_major_i2_relative_positions(int64_t gs_export_arg0, sl_i64 gs_export_arg1, int64_t gs_export_arg2, sl_i32 gs_export_arg3);
#define i2_relative_positions inkling_row_major_i2_relative_positions
int64_t inkling_row_major_i2_shortconv_decode(sl_f32 gs_export_arg0, sl_f32 gs_export_arg1, sl_f32 gs_export_arg2, sl_f32 gs_export_arg3, sl_f32 gs_export_arg4);
#define i2_shortconv_decode inkling_row_major_i2_shortconv_decode
int64_t inkling_row_major_i2_attention_decode(sl_f32 gs_export_arg0, sl_f32 gs_export_arg1, sl_f32 gs_export_arg2, sl_f32 gs_export_arg3, sl_f32 gs_export_arg4, sl_f32 gs_export_arg5, sl_i64 gs_export_arg6, int64_t gs_export_arg7, int64_t gs_export_arg8, sl_f32 gs_export_arg9, sl_f32 gs_export_arg10);
#define i2_attention_decode inkling_row_major_i2_attention_decode
int64_t inkling_row_major_i2_cache_layout(sl_i64 gs_export_arg0, int64_t gs_export_arg1, int64_t gs_export_arg2, int64_t gs_export_arg3, int64_t gs_export_arg4, int64_t gs_export_arg5, int64_t gs_export_arg6, sl_i64 gs_export_arg7);
#define i2_cache_layout inkling_row_major_i2_cache_layout
int64_t inkling_row_major_i2_cache_row(int64_t gs_export_arg0, int64_t gs_export_arg1, int64_t gs_export_arg2, uint8_t gs_export_arg3);
#define i2_cache_row inkling_row_major_i2_cache_row
void inkling_row_major_i2_reset_cache_positions(sl_i64 gs_export_arg0);
#define i2_reset_cache_positions inkling_row_major_i2_reset_cache_positions
int64_t inkling_row_major_i2_store_kv(int64_t gs_export_arg0, int64_t gs_export_arg1, uint8_t gs_export_arg2, sl_i64 gs_export_arg3, sl_f32 gs_export_arg4, sl_f32 gs_export_arg5, sl_f32 gs_export_arg6, sl_f32 gs_export_arg7);
#define i2_store_kv inkling_row_major_i2_store_kv
int64_t inkling_row_major_i2_depth_mix(sl_f32 gs_export_arg0, int64_t gs_export_arg1, sl_f32 gs_export_arg2, sl_f32 gs_export_arg3, sl_f32 gs_export_arg4, sl_f32 gs_export_arg5);
int64_t inkling_row_major_i2_loop_blend(sl_f32 gs_export_arg0, sl_f32 gs_export_arg1, sl_f32 gs_export_arg2, sl_f32 gs_export_arg3, sl_f32 gs_export_arg4);
int64_t inkling_row_major_i2_attention_token(sl_f32 gs_export_arg0, sl_f32 gs_export_arg1, sl_f32 gs_export_arg2, sl_f32 gs_export_arg3, sl_f32 gs_export_arg4, sl_f32 gs_export_arg5, int64_t gs_export_arg6, int64_t gs_export_arg7, uint8_t gs_export_arg8, sl_i64 gs_export_arg9, sl_f32 gs_export_arg10, sl_f32 gs_export_arg11, sl_f32 gs_export_arg12, sl_f32 gs_export_arg13, sl_f32 gs_export_arg14, sl_f32 gs_export_arg15);
int64_t inkling_row_major_i2_rms_gain(sl_f32 gs_export_arg0, sl_f32 gs_export_arg1, float gs_export_arg2, sl_f32 gs_export_arg3);
int64_t inkling_row_major_i2_postconv_token(sl_f32 gs_export_arg0, sl_f32 gs_export_arg1, sl_f32 gs_export_arg2, sl_f32 gs_export_arg3);
int64_t inkling_row_major_i2_text_logits(sl_f32 gs_export_arg0, int64_t gs_export_arg1, float gs_export_arg2);
#define I2_CALL(name) inkling_row_major_ ## name
#else
#define I2_CALL(name) name
#endif

#include "graph-execute.h"
#include "ggml.h"

#include <assert.h>
#include <fcntl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

static void check_state(const char *name, int token, const float *values, size_t length) {
    if (!getenv("I2_CHECK_STATE")) return;
    const unsigned char *bytes = (const unsigned char *)values;
    uint64_t value = UINT64_C(14695981039346656037);
    for (size_t i = 0; i < length * sizeof(*values); ++i) {
        value = (value ^ bytes[i]) * UINT64_C(1099511628211);
    }
    fprintf(stderr, "I2 state %s token %d %016llx\n", name, token, (unsigned long long)value);
}

static double now(void);
static double call_seconds[9];
static unsigned call_count[9];

#define MEASURE(index, expression) ({ \
    const double start = now(); \
    __auto_type result = (expression); \
    call_seconds[index] += now() - start; \
    ++call_count[index]; \
    result; \
})
#define i2_attention_token(...) MEASURE(0, I2_CALL(i2_attention_token)(__VA_ARGS__))
#define i2_depth_mix(...) MEASURE(1, I2_CALL(i2_depth_mix)(__VA_ARGS__))
#define i2_rms_gain(...) MEASURE(2, I2_CALL(i2_rms_gain)(__VA_ARGS__))
#define i2_loop_blend(...) MEASURE(3, I2_CALL(i2_loop_blend)(__VA_ARGS__))
#define i2_postconv_token(...) MEASURE(4, I2_CALL(i2_postconv_token)(__VA_ARGS__))
#define i2_text_logits(...) MEASURE(5, I2_CALL(i2_text_logits)(__VA_ARGS__))
#define i2_attention_projections(...) MEASURE(6, i2_attention_projections(__VA_ARGS__))
#define i2_moe_ffn(...) MEASURE(7, i2_moe_ffn(__VA_ARGS__))
#define fg_standalone_graph_compute(...) MEASURE(8, fg_standalone_graph_compute(__VA_ARGS__))

extern void fg_cuda_enable_inkling_quants(bool enabled);
extern void i2_cuda_attention_stats(void);
extern int64_t fg_cuda_load_dense(sl_i64 ranges, int64_t bytes);
extern int64_t fg_cuda_free_bytes(void);

enum { model_width=2304, ff=1152, experts=128, active=8 };
typedef struct {
    const uint8_t *data;
    int64_t kind;
    int64_t shape[4];
} weight;

static weight bind(const uint8_t *file, const int64_t *tensors, int64_t index) {
    assert(index >= 0 && index < 704);
    const int64_t *record=tensors+12*index;
    weight w={ .data=file+record[10], .kind=record[2] };
    memcpy(w.shape,record+4,sizeof w.shape);
    return w;
}

static struct ggml_tensor * load(struct ggml_context * ctx, weight w) {
    struct ggml_tensor *t=ggml_new_tensor(ctx,(enum ggml_type)w.kind,4,w.shape);
    assert(t);
    t->data=(void *)(uintptr_t) w.data;
    return t;
}

static sl_f32 float_weights(weight w, int64_t count) {
    assert(w.kind == GGML_TYPE_F32);
    assert(w.shape[0]*w.shape[1]*w.shape[2]*w.shape[3]==count);
    return (sl_f32){(float *)(uintptr_t)w.data,count};
}

static void evaluate(struct ggml_context *ctx, struct ggml_tensor **nodes, int count,
                     float *output, int size) {
    struct ggml_cgraph *graph=ggml_new_graph_custom(ctx,256,false);
    assert(graph);
    for(int i=0;i<count;i++)ggml_build_forward_expand(graph,nodes[i]);
    void *storage=fg_standalone_graph_alloc(graph);
    assert(storage && fg_standalone_graph_compute(graph)==GGML_STATUS_SUCCESS);
    int at=0;
    for(int i=0;i<count;i++) {
        assert(nodes[i]);
        int64_t n=ggml_nelements(nodes[i]);
        assert(at+n<=size);
        memcpy(output+at,nodes[i]->data,(size_t)n*sizeof(float));
        at+=(int)n;
    }
    assert(at==size);
    free(storage);
}


// Test driver for the shared Goose graph executor. Each component's
// floating-point operations and expert decisions are in the runtime.
enum { context_size=128, physical=24, logical=48, key_width=1024, attn_width=3072, out_width=201024 };
struct decoder {
    const uint8_t *data;
    int64_t tensor[768*12], globals[5], core[24*10], aux[24*30], visits[48*4];
    int64_t pos[48*context_size];
    float *keys, *values, *kh, *vh, *ha, *hf;
};

static sl_f32 vec(float *p,int64_t n) { return (sl_f32){p,n}; }
static sl_i64 ints(int64_t *p,int64_t n) { return (sl_i64){p,n}; }
static weight named(struct decoder *d,int64_t id) { return bind(d->data,d->tensor,id); }
static sl_f32 coefficients(struct decoder *d,int64_t id,int64_t n) {
    return float_weights(named(d,id),n);
}

static struct ggml_tensor *input_tensor(struct ggml_context *ctx,float *values,int n) {
    struct ggml_tensor *t=ggml_new_tensor_2d(ctx,GGML_TYPE_F32,n,1);
    assert(t);
    t->data=values;
    return t;
}

static struct ggml_context *context(void) {
    struct ggml_context *ctx=ggml_init((struct ggml_init_params){2*1024*1024,NULL,true});
    assert(ctx);
    return ctx;
}

static void project(struct decoder *d,int64_t id,float *input,int n,float *output,int rows) {
    struct ggml_context *ctx=context();
    struct ggml_tensor *w=load(ctx,named(d,id)), *x=input_tensor(ctx,input,n);
    struct ggml_tensor *result=ggml_mul_mat(ctx,w,x);
    assert(result);
    struct ggml_tensor *nodes[]={result};
    evaluate(ctx,nodes,1,output,rows);
    ggml_free(ctx);
}

static void mix(float *blocks,int count,float *cur,sl_f32 query,int queries,float *out) {
    assert(count>=0 && count<12);
    float states[12*model_width], norm[12*model_width], p[12];
    for(int i=0;i<count;i++) memcpy(states+i*model_width,blocks+(count-1-i)*model_width,model_width*sizeof(float));
    memcpy(states+count*model_width,cur,model_width*sizeof(float));
    int n=(count+1)*model_width;
    assert(i2_depth_mix(vec(states,n),model_width,query,vec(norm,n),vec(p,count+1),vec(out,queries*model_width))==queries);
}

static void control(struct decoder *d,const int64_t *aux,int which,float *current,float *message) {
    float norm[2*model_width], h[64], gain[model_width], forget[model_width];
    assert(i2_rms_gain(vec(current,model_width),vec(NULL,0),1e-6f,vec(norm,model_width))==model_width);
    assert(i2_rms_gain(vec(message,model_width),vec(NULL,0),1e-6f,vec(norm+model_width,model_width))==model_width);
    const int at=18+which*3;
    project(d,aux[at],norm,2*model_width,h,64);
    project(d,aux[at+1],h,64,forget,model_width);
    project(d,aux[at+2],h,64,gain,model_width);
    assert(i2_loop_blend(vec(current,model_width),vec(message,model_width),vec(gain,model_width),
                         vec(forget,model_width),vec(current,model_width))==model_width);
}

static void layer(struct decoder *d,int physical_layer,int slot,int absolute,int sliding,
                  float *cur,float *blocks,int *count,float (*messages)[model_width]) {
    const int64_t *core=d->core+10*physical_layer, *aux=d->aux+30*physical_layer;
    float inputs[3][model_width];
    if(aux[10]>=0) {
        const int n=sliding?1:3;
        mix(blocks,*count,cur,coefficients(d,aux[10],n*model_width),n,inputs[0]);
        if(sliding){memcpy(inputs[1],inputs[0],sizeof inputs[0]);memcpy(inputs[2],inputs[0],sizeof inputs[0]);}
    } else {
        for(int i=0;i<3;i++)memcpy(inputs[i],cur,model_width*sizeof(float));
    }
    for(int i=0;i<3;i++){
        if(messages)control(d,aux,i,inputs[i],messages[i]);
        assert(i2_rms_gain(vec(inputs[i],model_width),coefficients(d,aux[sliding?11:12+i],model_width),
                           1e-6f,vec(inputs[i],model_width))==model_width);
    }

    struct ggml_context *ctx=context();
    int64_t in[3], projection[8]={0}, wp[8]={0}, work[16]={0};
    for(int i=0;i<3;i++)in[i]=(intptr_t)input_tensor(ctx,inputs[i],model_width);
    int64_t ids[]={core[0],core[1],core[2],aux[5],aux[8],core[4],aux[15],aux[16]};
    const int count_proj=sliding?6:8;
    for (int i=0;i<count_proj;i++)wp[i]=(intptr_t)load(ctx,named(d,ids[i]));
    assert(i2_attention_projections((intptr_t)ctx,ints(in,3),ints(wp,8),sliding,
                                    ints(work,16),ints(projection,8))==count_proj);
    struct ggml_tensor *roots[8];
    for(int i=0;i<count_proj;i++)roots[i]=(struct ggml_tensor *)(intptr_t)projection[i];
    float projected[8664], attn[attn_width], scratch[context_size];
    int packed=sliding?8600:8664;
    evaluate(ctx,roots,count_proj,projected,packed);
    ggml_free(ctx);

    const int key_at=slot*context_size*key_width;
    assert(i2_attention_token(vec(projected,packed),coefficients(d,aux[4],3072),
          coefficients(d,aux[7],384),coefficients(d,aux[0],4096),coefficients(d,aux[1],4096),
          coefficients(d,aux[6],16384),absolute,context_size,sliding,ints(d->pos+slot*context_size,context_size),
          vec(d->keys+key_at,context_size*key_width),vec(d->values+key_at,context_size*key_width),
          vec(d->kh+slot*3*key_width,3*key_width),vec(d->vh+slot*3*key_width,3*key_width),
          vec(scratch,context_size),vec(attn,attn_width))==attn_width);
    float proj[model_width], next[model_width], ffn[model_width];
    project(d,core[3],attn,attn_width,proj,model_width);
    assert(i2_postconv_token(vec(proj,model_width),coefficients(d,aux[2],4*model_width),
                            vec(d->ha+slot*3*model_width,3*model_width),vec(next,model_width))==model_width);
    if(physical_layer%6==0){
        assert(*count < 11);
        memcpy(blocks+(*count)*model_width,cur,model_width*sizeof(float));
        (*count)++;
    }else{
        for(int i=0;i<model_width;i++)next[i]+=cur[i];
    }
    mix(blocks,*count,next,coefficients(d,aux[9],model_width),1,ffn);
    if(messages)control(d,aux,3,ffn,messages[3]);
    assert(i2_rms_gain(vec(ffn,model_width),coefficients(d,core[9],model_width),1e-6f,vec(ffn,model_width))==model_width);
    ctx=context();
    int64_t moe_weights[4];
    const int64_t mid[]={core[5],core[8],core[6],core[7]};
    for(int i=0;i<4;i++)moe_weights[i]=(intptr_t)load(ctx,named(d,mid[i]));
    int64_t nodes[8]={0};
    struct ggml_tensor *y=(struct ggml_tensor *)(intptr_t)i2_moe_ffn((intptr_t)ctx,
       (intptr_t)input_tensor(ctx,ffn,model_width),ints(moe_weights,4),ints(work,16),ints(nodes,8));
    assert(y);
    struct ggml_tensor *moe_root[]={y};
    evaluate(ctx,moe_root,1,proj,model_width);
    ggml_free(ctx);
    assert(i2_postconv_token(vec(proj,model_width),coefficients(d,aux[3],4*model_width),
                            vec(d->hf+slot*3*model_width,3*model_width),vec(cur,model_width))==model_width);
    for(int i=0;i<model_width;i++){
        cur[i]+=next[i];
        assert(isfinite(cur[i]));
    }
}

static int token(struct decoder *d,int input,int absolute,int need_logits){
    float cur[model_width], prelude[model_width], bank_output[model_width], blocks[12*model_width];
    float bank[12*model_width], messages[4][model_width];
    int count=0, prelude_count=0, bank_count=0;
    weight emb=named(d,d->globals[0]);
    int64_t row=ggml_row_size(GGML_TYPE_Q8_0,model_width);
    assert(i2_embed_row((sl_u8){(uint8_t *)(uintptr_t)(emb.data+row*input),row},
                        coefficients(d,d->globals[1],model_width),1e-6f,vec(cur,model_width))==model_width);
    for(int i=0;i<logical;i++){
        const int64_t *v=d->visits+4*i;
        const int ip=(int)v[0], slot=(int)v[1], pass=(int)v[2], sliding=(int)v[3];
        if(ip==6){
            if(pass==0){
                memcpy(prelude,cur,sizeof cur);
                prelude_count=count;
            }else{
                bank_count=count-prelude_count-1;
                assert(bank_count>=0 && bank_count<11);
                memcpy(bank,blocks+(prelude_count+1)*model_width,(size_t)bank_count*model_width*sizeof(float));
                memcpy(bank_output,cur,sizeof cur);
                memcpy(cur,prelude,sizeof cur);
                count=prelude_count;
            }
        }
        if(pass>0)mix(bank,bank_count,bank_output,coefficients(d,d->aux[ip*30+17],4*model_width),4,messages[0]);
        layer(d,ip,slot,absolute,sliding,cur,blocks,&count,pass>0?messages:NULL);
    }
    fprintf(stderr,"Pure Goose visited 48 layers for token %d at position %d\n",input,absolute);
    check_state("residual", absolute, cur, model_width);
    if(!need_logits)return -1;
    float normalized[model_width];
    mix(blocks,count,cur,coefficients(d,d->globals[4],model_width),1,normalized);
    assert(i2_rms_gain(vec(normalized,model_width),coefficients(d,d->globals[3],model_width),
                      1e-6f,vec(normalized,model_width))==model_width);
    float *logits=malloc(out_width*sizeof(float));assert(logits);
    project(d,d->globals[2],normalized,model_width,logits,out_width);
    assert(i2_text_logits(vec(logits,out_width),200058,30.0f)==200058);
    check_state("logits", absolute, logits, out_width);
    int best=0;
    for(int i=0;i<200058;i++){assert(isfinite(logits[i]));if(logits[i]>logits[best])best=i;}
    fprintf(stderr,"Goose greedy token %d, logit %.7g\n",best,logits[best]);
    free(logits);
    return best;
}

static void place_range(int64_t *ranges,int64_t *words,int64_t *used,int64_t budget,
                        const uint8_t *host,int64_t bytes) {
    const int64_t aligned=(bytes+31)/32*32;
    if (aligned>budget-*used)return;
    ranges[(*words)++]=(intptr_t)host;
    ranges[(*words)++]=bytes;
    ranges[(*words)++]=*used;
    *used+=aligned;
}

extern int64_t fg_cuda_load_experts(sl_i64 groups, sl_i64 layers, int64_t bytes, int64_t words32, int64_t words64);
extern void fg_cuda_expert_stats(sl_i64 stats);

static int64_t load_inkling_expert_bank(struct decoder *d, int64_t budget) {
    int64_t layers[physical * 4] = {0};
    const int64_t *up0 = d->tensor + 12 * d->core[6];
    const int64_t *down0 = d->tensor + 12 * d->core[7];
    const int64_t up = up0[9] / experts, down = down0[9] / experts;
    if (up <= 0 || down <= 0) return -1;
    for (int l = 0; l < physical; ++l) {
        const int64_t *u = d->tensor + 12 * d->core[10 * l + 6];
        const int64_t *v = d->tensor + 12 * d->core[10 * l + 7];
        if (u[9] != up * experts || v[9] != down * experts) return -1;
        layers[4 * l + 1] = (int64_t)(uintptr_t)(d->data + u[10]);
        layers[4 * l + 3] = (int64_t)(uintptr_t)(d->data + v[10]);
    }
    const int64_t spans = physical * experts;
    const int64_t slots = budget / (up + down) < spans ? budget / (up + down) : spans;
    if (slots < experts) return -1;
    int64_t groups[] = {up, 0, down, experts, physical, slots, 0, spans, physical};
    return fg_cuda_load_experts(ints(groups, 9), ints(layers, physical * 4), slots * (up + down),
                                spans + 4 * slots + 4 + 5 * experts, physical + experts + 5);
}

static int64_t place_inkling_weights(struct decoder *d,int64_t tensor_count,int64_t budget,bool adaptive) {
    int64_t *ranges=calloc((size_t)(tensor_count+24*128*2)*3,sizeof(int64_t));
    assert(ranges);
    int64_t words=0,used=0;
    for(int i=0;i<tensor_count;i++){
        int64_t *r=d->tensor+12*i;
        if(r[2]==GGML_TYPE_Q8_0 && i!=d->globals[0]) {
            place_range(ranges,&words,&used,budget,d->data+r[10],r[9]);
        }
    }
    int64_t dense=used;
    for(int phase=0;!adaptive && phase<2;phase++){
        for(int e=0;e<128;e++){
            for(int l=0;l<24;l++){
                bool loop=l>=6 && l<18;
                if(loop!=(phase==0))continue;
                int64_t *a=d->tensor+12*d->core[l*10+6], *b=d->tensor+12*d->core[l*10+7];
                assert(a[9]%128==0 && b[9]%128==0);
                int64_t up=a[9]/128, down=b[9]/128;
                if((up+31)/32*32+(down+31)/32*32>budget-used) continue;
                place_range(ranges,&words,&used,budget,d->data+a[10]+up*e,up);
                place_range(ranges,&words,&used,budget,d->data+b[10]+down*e,down);
            }
        }
    }
    assert(words>0 && fg_cuda_load_dense(ints(ranges,words),used)==used);
    free(ranges);
    printf("Resident weights: %.2f MiB, dense %.2f MiB\n",used/1048576.0,dense/1048576.0);
    return used;
}

static double now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC,&ts);
    return (double)ts.tv_sec+(double)ts.tv_nsec*1e-9;
}

static void report_piece(sl_u8 header, const uint8_t *data, sl_i64 state, sl_i64 kv, sl_i64 slots, int token_id) {
    int64_t key=gr_find_kv(header,state,kv,slots,(sl_u8){(uint8_t *)"tokenizer.ggml.tokens",21});
    int64_t span[2]={0};
    if(key>=0 && gr_arr_str_span(header,state,kv,key,token_id,ints(span,2))==0)
        printf("Token %d: %.*s\n",token_id,(int)span[1],(char *)data+span[0]);
}

int main(int argc,char **argv){
    const double started=now();
    assert(argc>=3 && argc<64);
    int fd=open(argv[1],O_RDONLY);assert(fd>=0);
    struct stat file;
    assert(fstat(fd,&file)==0 && file.st_size>32*1024*1024);
    const uint8_t *data=mmap(NULL,(size_t)file.st_size,PROT_READ,MAP_PRIVATE,fd,0);
    assert(data!=MAP_FAILED);close(fd);
    struct decoder *d=calloc(1,sizeof(*d)); assert(d); d->data=data;
    int64_t st[8]={0}, kv[256*8]={0}, key_slots[512]={0}, tensor_slots[2048]={0}, layout[9]={0};
    sl_u8 header={(uint8_t *)(uintptr_t)data,32*1024*1024};
    goose_init(0,NULL);
#ifdef I2_NUMERIC_MODULE
    i2_candidate_init(0, NULL);
#endif
    assert(fg_standalone_configure_cpu_decode(1)==0);
    assert(fg_standalone_configure_cpu_batch(1)==0);
    fg_standalone_set_cpu_prefill(false);
    fg_cuda_enable_inkling_quants(true);
    assert(gr_parse(header,file.st_size,ints(st,8),ints(kv,256*8),ints(d->tensor,768*12),
                    ints(key_slots,512),ints(tensor_slots,2048))==0);
    assert(i2_read_layout(header,ints(st,8),ints(kv,256*8),ints(key_slots,512),ints(layout,9),ints(d->visits,48*4))==48);
    assert(i2_bind_text_weights(header,ints(st,8),ints(kv,256*8),ints(key_slots,512),ints(d->tensor,768*12),
                              ints(tensor_slots,2048),ints(layout,9),ints(d->globals,5),ints(d->core,24*10))==24);
    assert(i2_bind_text_aux(header,ints(st,8),ints(kv,256*8),ints(key_slots,512),ints(d->tensor,768*12),
                           ints(tensor_slots,2048),ints(layout,9),ints(d->aux,24*30))==24);
    d->keys=calloc(48*context_size*key_width,sizeof(float)); d->values=calloc(48*context_size*key_width,sizeof(float));
    d->kh=calloc(48*3*key_width,sizeof(float)); d->vh=calloc(48*3*key_width,sizeof(float));
    d->ha=calloc(48*3*model_width,sizeof(float)); d->hf=calloc(48*3*model_width,sizeof(float));
    assert(d->keys && d->values && d->kh && d->vh && d->ha && d->hf);
    i2_reset_cache_positions(ints(d->pos,48*context_size));
    int result=-1;
    int64_t free_before=fg_cuda_free_bytes();
    const int64_t reserve=1536LL*1024*1024;
    assert(free_before>reserve);
    const int64_t limit=7168LL*1024*1024;
    const int64_t budget=free_before-reserve<limit?free_before-reserve:limit;
    const bool adaptive = getenv("I2_DIAGNOSTIC_EXPERT_BANK") != NULL;
    int64_t residency=place_inkling_weights(d,st[3],budget,adaptive);
    if (adaptive) {
        const int64_t bank = load_inkling_expert_bank(d, budget - residency);
        assert(bank > 0);
        printf("Adaptive expert bank: %.2f MiB\n", bank / 1048576.0);
        residency += bank;
    }
    assert(residency>0);
    const double prefill_started=now();
    for(int i=2;i<argc;i++){
        long id=strtol(argv[i],NULL,10);assert(id>=0 && id<200058);
        result=token(d,(int)id,i-2,i+1==argc);
    }
    assert(result>=0);
    const double first=now();
    report_piece(header,data,ints(st,8),ints(kv,256*8),ints(key_slots,512),result);
    printf("TTFT %.6f s; prompt processing %.6f s for %d input tokens\n",
           first-started,first-prefill_started,argc-2);
    const char *requested = getenv("I2_TEST_DECODE_STEPS");
    const int decode_steps = requested ? atoi(requested) : 2;
    assert(decode_steps > 0 && argc - 2 + decode_steps <= context_size);
    for(int i=0;i<decode_steps;i++){
        result=token(d,result,argc-2+i,1);
        assert(result>=0);
        report_piece(header,data,ints(st,8),ints(kv,256*8),ints(key_slots,512),result);
    }
    printf("Decode %.6f tok/s over %d measured output intervals\n",decode_steps/(now()-first),decode_steps);
    printf("GPU allocation increase after context: %.2f MiB\n",(free_before-fg_cuda_free_bytes())/1048576.0);
    const char *activity[] = {"i2_attention_token", "i2_depth_mix", "i2_rms_gain", "i2_loop_blend", "i2_postconv_token", "i2_text_logits", "i2_attention_projections", "i2_moe_ffn", "fg_standalone_graph_compute"};
    for (unsigned i = 0; i < 9; ++i)
        printf("Operation %s count=%u time=%.6f s\n", activity[i], call_count[i], call_seconds[i]);
    int64_t stats[6] = {0};
    fg_cuda_expert_stats(ints(stats, 6));
    printf("Expert statistics: hits=%lld fills=%lld evictions=%lld bytes=%lld pinned=%lld ranges=%lld\n",
           (long long)stats[0], (long long)stats[1], (long long)stats[2],
           (long long)stats[3], (long long)stats[4], (long long)stats[5]);
    i2_cuda_attention_stats();
    free(d->hf);free(d->ha);free(d->vh);free(d->kh);free(d->values);free(d->keys);free(d);
    munmap((void *)(uintptr_t)data,(size_t)file.st_size);
}
