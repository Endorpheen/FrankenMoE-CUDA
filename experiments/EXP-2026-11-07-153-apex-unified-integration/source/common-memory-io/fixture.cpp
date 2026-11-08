// fixture.cpp - native-vs-Goose fixture for the common memory/IO layer.
// Real functions from patched llama-memory.cpp / llama-io.cpp; Goose path is
// enabled with fg153_common_set_enabled(1), native with (0).
#include "llama-memory.h"
#include "llama-io.h"
#include "llama-batch.h"
#include "llama-ext.h"
#include "ggml.h"
#include "ggml-backend.h"
#include "fg153-common-hooks.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

static int g_fail = 0;
static void num(const char * tc, const char * field, int64_t goose, int64_t native) {
    int ok = goose == native;
    if (!ok) g_fail++;
    std::printf("FG153_TRACE case=%s field=%s goose=%lld native=%lld %s\n",
                tc, field, (long long) goose, (long long) native, ok ? "PASS" : "FAIL");
}
static void stage(const char * s) { std::printf("FG153_STAGE %s\n", s); std::fflush(stdout); }

struct wbuf : llama_io_write_i {
    std::vector<uint8_t> b;
    bool fail_write = false;
    void write(const void * src, size_t n) override {
        if (fail_write) throw std::runtime_error("write-failed");
        auto * p = (const uint8_t *) src;
        b.insert(b.end(), p, p + n);
    }
    void write_tensor(ggml_tensor *, size_t, size_t) override { std::abort(); }
    size_t n_bytes() override { return b.size(); }
};
struct rbuf : llama_io_read_i {
    const std::vector<uint8_t> & b;
    size_t off = 0;
    explicit rbuf(const std::vector<uint8_t> & v) : b(v) {}
    void read(void * dst, size_t n) override {
        if (off + n > b.size()) throw std::runtime_error("truncated");
        std::memcpy(dst, b.data() + off, n); off += n;
    }
    void read_tensor(ggml_tensor *, size_t, size_t) override { std::abort(); }
    size_t n_bytes() override { return off; }
};

struct fake_mem : llama_memory_i {
    mutable ggml_tensor t0{}, t1{}, t2{}, t3{};
    fake_mem() {
        t0.buffer = (ggml_backend_buffer_t)(intptr_t) 0x1000;
        t1.buffer = (ggml_backend_buffer_t)(intptr_t) 0x1000;
        t2.buffer = (ggml_backend_buffer_t)(intptr_t) 0x2000;
        t3.buffer = nullptr;
    }
    llama_memory_context_ptr init_batch(llama_batch_allocr &, uint32_t, bool) override { return nullptr; }
    llama_memory_context_ptr init_full() override { return nullptr; }
    llama_memory_context_ptr init_update(llama_context *, bool) override { return nullptr; }
    void clear(bool) override {}
    bool seq_rm(llama_seq_id, llama_pos, llama_pos) override { return false; }
    void seq_cp(llama_seq_id, llama_seq_id, llama_pos, llama_pos) override {}
    void seq_keep(llama_seq_id) override {}
    void seq_add(llama_seq_id, llama_pos, llama_pos, llama_pos) override {}
    void seq_div(llama_seq_id, llama_pos, llama_pos, int) override {}
    llama_pos seq_pos_min(llama_seq_id) const override { return -1; }
    llama_pos seq_pos_max(llama_seq_id) const override { return -1; }
    std::map<ggml_backend_buffer_type_t, size_t> memory_breakdown() const override { return {}; }
    void state_write(llama_io_write_i & io, llama_seq_id = -1, llama_state_seq_flags = 0) const override {
        uint32_t marker = 0xC0DE;
        io.write(&marker, sizeof(marker));
        io.write_tensor(const_cast<ggml_tensor *>(&t0), 0, 8);
        io.write_tensor(const_cast<ggml_tensor *>(&t1), 0, 8);
        io.write_tensor(const_cast<ggml_tensor *>(&t2), 0, 16);
        io.write_tensor(const_cast<ggml_tensor *>(&t3), 0, 1);
    }
    void state_read(llama_io_read_i &, llama_seq_id = -1, llama_state_seq_flags = 0) override {}
    bool get_can_shift() const override { return true; }
};

static void status_tests() {
    for (int a = 0; a < 4; ++a) {
        for (int b = 0; b < 4; ++b) {
            fg153_common_set_enabled(0);
            int n = (int) llama_memory_status_combine((llama_memory_status) a, (llama_memory_status) b);
            fg153_common_set_enabled(1);
            int g = (int) llama_memory_status_combine((llama_memory_status) a, (llama_memory_status) b);
            num("combine", "value", g, n);
        }
        fg153_common_set_enabled(0);
        int n = llama_memory_status_is_fail((llama_memory_status) a) ? 1 : 0;
        fg153_common_set_enabled(1);
        int g = llama_memory_status_is_fail((llama_memory_status) a) ? 1 : 0;
        num("is_fail", "value", g, n);
    }
    fg153_common_set_enabled(1);
}

static void string_tests() {
    const std::string s = "hello-world";
    wbuf wn, wg;
    fg153_common_set_enabled(0); wn.write_string(s);
    fg153_common_set_enabled(1); wg.write_string(s);
    num("str", "bytes", (int64_t) wg.b.size(), (int64_t) wn.b.size());
    num("str", "equal", wg.b == wn.b ? 1 : 0, 1);

    fg153_common_set_enabled(0); rbuf rn(wn.b); std::string sn; rn.read_string(sn);
    fg153_common_set_enabled(1); rbuf rg(wg.b); std::string sg; rg.read_string(sg);
    num("str", "value", sg == sn ? 1 : 0, 1);
    num("str", "consumed", (int64_t) rg.n_bytes(), (int64_t) rn.n_bytes());

    // zero length
    std::vector<uint8_t> zero(4, 0);
    fg153_common_set_enabled(0); rbuf rz0(zero); std::string s0; rz0.read_string(s0);
    fg153_common_set_enabled(1); rbuf rz1(zero); std::string s1; rz1.read_string(s1);
    num("str-zero", "value", s1 == s0 ? 1 : 0, 1);
    num("str-zero", "empty", (int64_t) s1.size(), 0);

    // truncated: length says 5, only 2 bytes present
    std::vector<uint8_t> tr = { 5, 0, 0, 0, 'a', 'b' };
    int n_throw = 0, g_throw = 0; std::string nmsg, gmsg;
    fg153_common_set_enabled(0);
    try { rbuf r(tr); std::string x; r.read_string(x); } catch (const std::exception & e) { n_throw = 1; nmsg = e.what(); }
    fg153_common_set_enabled(1);
    try { rbuf r(tr); std::string x; r.read_string(x); } catch (const std::exception & e) { g_throw = 1; gmsg = e.what(); }
    num("str-truncated", "threw", g_throw, n_throw);
    num("str-truncated", "message", gmsg == nmsg ? 1 : 0, 1);

    // malformed length: 1000 declared, 4 bytes present
    std::vector<uint8_t> mal = { 0xE8, 0x03, 0x00, 0x00, 'z', 'z', 'z', 'z' };
    n_throw = g_throw = 0; nmsg.clear(); gmsg.clear();
    fg153_common_set_enabled(0);
    try { rbuf r(mal); std::string x; r.read_string(x); } catch (const std::exception & e) { n_throw = 1; nmsg = e.what(); }
    fg153_common_set_enabled(1);
    try { rbuf r(mal); std::string x; r.read_string(x); } catch (const std::exception & e) { g_throw = 1; gmsg = e.what(); }
    num("str-malformed", "threw", g_throw, n_throw);
    num("str-malformed", "message", gmsg == nmsg ? 1 : 0, 1);

    // write failure propagates
    int nw = 0, gw = 0; nmsg.clear(); gmsg.clear();
    fg153_common_set_enabled(0);
    try { wbuf w; w.fail_write = true; w.write_string(s); } catch (const std::exception & e) { nw = 1; nmsg = e.what(); }
    fg153_common_set_enabled(1);
    try { wbuf w; w.fail_write = true; w.write_string(s); } catch (const std::exception & e) { gw = 1; gmsg = e.what(); }
    num("str-write-fail", "threw", gw, nw);
    num("str-write-fail", "message", gmsg == nmsg ? 1 : 0, 1);
    fg153_common_set_enabled(1);
}

static void buffer_tests() {
    fg153_common_set_enabled(0);
    auto vn = llama_memory_get_buffers(llama_memory_t(new fake_mem()));
    fg153_common_set_enabled(1);
    auto vg = llama_memory_get_buffers(llama_memory_t(new fake_mem()));
    num("get-buffers", "count", (int64_t) vg.size(), (int64_t) vn.size());
    for (size_t i = 0; i < vn.size() && i < vg.size(); ++i)
        num("get-buffers", "item", (int64_t)(intptr_t) vg[i], (int64_t)(intptr_t) vn[i]);
    fg153_common_set_enabled(1);
}

int main() {
    stage("status");
    status_tests();
    stage("strings");
    string_tests();
    stage("buffers");
    buffer_tests();
    std::printf("FG153_RESULT failures=%d\n", g_fail);
    return g_fail ? 1 : 0;
}
