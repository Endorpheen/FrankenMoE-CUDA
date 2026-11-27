#ifndef BONSAI_HTTP_MTP_ABI_H
#define BONSAI_HTTP_MTP_ABI_H

void bonsai_mtp_goose_init(int argc, char **argv);
int64_t bonsai_mtp_target_graph(uint64_t context, sl_i64 weights, sl_i64 states,
    sl_i64 globals, sl_i64 scratch, int64_t token, int64_t position, int64_t want_logits);
int64_t bonsai_mtp_head(uint64_t context, sl_i64 weights, sl_i64 states,
    sl_i64 draft, sl_i64 globals, sl_i64 scratch, int64_t token, int64_t position);
int64_t bonsai_mtp_kv(uint64_t context, sl_i64 weights, sl_i64 states,
    sl_i64 draft, sl_i64 globals, sl_i64 scratch, int64_t token, int64_t position);
int64_t bonsai_mtp_kv_batch(uint64_t context, sl_i64 weights, sl_i64 states,
    sl_i64 draft, sl_i64 globals, sl_i64 scratch, int64_t token, int64_t position);

#endif
