/* fg153-projection.h - minimal typed friend projection for the recurrent class.
 *
 * The parent injects into the patched header:
 *   1. `friend struct fg153_recurrent_projection;` inside class llama_memory_recurrent
 *   2. `#include "fg153-projection.h"` after the class definition
 *   3. the additive tagged ctor `llama_memory_recurrent(fg153_native_init_tag, ...)`
 *      (metadata only, no tensor alloc; the original ctor body is left untouched
 *      for rollback)
 * No struct offsets are invented: all members are reached by real names because
 * this struct is a friend, and every accessor lives in services.cpp.
 */
#ifndef FG153_RECURRENT_PROJECTION_H
#define FG153_RECURRENT_PROJECTION_H

#include <stdint.h>
#include <vector>

struct llama_memory_recurrent;
struct llama_model;
struct ggml_context;
struct ggml_tensor;
struct ggml_backend_buffer;
struct ggml_backend_buffer_type;

struct fg153_recurrent_projection {
    struct bufent {
        ggml_context * ctx;
        ggml_backend_buffer * buf;
        ggml_backend_buffer_type * buft;
    };

    uint64_t magic = 0;
    llama_memory_recurrent * mem = nullptr;
    int32_t empty_override = -1; /* -1: derive from ctxs_bufs, else explicit */
    int32_t adopted = 0;
    int64_t side_count = 0;
    std::vector<bufent> side;    /* Goose-driven construction table pre-adopt */
    const void * filter = nullptr; /* const std::function<bool(int32_t)>* */
    void * model = nullptr;        /* llama_model* for dev_layer() */
    uint64_t reads = 0;
    uint64_t writes = 0;

    int64_t get(int64_t k, int64_t i) const;
    void    put(int64_t k, int64_t i, int64_t v);

    int64_t buffer_count() const;
    int64_t buffer_at(int64_t k) const;
    int64_t buffer_ctx_at(int64_t k) const;
    int64_t buffer_buft_at(int64_t k) const;

    // Note: size aggregation is Goose-owned (recurrent-lifecycle entries 20..24);
    // this projection exposes only per-tensor/per-buffer primitives.

    /* Move the side table into mem->ctxs_bufs with the real owner types. */
    int adopt();

    static fg153_recurrent_projection * open(llama_memory_recurrent * m,
                                             const void * filter, void * model);
    static void close(fg153_recurrent_projection * p);
};

#endif /* FG153_RECURRENT_PROJECTION_H */
