#pragma once
#include <stddef.h>
#include <stdint.h>

struct ggml_tensor;
struct ggml_compute_params;

struct fg006_tensor { int64_t ne[4], nb[4], kind, bytes; };
struct fg006_node {
    struct fg006_tensor x, y, z, d;
    int64_t op, ith, nth, ip[16];
    float fp[8];
};

void fg006_goose_workspace(const struct fg006_node * p, int64_t nth, int64_t info[4]);
uint8_t fg006_goose_validate(const struct fg006_node * p, const void * ids);
uint8_t fg006_goose_compute(const struct fg006_node * p,
        const void * x, const void * y, const void * z, void * dst,
        float * scratch, int64_t nf, uint8_t * raw, int64_t raw_bytes,
        int64_t * indices, int64_t ni);

int fg006_provider_enabled(void);
struct fg006_node fg006_describe(const struct ggml_tensor * t, int ith, int nth);
int fg006_supported_node(const struct fg006_node * p);
uint8_t fg006_try(const struct ggml_compute_params * params, struct ggml_tensor * t);
void fg006_workspace_node(const struct fg006_node * p, int64_t nth, int64_t info[4]);
size_t fg006_extra_workspace(const struct ggml_tensor * t, int n_threads);
size_t fg006_extra_workspace_legacy(const struct ggml_tensor * t, int n_tasks);
int fg006_validate_ids_node(const struct fg006_node * p, const void * ids);
uint8_t fg006_try_raw(int ith, int nth, size_t wsize, void * wdata, int use_ref,
        struct ggml_tensor * t);
