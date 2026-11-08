// fgcio-context-adapter.cpp - thin forwarding adapters (no IO policy here).
#include "fgcio-context-adapter.h"
#include <memory>
#include <stdexcept>

static const char * k_bounds = "unexpectedly reached end of buffer";

namespace {
class writer final : public llama_io_write_i {
public:
    writer(int64_t h, bool own, bool device) : h_(h), own_(own), device_(device) {}
    ~writer() override {
        if (!h_) return;
        if (device_) fgcio_flush(h_);          // commit geometry, keep cache
        else if (own_) fgcio_dispose(h_);      // flush + free per-call handle
    }
    void write(const void * src, size_t size) override {
        if (fgcio_write(h_, (int64_t)(intptr_t) src, (int64_t) size) < 0)
            throw std::runtime_error(k_bounds);
    }
    void write_tensor(ggml_tensor * t, size_t off, size_t size) override {
        if (fgcio_write_tensor(h_, (int64_t)(intptr_t) t, (int64_t) size, (int64_t) off) < 0)
            throw std::runtime_error(k_bounds);
    }
    size_t n_bytes() override { return (size_t) fgcio_n_bytes(h_); }
private:
    int64_t h_; bool own_; bool device_;
};

class reader final : public llama_io_read_i {
public:
    explicit reader(int64_t h) : h_(h) {}
    ~reader() override { if (h_) { fgcio_flush(h_); fgcio_dispose(h_); } }
    void read(void * dst, size_t size) override {
        if (fgcio_read(h_, (int64_t)(intptr_t) dst, (int64_t) size) < 0)
            throw std::runtime_error(k_bounds);
    }
    void read_tensor(ggml_tensor * t, size_t off, size_t size) override {
        if (fgcio_read_tensor(h_, (int64_t)(intptr_t) t, (int64_t) size, (int64_t) off) < 0)
            throw std::runtime_error(k_bounds);
    }
    void discard() override { if (h_) fgcio_discard(h_); }
    size_t n_bytes() override { return (size_t) fgcio_n_bytes(h_); }
private:
    int64_t h_;
};
} // namespace

llama_io_write_i * fgcio_adapter_write_dummy(bool skip) {
    int64_t h = fgcio_create(0, 0, 0, skip ? 1 : 0);
    return h ? new writer(h, true, false) : nullptr;
}
llama_io_write_i * fgcio_adapter_write_host(uint8_t * dst, size_t size) {
    int64_t h = fgcio_create(1, (int64_t)(intptr_t) dst, (int64_t) size, 0);
    return h ? new writer(h, true, false) : nullptr;
}
llama_io_read_i * fgcio_adapter_read_host(const uint8_t * src, size_t size) {
    int64_t h = fgcio_create(2, (int64_t)(intptr_t) src, (int64_t) size, 0);
    return h ? new reader(h) : nullptr;
}
llama_io_write_i * fgcio_adapter_write_device(int64_t persistent_write_handle, uint8_t * dst, size_t size) {
    if (!persistent_write_handle) return nullptr;
    if (fgcio_bind_buffer(persistent_write_handle, (int64_t)(intptr_t) dst, (int64_t) size) != 0) return nullptr;
    return new writer(persistent_write_handle, false, true);
}
llama_io_read_i * fgcio_adapter_read_device(int64_t shared_write_handle) {
    int64_t h = fgcio_create(6, 0, 0, shared_write_handle);
    return h ? new reader(h) : nullptr;
}
llama_io_read_i * fgcio_adapter_read_device_take(int64_t owned_read_handle) {
    return owned_read_handle ? new reader(owned_read_handle) : nullptr;
}
void fgcio_adapter_free_write(llama_io_write_i * io) { delete io; }
void fgcio_adapter_free_read(llama_io_read_i * io) { delete io; }

static fgcio_device_handle_fn g_dev_fn = nullptr;
static void * g_dev_ctx = nullptr;
void fgcio_adapter_set_device_handle_hook(fgcio_device_handle_fn fn, void * owner_ctx) {
    g_dev_fn = fn; g_dev_ctx = owner_ctx;
}
int64_t fgcio_adapter_device_handle(int64_t seq_id) {
    return g_dev_fn ? g_dev_fn(g_dev_ctx, seq_id) : 0;
}
