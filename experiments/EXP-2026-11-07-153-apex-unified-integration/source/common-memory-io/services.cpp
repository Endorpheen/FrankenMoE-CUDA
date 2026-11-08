// services.cpp - typed primitives for the common memory/IO Goose module.
// Only byte/string transfers, storage and visitor recording live here; ownership
// loops and the u32-length format policy are in common-memory.goose.
#include "bridge.h"
#include "fg153-common-hooks.h"

#include "llama-io.h"
#include "ggml.h"
#include "ggml-backend.h"

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#define FG153_BOX_MAGIC 0x434D4D4F4E424F58ull /* "CMMONBOX" */

struct fg153_cbox {
    uint64_t magic=FG153_BOX_MAGIC;
    std::vector<int64_t> values;
    int64_t count=0,total=0;
    explicit fg153_cbox(size_t n):values(n) {}
};

// fg153_buffer_visitor is defined inline in fg153-common-hooks.h.

static thread_local std::exception_ptr g_ep;
static thread_local uint32_t g_last_u32 = 0;

std::exception_ptr fg153_common_exception() { return g_ep; }

template <class F>
static int64_t cap(F && f) {
    try { f(); return 1; }
    catch (...) { g_ep = std::current_exception(); return 0; }
}

int64_t fg153_cg(int64_t p,int64_t k,int64_t i) {
    if(!p)return 0;
    auto *b=(fg153_cbox *)(intptr_t)p;if(b->magic!=FG153_BOX_MAGIC)std::abort();
    if(k==11)return b->count;if(k==12)return b->total;
    if(k!=10||i<0||(size_t)i>=b->values.size())std::abort();return b->values[(size_t)i];
}
void fg153_cp(int64_t p,int64_t k,int64_t i,int64_t v) {
    if(!p)std::abort();auto *b=(fg153_cbox *)(intptr_t)p;if(b->magic!=FG153_BOX_MAGIC)std::abort();
    if(k==11){b->count=v;return;}if(k==12){b->total=v;return;}
    if(k!=10||i<0||(size_t)i>=b->values.size())std::abort();b->values[(size_t)i]=v;
}

static std::atomic<uint64_t> g_wit[64];
void fg153_cw(int64_t mode) {
    if (mode >= 0 && mode < 64) g_wit[mode].fetch_add(1, std::memory_order_relaxed);
}

int64_t fg153_cs(int64_t op, int64_t a, int64_t b, int64_t c) {
    switch (op) {
        case 0: std::abort();
        case 1: return (int64_t) ((const std::string *)(intptr_t) a)->size();
        case 2: { // write u32 length/value
            auto * io = (llama_io_write_i *)(intptr_t) a;
            uint32_t v = (uint32_t) b;
            return cap([&] { io->write(&v, sizeof(v)); });
        }
        case 3: { // write bytes of string[0..len)
            auto * io = (llama_io_write_i *)(intptr_t) a;
            auto * s = (const std::string *)(intptr_t) b;
            size_t n = (size_t) c;
            return cap([&] { io->write(s->data(), n); });
        }
        case 4: { // read u32 (status; value via op 7)
            auto * io = (llama_io_read_i *)(intptr_t) a;
            uint32_t v = 0;
            int64_t ok = cap([&] { io->read(&v, sizeof(v)); });
            g_last_u32 = v;
            return ok;
        }
        case 5: { // read len bytes into string (RAII buffer)
            auto * io = (llama_io_read_i *)(intptr_t) a;
            auto * s = (std::string *)(intptr_t) c;
            size_t n = (size_t) b;
            return cap([&] {
                std::vector<char> buf(n);
                io->read(buf.data(), n);
                s->assign(buf.data(), n);
            });
        }
        case 6: {
            auto * s = (std::string *)(intptr_t) a;
            return cap([&] { s->clear(); });
        }
        case 7: return (int64_t) g_last_u32;
        case 20: return (int64_t)(intptr_t) new fg153_cbox((size_t)a);
        case 21: delete (fg153_cbox *)(intptr_t) a; return 0;
        case 22: return (int64_t)(intptr_t) new fg153_buffer_visitor();
        case 23: delete (fg153_buffer_visitor *)(intptr_t) a; return 0;
        case 24: return (int64_t) ((fg153_buffer_visitor *)(intptr_t) a)->total;
        case 25: return (int64_t) ((fg153_buffer_visitor *)(intptr_t) a)->ev.size();
        case 26: return (int64_t)(intptr_t) ((fg153_buffer_visitor *)(intptr_t) a)->ev[(size_t) b].first;
        case 27: return (int64_t) ((fg153_buffer_visitor *)(intptr_t) a)->ev[(size_t) b].second;
        default: std::abort();
    }
}

extern "C" uint64_t fg153_common_witness(int mode){return mode>=0&&mode<64?g_wit[mode].load(std::memory_order_relaxed):0;}
