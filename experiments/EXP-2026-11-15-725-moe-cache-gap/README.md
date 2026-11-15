# EXP-2026-11-15-725 — the Goose expert cache exists, is linked, and is never called

Base: `fa8def24`. Follow-up to the audit: 84 GiB of host-to-device traffic per request is the
bottleneck, so the question was whether the cache that decides expert residency is running.

## 1. The Goose cache modules exist

| module | exports | externs | in build set |
|---|---:|---:|---|
| `goose/cuda/goose-moe-cache-admission.goose` | 0 | 3 | **no** |
| `goose/cuda/goose-moe-cache-copy-decisions.goose` | 0 | 3 | **no** |

The admission module's own header states its purpose: which layers are admitted to the device cache,
which share a group, and the per-group host bytes - i.e. exactly the decision that governs whether an
expert is copied again.

## 2. But in the running library they are dead

`libllama.so.0.7.0` in the pinned runtime **does** define them:

```
T fg262a_enabled
T fg262a_run
T fg263c_enabled
T fg263c_fill_bytes
```

Yet an `objdump` scan for `call <fg262a_run>`, `call <fg262a_enabled>` and `call <fg263c_enabled>`
across the whole library finds **no caller at all**. Same conclusion as the earlier flash-attention
investigation would have been if it had stopped at `grep call` - so this was cross-checked a second
way: an environment sweep below.

## 3. The environment switches do nothing

`GGML_GOOSE_MOE_CACHE_ADMIT` and `GGML_GOOSE_MOE_CACHE_COPY` exist as strings in the library but are
**not set in the reference profile**. Setting both to 1:

| arm | decode | vs base |
|---|---:|---:|
| baseline | 39.86 | - |
| `CACHE_ADMIT=1 CACHE_COPY=1` | 39.42 | -1.1% |

No change in behaviour and no cache markers appear in the log, which confirms the dead-code finding
from the other direction: the switches are read by code that never runs.

## 4. The reference did not use the Goose cache either

`libllama-exp244.so` (the reference build) contains **0** `fg262a` symbols and **22** native
`llama_moe_cache` symbols:

```
T llama_moe_cache::copy_experts(...)
T llama_moe_cache::copy(...)
W llama_moe_cache::impl::copy(...)
```

So in the reference, expert residency was managed by **C++**, not by Goose. The Goose port of that
decision exists in this repository as source, compiles, links into our libllama - and is not wired
to anything.

## 5. What that means

This is the first case in the project where the migration gap and the performance gap are the same
thing, and it is measurable:

- 84 GiB of expert traffic per request, 4.24 s of 7.93 s of decode kernel time (EXP-724);
- the decision that would govern it is Goose code that exists and is never invoked;
- the reference got its 41.94 with C++ making that decision and a larger effective cache.

Wiring `fg262a_run` into the expert copy path is therefore not an optimisation of a working
component - it is finishing a migration that was left half-done, and it is the one place where
doing so could move both goals at once.

## 6. What is not claimed

That wiring it will be faster. The Goose decision may be correct and the copy volume may be
unchanged, in which case the gain is migration, not speed. That is still worth having, and it is
measurable either way with the profile that already exists.

Nor is it claimed that this is a regression we introduced: the reference did not call it either.

## 7. Next step

Find the expert copy path in the native code, identify the admission decision inside it, and route
that decision through `fg262a_run`. Then measure: copies per request, decode, prefill, VRAM, RAM.
