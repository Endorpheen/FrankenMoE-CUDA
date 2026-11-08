// fg153-common-hooks.h - default-on hooks for common memory/IO free functions.
#ifndef FG153_COMMON_HOOKS_H
#define FG153_COMMON_HOOKS_H

#include "llama-memory.h"
#include "llama-io.h"
#include "ggml.h"
#include "ggml-backend.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

// Raw event recorder: no dedup/sort (that policy is Goose-owned). Shared by
// services.cpp and hooks.cpp; definition is inline to keep one ODR entity.
struct fg153_buffer_visitor : llama_io_write_i {
    uint64_t total = 0;
    std::vector<std::pair<ggml_backend_buffer_t, size_t>> ev;
    void write(const void *, size_t n) override { total += n; }
    void write_tensor(ggml_tensor * t, size_t, size_t n) override {
        ev.emplace_back(t ? t->buffer : nullptr, n);
        total += n;
    }
    size_t n_bytes() override { return (size_t) total; }
};

extern "C" int64_t fg153_goose_common_entry(int64_t mode, int64_t a, int64_t b,
                                            int64_t c, int64_t d, int64_t e);
extern "C" int64_t fg153_cg(int64_t, int64_t, int64_t);
extern "C" void    fg153_cp(int64_t, int64_t, int64_t, int64_t);
extern "C" int64_t fg153_cs(int64_t, int64_t, int64_t, int64_t);

bool fg153_common_hooks_on();
void fg153_common_set_enabled(int on);

// C++ exception captured by services during a primitive (rethrow without
// unwinding through the emitted C frames).
std::exception_ptr fg153_common_exception();

llama_memory_status fg153_common_hook_status_combine(llama_memory_status s0, llama_memory_status s1);
bool fg153_common_hook_status_is_fail(llama_memory_status status);
std::vector<ggml_backend_buffer_t> fg153_common_hook_get_buffers(const llama_memory_t & mem);
void fg153_common_hook_write_string(llama_io_write_i * io, const std::string & str);
void fg153_common_hook_read_string(llama_io_read_i * io, std::string & str);

#endif /* FG153_COMMON_HOOKS_H */
