#pragma once
/* APX030 canonical storage: example schema for other workers.
 *
 * This is an example, not a canonical schema.  Each schema owner (MODEL,
 * CONTEXT, KV, VOCAB, ...) publishes its own kind id and field-key ids; the
 * storage layer only stores the signed i64 keys/values it is given and never
 * owns stored handles.
 *
 * Usage (also compiled in canonical-storage-fixture.c):
 *
 *   fgc_handle model = fgc_record_new(FGC_EXAMPLE_KIND_MODEL);
 *   fgc_record_set(model, FGC_EXAMPLE_KEY_N_LAYER, 40);
 *   fgc_record_set(model, FGC_EXAMPLE_KEY_N_EMBD, 2048);
 *   int64_t n_layer = fgc_record_get(model, FGC_EXAMPLE_KEY_N_LAYER);
 *   fgc_record_free(model);
 */

#define FGC_EXAMPLE_KIND_MODEL   INT64_C(1)
#define FGC_EXAMPLE_KEY_N_LAYER  INT64_C(0)
#define FGC_EXAMPLE_KEY_N_EMBD   INT64_C(-1)
