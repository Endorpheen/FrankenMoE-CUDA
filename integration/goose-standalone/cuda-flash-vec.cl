#include "flash-vec-types.h"

static float fg175v_ld_f32(uint64_t p, uint64_t off) { return fg_device_global_load(p + off); }
static void fg175v_st_f32(uint64_t p, uint64_t off, float v) { fg_device_global_store(p + off, v); }
static float fg175v_powf(float base, int32_t exp) { return fg_attn_exp(fg_vector_log(base) * (float) exp); }
static int32_t fg175v_round_i8(float v) { return fg180q_i32(fg180q_round(v)); }
#include "cuda-flash-vec-ptx-0.h"
static void fg175v_lc(void) {}
static void fg175v_sync(void) {}


#include "flash-vec-device.h"

#include "cuda-flash-vec-entries.h"
