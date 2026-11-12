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
int64_t props_keys(int64_t gs_export_arg0, uint64_t gs_export_arg1);
int64_t props_mode(int64_t gs_export_arg0_2, uint64_t gs_export_arg1_2);
int64_t post_props_decision(int64_t gs_export_arg0_3, uint64_t gs_export_arg1_3);
#ifdef __cplusplus
}
#endif
