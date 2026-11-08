// fixture.cpp - REAL GGML device roundtrip through the context-aware adapters:
// two context keys, repeat/rebind, geometry change, ON_DEVICE metadata, cache 0.
#include "fgcio-context-adapter.h"
#include "ggml.h"
#include "ggml-cpu.h"
#include "ggml-backend.h"
#include <cstdint>
#include <cstdio>
#include <cstring>

extern "C" int64_t fgc_live_allocs(void);
extern "C" int64_t fgc_live_bytes(void);
static int fails;
static void num(const char * c, const char * f, int64_t g, int64_t w) {
    int ok = g == w; if (!ok) fails++;
    std::printf("FG153_TRACE case=%s field=%s got=%lld want=%lld %s\n",
                c, f, (long long) g, (long long) w, ok ? "PASS" : "FAIL");
}
struct T { ggml_tensor * a, * b, * s0, * s1, * d0, * d1; };
static T mk(ggml_context * c, ggml_backend_buffer_t * buf) {
    T t;
    t.a  = ggml_new_tensor_1d(c, GGML_TYPE_F32, 4);
    t.b  = ggml_new_tensor_1d(c, GGML_TYPE_F32, 4);
    t.s0 = ggml_new_tensor_1d(c, GGML_TYPE_F32, 2);
    t.s1 = ggml_new_tensor_1d(c, GGML_TYPE_F32, 2);
    t.d0 = ggml_new_tensor_1d(c, GGML_TYPE_F32, 2);
    t.d1 = ggml_new_tensor_1d(c, GGML_TYPE_F32, 2);
    *buf = ggml_backend_alloc_ctx_tensors_from_buft(c, ggml_backend_cpu_buffer_type());
    ggml_backend_buffer_clear(*buf, 0);
    return t;
}
static int roundtrip(T & t, int64_t key, int64_t seq, bool split, uint8_t * buf, size_t cap) {
    const uint32_t magic = 0xaf143cd8; const int32_t sid = (int32_t) seq;
    float in[4] = {1.25f, -2.5f, 3.75f, 9.0f};
    ggml_backend_tensor_set(t.a, in, 0, sizeof in);
    ggml_backend_tensor_set(t.s0, in, 0, 8);
    ggml_backend_tensor_set(t.s1, in + 2, 0, 8);
    int64_t w = fgcio_context_write_device(key, seq, (int64_t)(intptr_t) buf, (int64_t) cap);
    if (!w) return 0;
    size_t written = 0;
    {
        llama_io_write_i * io = fgcio_adapter_write_device(w, buf, cap);
        if (!io) return 0;
        io->write(&magic, 4); io->write(&sid, 4);
        if (split) { io->write_tensor(t.s0, 0, 8); io->write_tensor(t.s1, 0, 8); }
        else       { io->write_tensor(t.a, 0, 16); }
        written = io->n_bytes();
        fgcio_adapter_free_write(io); // flush, keep persistent cache
    }
    int64_t r = fgcio_context_read_device(key, seq, (int64_t)(intptr_t) buf, (int64_t) written);
    if (!r) return 0;
    int ok = 1;
    {
        llama_io_read_i * io = fgcio_adapter_read_device_take(r);
        uint32_t m = 0; int32_t sread = 0;
        io->read(&m, 4); io->read(&sread, 4);
        if (m != magic || sread != sid) ok = 0;
        if (split) { io->read_tensor(t.d0, 0, 8); io->read_tensor(t.d1, 0, 8); }
        else       { io->read_tensor(t.b, 0, 16); }
        fgcio_adapter_free_read(io); // flush pending tensor copies then dispose
    }
    float out[4] = {0};
    if (split) { ggml_backend_tensor_get(t.d0, out, 0, 8); ggml_backend_tensor_get(t.d1, out + 2, 0, 8); }
    else       { ggml_backend_tensor_get(t.b, out, 0, 16); }
    return ok && std::memcmp(out, in, sizeof in) == 0;
}
int main() {
    ggml_cpu_init();
    ggml_init_params ip = {1 << 20, nullptr, true};
    ggml_context * c = ggml_init(ip);
    ggml_backend_buffer_t buf = nullptr;
    T t = mk(c, &buf);
    uint8_t b1[256] = {0}, b2[256] = {0};
    num("ctxA", "round1", roundtrip(t, 0x1111, 0, false, b1, sizeof b1), 1);
    num("ctxB", "round1", roundtrip(t, 0x2222, 0, true, b2, sizeof b2), 1);
    num("ctxA", "repeat-rebind", roundtrip(t, 0x1111, 0, true, b1, sizeof b1), 1);
    num("ctxA", "geometry-change", roundtrip(t, 0x1111, 0, false, b1, sizeof b1), 1);
    num("ctxB", "repeat-rebind", roundtrip(t, 0x2222, 0, false, b2, sizeof b2), 1);
    fgcio_context_drop(0x1111);
    num("ctxB", "after-drop-other", roundtrip(t, 0x2222, 0, true, b2, sizeof b2), 1);
    fgcio_context_drop(0x2222);
    num("cache", "live_allocs", fgc_live_allocs(), 0);
    num("cache", "live_bytes", fgc_live_bytes(), 0);
    ggml_backend_buffer_free(buf); ggml_free(c);
    std::printf("FG153_RESULT failures=%d\n", fails);
    return fails ? 1 : 0;
}
