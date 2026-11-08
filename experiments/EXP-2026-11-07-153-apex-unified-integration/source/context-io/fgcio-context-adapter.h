// fgcio-context-adapter.h - thin llama_io_* compatibility adapters that forward
// to canonical fgcio_* handles. No C++ buffer ownership and no IO algorithms.
#ifndef FGCIO_CONTEXT_ADAPTER_H
#define FGCIO_CONTEXT_ADAPTER_H

#include "canonical-io.h"
#include "llama-io.h"
#include <cstddef>
#include <cstdint>

struct llama_file;

// Per-call handles: adapter owns and disposes on destruction.
llama_io_write_i * fgcio_adapter_write_dummy(bool skip_tensors);
llama_io_write_i * fgcio_adapter_write_host(uint8_t * dst, size_t size);
llama_io_read_i  * fgcio_adapter_read_host(const uint8_t * src, size_t size);

// Device: the persistent write handle owns the cached buffer/geometry and is
// owned by the context/memory owner (worker5/12). The adapter does NOT dispose
// it; the read adapter borrows the write handle's groups and disposes itself.
llama_io_write_i * fgcio_adapter_write_device(int64_t persistent_write_handle, uint8_t * dst, size_t size);
llama_io_read_i  * fgcio_adapter_read_device(int64_t shared_write_handle);
llama_io_read_i  * fgcio_adapter_read_device_take(int64_t owned_read_handle);

/* canonical additions implemented in this adapter module (Goose) */
extern "C" {
int64_t fgcio_bind_buffer(int64_t h, int64_t ptr, int64_t size);
int64_t fgcio_context_write_device(int64_t ctx, int64_t seq, int64_t dst, int64_t size);
int64_t fgcio_context_read_device(int64_t ctx, int64_t seq, int64_t src, int64_t size);
int64_t fgcio_context_drop(int64_t ctx);
int64_t fgcio_context_has(int64_t ctx,int64_t seq);
}

void fgcio_adapter_free_write(llama_io_write_i * io);
void fgcio_adapter_free_read(llama_io_read_i * io);

// Owner hook required from worker5/12: one persistent fgcio_create(5,0,0,0)
// handle per (context, seq_id), created lazily and disposed with the context.
typedef int64_t (*fgcio_device_handle_fn)(void * owner_ctx, int64_t seq_id);
void fgcio_adapter_set_device_handle_hook(fgcio_device_handle_fn fn, void * owner_ctx);

// File route: llama_file is not a FILE*; canonical file type needs a raw sink.
// Provided only when the owner supplies raw read/write effects (no C++ buffers).
struct fgcio_file_sink {
    void * ctx;
    int64_t (*write)(void * ctx, const void * src, int64_t n);
    int64_t (*read)(void * ctx, void * dst, int64_t n);
};

#endif
