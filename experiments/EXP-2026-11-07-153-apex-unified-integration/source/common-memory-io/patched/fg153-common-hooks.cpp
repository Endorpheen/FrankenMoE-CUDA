// fg153-common-hooks.cpp - default-on Goose hooks for status/string/state-buffer
// collection. Byte transfers and storage stay in services.cpp; ordering, u32
// length format and set semantics are in common-memory.goose.
#include "fg153-common-hooks.h"
#include "bridge.h"

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <memory>

static std::atomic<bool> g_on{true};
static const bool g_env_init = [] {
    const char * v = getenv("FG153_GOOSE_COMMON");
    g_on.store(!v || strcmp(v, "0") != 0);
    return true;
}();

bool fg153_common_hooks_on() { (void) g_env_init; return g_on.load(std::memory_order_relaxed); }
void fg153_common_set_enabled(int on) { g_on.store(on != 0, std::memory_order_relaxed); }

llama_memory_status fg153_common_hook_status_combine(llama_memory_status s0, llama_memory_status s1) {
    return (llama_memory_status) fg153_goose_common_entry(0, (int64_t) s0, (int64_t) s1, 0, 0, 0);
}
bool fg153_common_hook_status_is_fail(llama_memory_status status) {
    return fg153_goose_common_entry(1, (int64_t) status, 0, 0, 0, 0) != 0;
}

std::vector<ggml_backend_buffer_t> fg153_common_hook_get_buffers(const llama_memory_t & mem) {
    std::unique_ptr<fg153_buffer_visitor> vis((fg153_buffer_visitor *)(intptr_t) fg153_cs(22,0,0,0));
    mem->state_write(*vis);
    auto release=[](void *p){fg153_cs(21,(int64_t)(intptr_t)p,0,0);};
    std::unique_ptr<void,decltype(release)> owned((void *)(intptr_t) fg153_goose_common_entry(4,(int64_t)(intptr_t)vis.get(),0,0,0,0),release);
    int64_t out=(int64_t)(intptr_t)owned.get();
    int64_t n=fg153_cg(out,11,0);
    std::vector<ggml_backend_buffer_t> res;
    res.reserve((size_t)n);
    for(int64_t i=0;i<n;++i)res.push_back((ggml_backend_buffer_t)(intptr_t)fg153_cg(out,10,i));
    return res;
}

void fg153_common_hook_write_string(llama_io_write_i * io, const std::string & str) {
    int64_t ok = fg153_goose_common_entry(2, (int64_t)(intptr_t) io, (int64_t)(intptr_t) &str, 0, 0, 0);
    if (!ok) std::rethrow_exception(fg153_common_exception());
}
void fg153_common_hook_read_string(llama_io_read_i * io, std::string & str) {
    int64_t ok = fg153_goose_common_entry(3, (int64_t)(intptr_t) io, (int64_t)(intptr_t) &str, 0, 0, 0);
    if (!ok) std::rethrow_exception(fg153_common_exception());
}
