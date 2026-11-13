// Probe: count which llama_kv_cache entry points execute in a real 32K/128 pass.
//
// Feasibility was checked before writing this, not assumed: libllama dispatches 830 calls to
// llama_kv_cache symbols through its PLT, so LD_PRELOAD interposition reaches them. Symbols that
// are exported but never called through the PLT (seq_rm/seq_cp from inside the library) would not
// be caught this way, which is why the counters below are limited to names verified in the
// disassembly.
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>

static _Atomic unsigned long long c_prepare, c_init_batch, c_apply_ubatch, c_init_update,
                                 c_state_write, c_state_read, c_state_clear,
                                 c_get_size, c_get_n_kv, c_cpy_k, c_build_k_idxs, c_cpy_v;

__attribute__((destructor)) static void report(void) {
    fprintf(stderr,
            "FG197_KV prepare=%llu init_batch=%llu apply_ubatch=%llu init_update=%llu "
            "state_write=%llu state_read=%llu state_clear=%llu "
            "get_size=%llu get_n_kv=%llu cpy_k=%llu cpy_v=%llu build_k_idxs=%llu\n",
            atomic_load(&c_prepare), atomic_load(&c_init_batch), atomic_load(&c_apply_ubatch),
            atomic_load(&c_init_update), atomic_load(&c_state_write), atomic_load(&c_state_read),
            atomic_load(&c_state_clear), atomic_load(&c_get_size), atomic_load(&c_get_n_kv),
            atomic_load(&c_cpy_k), atomic_load(&c_cpy_v), atomic_load(&c_build_k_idxs));
}
static _Atomic int armed;
static void arm(void) { atomic_store(&armed, 1); }

// Arity-agnostic forwarders: up to six register arguments, all integer or pointer on this ABI.
#define WRAP6(MANGLED, CNT, TYPE)                                              \
    TYPE MANGLED(TYPE a, TYPE b, TYPE c, TYPE d, TYPE e, TYPE f) {             \
        arm();                                                                 \
        atomic_fetch_add(&CNT, 1);                                             \
        static TYPE (*real)(TYPE, TYPE, TYPE, TYPE, TYPE, TYPE);               \
        if (!real) real = (TYPE (*)(TYPE, TYPE, TYPE, TYPE, TYPE, TYPE))        \
                              dlsym(RTLD_NEXT, #MANGLED);                      \
        return real(a, b, c, d, e, f);                                         \
    }

typedef unsigned long X;
WRAP6(_ZN14llama_kv_cache7prepareERKSt6vectorI12llama_ubatchSaIS1_EE, c_prepare, X)
WRAP6(_ZN14llama_kv_cache10init_batchER18llama_batch_allocrjb, c_init_batch, X)
WRAP6(_ZN14llama_kv_cache12apply_ubatchERKNS_9slot_infoERK12llama_ubatch, c_apply_ubatch, X)
WRAP6(_ZN14llama_kv_cache11init_updateEP13llama_contextb, c_init_update, X)
WRAP6(_ZN14llama_kv_cache11state_writeEP13llama_io_write_i, c_state_write, X)
WRAP6(_ZN14llama_kv_cache10state_readEP12llama_io_read_iij, c_state_read, X)
WRAP6(_ZN14llama_kv_cache11state_clearEi, c_state_clear, X)
WRAP6(_ZNK14llama_kv_cache8get_sizeEv, c_get_size, X)
WRAP6(_ZNK22llama_kv_cache_context8get_n_kvEv, c_get_n_kv, X)
WRAP6(_ZNK22llama_kv_cache_context5cpy_kEP12ggml_contextP11ggml_tensorS3_i, c_cpy_k, X)
WRAP6(_ZNK22llama_kv_cache_context5cpy_vEP12ggml_contextP11ggml_tensorS3_i, c_cpy_v, X)
WRAP6(_ZNK22llama_kv_cache_context18build_input_k_idxsEP12ggml_contextRK12llama_ubatch, c_build_k_idxs, X)
