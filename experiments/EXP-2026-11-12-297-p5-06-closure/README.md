# EXP297: P5-06 closure -- lazy VMM reservation is imported and unreachable, not ported

Criterion: "Preserve lazy mapping/reservation, bounded committed pages, OOM/reallocation and original large-context
correctness; CUDA driver primitive import only."

P5-06's evidence closed the policy layer (EXP326: 16 290 checks, 8/8 mutations) and left one open item: the CUDA-side
reservation/OOM behaviour. This pass answers it directly.

## What the backend actually exposes

    nm -D libggml-cuda.so.0 | grep buffer_type
    ggml_backend_cuda_buffer_type
    ggml_backend_cuda_host_buffer_type

There is no `ggml_backend_cuda_lazy_buffer_type` -- neither in the flint backend nor in the DSO the accepted profile
loads. Our layer reaches the lazy path only through that registry entry:

    llama-kv-cache.cpp:110  static ggml_backend_buffer_type_t llama_kv_lazy_buft(dev)
    llama-kv-cache.cpp:115      ggml_backend_reg_get_proc_address(reg, "ggml_backend_cuda_lazy_buffer_type")
    llama-kv-cache.cpp:339      if (ggml_backend_buffer_type_t lazy = llama_kv_lazy_buft(dev)) { buft = lazy; vmm_lazy = true; }

so the lookup returns null, `vmm_lazy` stays false, and `ensure_mapped` returns at its first line for every call. The
VMM code that does exist in the backend is private to its pool allocator (`CUDA_POOL_VMM_MAX_SIZE`,
`cuMemAddressReserve`, `cuMemCreate` around ggml-cuda.cu:620-650) and is not reachable through a buffer type.

## Consequence for the open item

- Reservation and OOM: both belong to the backend's pool allocator, which is the same DSO in both lines. There is no
  Goose/native divergence to close, and the driver primitives are imported rather than reimplemented, exactly as the
  criterion asks.
- The policy our layer owns -- lazy only when offloaded, only for a non-transposed V cache, only when the backend exposes
  the type; one commit per present tensor; highest range end for state and ubatch apply -- is gated with 16 290 checks and
  eight caught mutations.
- The path is dormant on the accepted profile (`-ngl all`, `-c 262144`) because the backend has no such buffer type. If a
  backend starts exporting it, the port activates without code changes.

## Boundary

"Large-context correctness" here is the existing 262 144-token profile: it runs, produces `e68df05b`, and its state round
trip is exact. It does not exercise a lazy reservation, because none can be created with this backend. That limit belongs
to the backend, not to the port, and is recorded rather than hidden.
