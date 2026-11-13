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
int64_t body_mode(int64_t gs_export_arg0);
int64_t accel_override(int64_t gs_export_arg0_2);
int64_t status_for(int64_t gs_export_arg0_3, int64_t gs_export_arg1);
int64_t log_skip(int64_t gs_export_arg0_4);
int64_t content_type(int64_t gs_export_arg0_5, int64_t gs_export_arg1_2, uint64_t gs_export_arg2);
#ifdef __cplusplus
}
#endif
