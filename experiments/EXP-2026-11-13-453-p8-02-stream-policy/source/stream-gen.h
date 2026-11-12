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
int64_t append(int64_t gs_export_arg0, int64_t gs_export_arg1, int64_t gs_export_arg2, int64_t gs_export_arg3, uint64_t gs_export_arg4);
int64_t read_decision(int64_t gs_export_arg0_2, int64_t gs_export_arg1_2, int64_t gs_export_arg2_2, int64_t gs_export_arg3_2, int64_t gs_export_arg4_2, uint64_t gs_export_arg5);
#ifdef __cplusplus
}
#endif
