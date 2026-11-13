#pragma once
#include <stdint.h>
typedef struct gs_stack gs_stack;
#pragma pack(push, 1)
typedef struct { uint8_t *base; int64_t len; } gs_rhdr;
typedef struct { gs_rhdr *hdr; gs_stack *stk; } gs_rref;
typedef struct { gs_rhdr *hdr; gs_stack *stk; gs_rhdr *fl; gs_stack *flstk; } gs_pref;

typedef struct { int64_t *data; int64_t len; } sl_i64;

typedef struct { uint8_t *data; int64_t len; } sl_u8;

#pragma pack(pop)

#ifdef __cplusplus
extern "C" {
#endif
void goose_init(int argc, char **argv);
int64_t align_max(sl_i64 gs_export_arg0);
int64_t ring_next(int64_t gs_export_arg0_2, int64_t gs_export_arg1);
int64_t chunk(int64_t gs_export_arg0_3, int64_t gs_export_arg1_2, int64_t gs_export_arg2, int64_t gs_export_arg3, int64_t gs_export_arg4, uint64_t gs_export_arg5);
int64_t eligible(int64_t gs_export_arg0_4, int64_t gs_export_arg1_3, int64_t gs_export_arg2_2, int64_t gs_export_arg3_2, int64_t gs_export_arg4_2, int64_t gs_export_arg5_2, int64_t gs_export_arg6, int64_t gs_export_arg7, int64_t gs_export_arg8, int64_t gs_export_arg9, int64_t gs_export_arg10, uint64_t gs_export_arg11);
int64_t before(int64_t gs_export_arg0_5, int64_t gs_export_arg1_4, int64_t gs_export_arg2_3, int64_t gs_export_arg3_3);
#ifdef __cplusplus
}
#endif
