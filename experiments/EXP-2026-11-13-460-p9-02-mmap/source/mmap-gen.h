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
int64_t load_flags(int64_t gs_export_arg0, int64_t gs_export_arg1, uint64_t gs_export_arg2);
int64_t align_ok(int64_t gs_export_arg0_2, int64_t gs_export_arg1_2);
int64_t chunk_plan(int64_t gs_export_arg0_3, int64_t gs_export_arg1_3, int64_t gs_export_arg2_2, uint64_t gs_export_arg3);
int64_t staging_size(int64_t gs_export_arg0_4, uint64_t gs_export_arg1_4);
int64_t mlock_step(int64_t gs_export_arg0_5, int64_t gs_export_arg1_5, int64_t gs_export_arg2_3, int64_t gs_export_arg3_2, uint64_t gs_export_arg4);
int64_t align_range(int64_t gs_export_arg0_6, int64_t gs_export_arg1_6, int64_t gs_export_arg2_4, uint64_t gs_export_arg3_3);
#ifdef __cplusplus
}
#endif
