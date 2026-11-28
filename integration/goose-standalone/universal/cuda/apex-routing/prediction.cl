typedef uint uint32_t;
typedef int int32_t;
typedef ulong uint64_t;
typedef long int64_t;
typedef uchar uint8_t;
#define gs_mul_i64(a,b,file,line) ((a)*(b))
#define gs_add_i64(a,b,file,line) ((a)+(b))
#define gs_add_i32(a,b,file,line) ((a)+(b))
#define gs_shr_i32(a,b,...) ((a)>>(b))
#define GS_RANGE(a,low,high,type,file,line) (a)

static float pred_load_logit(uint64_t address, int32_t i) { return ((__global volatile float *)address)[i]; }
static void pred_store_logit(uint64_t address, int32_t i, float v) { ((__global volatile float *)address)[i] = v; }
static float pred_shuffle(float value, int32_t offset) { return __nvvm_shfl_sync_bfly_f32(0xffffffffu, value, offset, 31); }
static void pred_sync(void) { __nvvm_bar_sync(0); }
static void pred_fence_gpu(void) { __nvvm_membar_gl(); }
static void pred_fence_system(void) { __nvvm_membar_sys(); }
static int32_t pred_mark_done(uint64_t address) { return __nvvm_atom_add_gen_i((__global int *)address, 1); }
static void pred_reset(uint64_t address) { *(__global unsigned *)address = 0; }
static void pred_publish(uint64_t address, int32_t i, int32_t v) { ((__global volatile int *)address)[i] = v; }

#include "prediction-device.h"

// Fused route prediction for expert prefetch: 256 blocks compute one logit each
// (W row . x, F32); the last block to finish ranks the 256 logits and writes the
// K largest indices (ties to the lower index) and then `seq` into host-mapped
// memory. The output only chooses which expert weights are copied early.
__kernel void goose_pred_route(__global const float * restrict w, __global const float * restrict x,
        __global float * logits, __global unsigned * counter, __global volatile int * host, int width, int k, int seq) {
    __local float part[8];
    __local float v[256];
    __local int last;
    const int row = __nvvm_read_ptx_sreg_ctaid_x(), t = __nvvm_read_ptx_sreg_tid_x();
    apex_predict_route_g((uint64_t)w, (uint64_t)x, (uint64_t)logits, (uint64_t)counter, (uint64_t)host,
        (uint64_t)part, (uint64_t)v, (uint64_t)&last, width, k, seq, row, t);
}
