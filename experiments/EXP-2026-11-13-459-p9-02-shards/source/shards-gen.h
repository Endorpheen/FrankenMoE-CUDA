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
int64_t digits(int64_t gs_export_arg0);
int64_t width(int64_t gs_export_arg0_2);
int64_t split_path(sl_u8 gs_export_arg0_3, int64_t gs_export_arg1, int64_t gs_export_arg2, sl_u8 gs_export_arg3);
int64_t first_shard_ok(int64_t gs_export_arg0_4);
int64_t split_count_ok(int64_t gs_export_arg0_5, int64_t gs_export_arg1_2);
int64_t shard_index_ok(int64_t gs_export_arg0_6, int64_t gs_export_arg1_3);
int64_t tensor_count_ok(int64_t gs_export_arg0_7, int64_t gs_export_arg1_4);
int64_t magic_ok(int64_t gs_export_arg0_8);
int64_t version_ok(int64_t gs_export_arg0_9);
#ifdef __cplusplus
}
#endif
