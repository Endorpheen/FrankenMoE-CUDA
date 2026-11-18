// EXP953 differential gate: the Goose reader, reached through the shipping
// adapter, must answer exactly what the native gguf container answers on the
// same file. The native container is the independent oracle; the adapter is the
// tracked file from EXP952, unmodified.
#include "gguf.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

extern "C" void llama_goose_register(const struct gguf_context * ctx, const char * path);
extern "C" void llama_goose_unregister(const struct gguf_context * ctx);
extern "C" int  llama_goose_ready(const struct gguf_context * ctx);
extern "C" int64_t llama_goose_find_kv(const struct gguf_context * ctx, const char * key);
extern "C" int64_t llama_goose_tensor_index(const struct gguf_context * ctx, const char * name);
extern "C" int64_t llama_goose_kv_type(const struct gguf_context * ctx, int64_t index);
extern "C" int64_t llama_goose_arr_type(const struct gguf_context * ctx, int64_t index);
extern "C" int64_t llama_goose_arr_n(const struct gguf_context * ctx, int64_t index);
extern "C" const void * llama_goose_arr_data(const struct gguf_context * ctx, int64_t index, int64_t * bytes);
extern "C" const char * llama_goose_arr_str(const struct gguf_context * ctx, int64_t index, int64_t element);

static int checks = 0;
static int failures = 0;

// gguf_type_size is not part of the public header; the sizes are fixed by the
// wire format and are listed here so the gate does not depend on a private API.
static size_t element_size(enum gguf_type t) {
    switch (t) {
        case GGUF_TYPE_UINT8: case GGUF_TYPE_INT8: case GGUF_TYPE_BOOL: return 1;
        case GGUF_TYPE_UINT16: case GGUF_TYPE_INT16: return 2;
        case GGUF_TYPE_UINT32: case GGUF_TYPE_INT32: case GGUF_TYPE_FLOAT32: return 4;
        case GGUF_TYPE_UINT64: case GGUF_TYPE_INT64: case GGUF_TYPE_FLOAT64: return 8;
        default: return 0;
    }
}

static void check(bool cond, const std::string & what) {
    checks++;
    if (!cond) { failures++; std::printf("FAIL %s\n", what.c_str()); }
}

int main(int argc, char ** argv) {
    if (argc < 2) { std::printf("usage: gate <model.gguf>\n"); return 2; }
    const char * path = argv[1];

    struct gguf_init_params p{};
    p.no_alloc = true;
    p.ctx = nullptr;
    struct gguf_context * ctx = gguf_init_from_file(path, p);
    if (!ctx) { std::printf("native open failed\n"); return 2; }

    llama_goose_register(ctx, path);
    if (!llama_goose_ready(ctx)) { std::printf("adapter not ready\n"); gguf_free(ctx); return 2; }

    const int64_t n_kv = gguf_get_n_kv(ctx);
    const int64_t n_ten = gguf_get_n_tensors(ctx);
    std::printf("native: kv=%lld tensors=%lld\n", (long long) n_kv, (long long) n_ten);

    // ---- keys: name, find, wire type ----
    for (int64_t i = 0; i < n_kv; i++) {
        const char * key = gguf_get_key(ctx, i);
        check(llama_goose_find_kv(ctx, key) == i, std::string("key index ") + key);
        check(llama_goose_kv_type(ctx, i) == (int64_t) gguf_get_kv_type(ctx, i),
              std::string("kv type ") + key);
        if (gguf_get_kv_type(ctx, i) == GGUF_TYPE_ARRAY) {
            check(llama_goose_arr_type(ctx, i) == (int64_t) gguf_get_arr_type(ctx, i),
                  std::string("arr type ") + key);
            const int64_t n = (int64_t) gguf_get_arr_n(ctx, i);
            check(llama_goose_arr_n(ctx, i) == n, std::string("arr n ") + key);
            if (gguf_get_arr_type(ctx, i) == GGUF_TYPE_STRING) {
                for (int64_t e = 0; e < n; e++) {
                    const char * native = gguf_get_arr_str(ctx, i, (size_t) e);
                    const char * goose = llama_goose_arr_str(ctx, i, e);
                    if (native && goose) {
                        check(std::strcmp(native, goose) == 0,
                              std::string("arr str ") + key + "[" + std::to_string(e) + "]");
                    } else {
                        check(native == goose, std::string("arr str presence ") + key);
                    }
                }
            } else {
                int64_t bytes = 0;
                const void * data = llama_goose_arr_data(ctx, i, &bytes);
                const void * native_data = gguf_get_arr_data(ctx, i);
                if (native_data && data) {
                    const size_t native_bytes = gguf_get_arr_n(ctx, i) *
                        element_size(gguf_get_arr_type(ctx, i));
                    check((size_t) bytes == native_bytes, std::string("arr bytes ") + key);
                    check(std::memcmp(data, native_data, native_bytes) == 0,
                          std::string("arr data ") + key);
                } else {
                    check(native_data == data, std::string("arr presence ") + key);
                }
            }
        }
    }

    // ---- absent key and absent tensor must be reported the same way ----
    check(llama_goose_find_kv(ctx, "no.such.key") == -1, "absent key");
    check(llama_goose_tensor_index(ctx, "no.such.tensor") == -1, "absent tensor");

    // ---- tensors: name and index ----
    for (int64_t i = 0; i < n_ten && i < 64; i++) {
        const char * name = gguf_get_tensor_name(ctx, i);
        check(llama_goose_tensor_index(ctx, name) == i, std::string("tensor ") + name);
    }

    llama_goose_unregister(ctx);
    check(!llama_goose_ready(ctx), "unregister clears readiness");
    gguf_free(ctx);

    std::printf("%s checks=%d failures=%d\n", failures == 0 ? "PASS" : "BAD", checks, failures);
    return failures ? 1 : 0;
}
