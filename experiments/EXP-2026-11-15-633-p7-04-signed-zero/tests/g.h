#pragma once
#include <stdint.h>
typedef struct gs_stack gs_stack;
#pragma pack(push, 1)
typedef struct { uint8_t *base; int64_t len; } gs_rhdr;
typedef struct { gs_rhdr *hdr; gs_stack *stk; } gs_rref;
typedef struct { gs_rhdr *hdr; gs_stack *stk; gs_rhdr *fl; gs_stack *flstk; } gs_pref;

typedef struct { uint8_t *data; int64_t len; } sl_u8;

typedef struct { float *data; int64_t len; } sl_f32;

#pragma pack(pop)

#ifdef __cplusplus
extern "C" {
#endif
void goose_init(int argc, char **argv);
int64_t qz_dequant(int64_t gs_export_arg0, sl_u8 gs_export_arg1, int64_t gs_export_arg2, sl_f32 gs_export_arg3);
int64_t qz_dequant_block_size(int64_t gs_export_arg0_2);
#ifdef __cplusplus
}
#endif
