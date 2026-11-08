#pragma once
#include <stdint.h>
#include <stddef.h>

/* Qualified pure-runtime helpers the generated Goose C expects. */
float fgm_half(uint16_t v);
uint16_t fgm_to_half(float v);
uint32_t fgm_bits(float x);
float fgm_from_bits(uint32_t v);
void fgm_witness(int64_t op, int64_t value);

/* Callable unit: the generated entry function (emitter id verified). */
int64_t fgm_matrix_entry(int64_t mode, const void * w, int64_t wbytes,
                         const void * x, int64_t xbytes, float * y, int64_t ylen,
                         int64_t p0, int64_t p1, int64_t p2, int64_t p3);
