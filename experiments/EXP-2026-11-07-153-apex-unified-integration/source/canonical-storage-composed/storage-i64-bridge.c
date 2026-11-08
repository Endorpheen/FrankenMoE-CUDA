/* ABI-only conversions; all storage algorithms execute Goose. */
#include "foundation/canonical-storage.h"
#include <stdint.h>
int64_t fgc_i64_record_free(int64_t h) { fgc_record_free(h); return 0; }
int64_t fgc_i64_vec_free(int64_t h) { fgc_vec_free(h); return 0; }
int64_t fgc_i64_map_free(int64_t h) { fgc_map_free(h); return 0; }
int64_t fgc_i64_vec_data(int64_t h) { return (int64_t)(intptr_t)fgc_vec_data(h); }
int64_t fgc_i64_map_get(int64_t h,int64_t key,int64_t out) { return fgc_map_get(h,key,(int64_t *)(intptr_t)out); }
