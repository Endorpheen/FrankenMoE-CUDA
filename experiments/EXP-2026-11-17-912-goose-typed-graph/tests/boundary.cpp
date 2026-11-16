#define _GNU_SOURCE 1
#include "ggml.h"
#include "runtime.h"
#include <dlfcn.h>
#include <link.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

template<class T> static T sym(void * library, const char * name) {
    void * pointer = dlsym(library, name);
    if (!pointer) { std::fprintf(stderr, "FAIL missing %s\n", name); std::exit(2); }
    return reinterpret_cast<T>(pointer);
}

static void require(bool value, const char * message) {
    if (!value) { std::fprintf(stderr, "FAIL %s\n", message); std::exit(3); }
}

int main(int argc, char ** argv) {
    if (argc != 3) return 2;
    alarm(5);
    void * library = dlmopen(LM_ID_NEWLM, argv[1], RTLD_NOW | RTLD_LOCAL);
    require(library != nullptr, "dlmopen");
    auto init = sym<decltype(&ggml_init)>(library, "ggml_init");
    auto free_context = sym<decltype(&ggml_free)>(library, "ggml_free");
    auto used = sym<decltype(&ggml_used_mem)>(library, "ggml_used_mem");
    auto tensor = sym<decltype(&ggml_new_tensor_1d)>(library, "ggml_new_tensor_1d");
    auto legacy = sym<decltype(&gb_common)>(library, "gb_common");
    ggml_context * context = init({1024 * 1024, nullptr, true});
    require(context != nullptr, "context");
    ggml_tensor * input = tensor(context, GGML_TYPE_F32, 8);
    const size_t page = (size_t) sysconf(_SC_PAGESIZE);
    void * storage = mmap(nullptr, page * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    require(storage != MAP_FAILED, "mmap guard");
    require(mprotect((char *) storage + page, page, PROT_NONE) == 0, "protected page");
    uint64_t * packet = (uint64_t *) ((char *) storage + page - 7 * sizeof(uint64_t));
    packet[0] = 0;
    packet[1] = (uintptr_t) context;
    packet[2] = (uintptr_t) input;
    packet[3] = packet[4] = 0;
    packet[5] = GGML_OP_DUP;
    packet[6] = 0;
    ggml_tensor * result = (ggml_tensor *) (intptr_t) legacy((int64_t) (uintptr_t) packet);
    require(result && result->op == GGML_OP_DUP && result->src[0] == input, "legacy seven-word packet");
    int short_cases = 0;
    if (std::atoi(argv[2])) {
        auto typed = sym<decltype(&gb_common_args)>(library, "gb_common_args");
        for (int64_t length : {-1, 0, 7, 15}) {
            const size_t before = used(context);
            require(typed({(int64_t *) packet, length}) == 0, "short slice accepted");
            require(used(context) == before, "short slice mutated arena");
            ++short_cases;
        }
        int64_t full[16] = {0, (int64_t) (uintptr_t) context, (int64_t) (uintptr_t) input, 0, 0, GGML_OP_DUP, 0};
        result = (ggml_tensor *) (intptr_t) typed({full, 16});
        require(result && result->op == GGML_OP_DUP && result->src[0] == input, "full typed slice");
    }
    require(munmap(storage, page * 2) == 0, "unmap guard");
    free_context(context);
    std::printf("PASS graph-entry-boundary protected_legacy_words=7 short_slice_cases=%d\n", short_cases);
}
