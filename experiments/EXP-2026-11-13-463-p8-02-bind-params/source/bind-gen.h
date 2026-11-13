#pragma once
#include <stdint.h>
typedef struct gs_stack gs_stack;
#pragma pack(push, 1)
typedef struct { uint8_t *base; int64_t len; } gs_rhdr;
typedef struct { gs_rhdr *hdr; gs_stack *stk; } gs_rref;
typedef struct { gs_rhdr *hdr; gs_stack *stk; gs_rhdr *fl; gs_stack *flstk; } gs_pref;

typedef struct { uint8_t *data; int64_t len; } sl_u8;

typedef struct { int64_t *data; int64_t len; } sl_i64;

#pragma pack(pop)

#ifdef __cplusplus
extern "C" {
#endif
void goose_init(int argc, char **argv);
int64_t path_match(sl_u8 gs_export_arg0, sl_i64 gs_export_arg1, sl_i64 gs_export_arg2, int64_t gs_export_arg3, sl_u8 gs_export_arg4, sl_i64 gs_export_arg5);
int64_t duplicate(sl_u8 gs_export_arg0_2, sl_i64 gs_export_arg1_2, sl_i64 gs_export_arg2_2);
int64_t bind_keep(int64_t gs_export_arg0_3);
int64_t full_path(sl_u8 gs_export_arg0_4, sl_u8 gs_export_arg1_3, sl_u8 gs_export_arg2_3);
#ifdef __cplusplus
}
#endif
