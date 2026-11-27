#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "native-kv.h"

struct decoder;

enum { INKLING_TEXT_VOCAB = 200058 };

struct decoder *inkling_backend_open(const uint8_t *image, size_t bytes, int64_t capacity,
                                     int64_t expert_limit, enum inkling_kv_format format);
int inkling_backend_reset(struct decoder *backend);
int64_t inkling_backend_eval(struct decoder *backend, const int32_t *tokens, int64_t count,
                             bool prefill, float *logits, int64_t logits_count);
