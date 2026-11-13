# EXP197: which llama_kv_cache entry points a real Apex pass executes

Not a port yet. This is the measurement that has to come first, because `llama-kv-cache.cpp` is
2965 lines and the profile may enter only a fraction of it.

## How it was measured

`libllama` dispatches **830** calls to `llama_kv_cache` symbols through its PLT — checked in the
disassembly before writing the probe, not assumed. So `LD_PRELOAD` interposition reaches them,
the same mechanism the CUDA modules use.

Two defects in the probe itself, both found by running it:

1. Reported from `std::atexit`, which does not run once the shared library is unloaded — the same
   defect EXP188 and EXP195 hit. `__attribute__((destructor))` fixed it.
2. Two mangled names were wrong: `R18llama_batch_allocr`, not `R16`, and `RK12llama_ubatch`, not
   `RK11`. A wrong name simply never binds, so the counter stayed at zero and looked like "the
   method is not called" rather than "the probe is wrong". Both were read off `nm` after that.

The runner also writes its results under `runtime/exp195/results` because it was derived from that
stage's copy; the first reading was missed for exactly that reason.

## Result — one 32K/128 pass

    FG197_KV prepare=138 init_batch=0 apply_ubatch=276 init_update=138
              state_write=0 state_read=0 state_clear=0
              get_size=617 get_n_kv=150 cpy_k=170 cpy_v=170 build_k_idxs=17

## What this says about the port

- `prepare`, `apply_ubatch`, `init_update`, `get_size`, `get_n_kv`, `cpy_k`, `cpy_v` and
  `build_k_idxs` all execute and are the real targets.
- `init_batch` is **zero** on this profile, and so are `state_write`, `state_read` and
  `state_clear` — the serialization and state-transfer paths do not run here. Porting them first
  would be code nothing reaches, which is what this measurement exists to prevent.
- `cpy_k`/`cpy_v` at 170 each are the Q4_0 cache writes into the attention path, and the
  dequantization side of that is already Goose (EXP173).

So the entry point for a port is the small set above, roughly a fifth of the file's surface, and
the rest stays native until a profile exercises it.

## Reproducing

    gcc-13 -O2 -fPIC -shared source/probe-shim.c -o /tmp/libfg197probe.so -ldl
    # runner = EXP196's run_apex_probe.py with the preload path swapped
    grep FG197_KV runtime/exp195/results/apex-kvprobe-*/server.log
