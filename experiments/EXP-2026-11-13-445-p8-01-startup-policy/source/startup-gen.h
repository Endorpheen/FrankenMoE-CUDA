#pragma once
#include <stdint.h>
typedef struct gs_stack gs_stack;
#pragma pack(push, 1)
typedef struct { uint8_t *base; int64_t len; } gs_rhdr;
typedef struct { gs_rhdr *hdr; gs_stack *stk; } gs_rref;
typedef struct { gs_rhdr *hdr; gs_stack *stk; gs_rhdr *fl; gs_stack *flstk; } gs_pref;

typedef struct { uint8_t *data; int64_t len; } sl_u8;

#pragma pack(pop)

#ifdef __cplusplus
extern "C" {
#endif
void goose_init(int argc, char **argv);
int64_t decide(int64_t gs_export_arg0, int64_t gs_export_arg1, int64_t gs_export_arg2, int64_t gs_export_arg3, int64_t gs_export_arg4, int64_t gs_export_arg5, int64_t gs_export_arg6, int64_t gs_export_arg7, int64_t gs_export_arg8, int64_t gs_export_arg9, int64_t gs_export_arg10, int64_t gs_export_arg11, int64_t gs_export_arg12, uint64_t gs_export_arg13);
#ifdef __cplusplus
}
#endif
