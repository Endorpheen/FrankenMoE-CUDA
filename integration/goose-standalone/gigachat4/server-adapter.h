#ifndef GIGA_SERVER_ADAPTER_H
#define GIGA_SERVER_ADAPTER_H
#include "request-state.h"

int giga_server_open(const void *mapping, size_t bytes, int64_t capacity);
void giga_server_reset(void);
int64_t giga_server_step(const int32_t *tokens, int64_t count, int64_t rows,
                         float *logits, int64_t logit_capacity);
#endif
