# EXP P4-12: meta backend policy, registry identity, threading services

Ported slices of the APEX_COMMON backend layer to Goose, with a differential
gate against the real native code. No GPU, no driver and no production change:
the native oracle runs `ggml-backend-meta.cpp` on synthetic devices and calls
`ggml_threadpool_params_*` and `bmoe::validate` from the base tree.

Base commit: `df3ce73a` (2026-11-15). Native tree: `upstream/llama.cpp`.
Experiment id is provisional; the lead assigns it at integration.

## Slices

Three atomic slices, 23 exported functions, one Goose file.

### S1 `mb_` - meta device and buffer-type policy

`ggml-backend-meta.cpp` lines 54-392.

| export | native source |
|---|---|
| `mb_name`, `mb_description` | device context constructor, 62-77 |
| `mb_free_sum`, `mb_total_sum` | `ggml_backend_meta_device_get_memory`, 99-110 |
| `mb_caps_and` | `ggml_backend_meta_device_get_props`, 118-146 |
| `mb_device_type` | `ggml_backend_meta_device_get_type`, 112-116 |
| `mb_supports_op` | `ggml_backend_meta_device_supports_op`, 154-159 |
| `mb_supports_buft` | `ggml_backend_meta_device_supports_buft`, 161-178 |
| `mb_alignment_max` | `ggml_backend_meta_buffer_type_get_alignment`, 293-302 |
| `mb_max_size_min` | `ggml_backend_meta_buffer_type_get_max_size`, 304-311 |
| `mb_alloc_size_max` | `ggml_backend_meta_buffer_type_get_alloc_size`, 313-321 |
| `mb_is_host` | `ggml_backend_meta_buffer_type_is_host`, 323-331 |
| `mb_host_buft` | `ggml_backend_meta_device_get_host_buffer_type`, 373-392 |

Two base-commit behaviours the gate pinned down, both reproduced on purpose:

1. Names are constant `Meta()`. The constructors build the name from the
   by-value `simple_devs` / `simple_bufts` parameter *after* it was moved into
   the member (lines 62-77 and 256-265). libstdc++ leaves the moved-from vector
   empty, so the join loop never runs. This is an upstream defect, not a policy
   choice: the port reproduces the observed output and the intended join is
   listed in `REPORT.md` as a follow-up.
2. Caps start as `async = true`, `mmap_support = true` and the other three
   `false` (lines 130-136), so a meta device always reports
   `host_buffer = false`, `buffer_from_host_ptr = false`, `events = false`
   regardless of the members.

### S2 `rg_` - registry identity

`ggml-backend-meta.cpp` lines 215-245 and 346-371. The native registry keys on
the ordered device sequence plus the split-state callback and its userdata, and
the buffer type is memoized per meta device handle.

| export | rule |
|---|---|
| `rg_key_eq` | ordered device equality, then callback, then userdata |
| `rg_memo_slot` | existing slot on a hit, otherwise the next free slot |
| `rg_buft_slot` | one buffer-type slot per distinct meta device slot |

The lexicographic map order (`operator<`) is not ported: it orders raw pointers
and cannot be reproduced deterministically across processes. The gate therefore
checks pointer equality relations, which is exactly what the map key decides.

### S3 `th_` / `rt_` - thread-count and affinity policy

`ggml.c` lines 8045-8065, `ggml-cpu.c` lines 2682-2712 and 3384-3410,
`runtime/core/src/config.cpp` lines 21-23 and 155-159.

| export | decision |
|---|---|
| `th_params_default` | n_threads copied verbatim, prio 0, poll 50, strict_cpu false, paused false, cpumask zero |
| `th_params_match` | n_threads, prio, poll, strict_cpu and cpumask; paused ignored |
| `th_cpumask_valid` | any bit set |
| `th_cpumask_next` | strict round-robin with subtract-512 wrap, non-strict full copy |
| `th_plan_threads` | non-positive request means pool width or 4, then clamp to the pool |
| `rt_n_threads_ok` | `n_threads > 0` |
| `rt_io_threads_ok` | `1 <= moe.io_threads <= 8` |

`th_cpumask_next`, `th_cpumask_valid` and `th_plan_threads` are static native
helpers, so the fixture uses a line-equivalent C++ transcription as the oracle;
the transcription is marked in `tests/fixture.cpp`. The other five functions
call the real exported native code.

## Gate

67 comparisons: 24 in S1, 7 in S2, 36 in S3. The oracle is the real meta
backend instantiated with synthetic `ggml_backend_device` structs whose iface
returns controlled names, memory, caps, buffer types and support flags. This
exercises the actual native getters without a GPU: the meta backend's device
and buffer-type layer only needs handles and iface callbacks.

```
summary pass=67 fail=0 total=67
```

Mutation gate: 12 mutations, two halves of six, port rebuilt at `-O0` for every
mutation. All 12 are killed by a specific case; see `tests/mutations.txt`.

```
half A: killed 6/6
half B: killed 6/6
```

## Layout

```
source/runtime.goose   the three slices, exports mb_/rg_/th_/rt_
tests/fixture.cpp      native oracle vs port, 67 comparisons
tests/build.sh         gate build (-O1) and run, root derived from the script
tests/mutate.py        12 mutations in two halves, -O0 builds
tests/gate.txt         recorded gate output
tests/mutations.txt    recorded mutation results
```

Generated C, objects and gate binaries stay under `build/` and are not part of
the evidence; the Goose source plus these results reproduce them.

Reproduce:

```
bash tests/build.sh
python3 tests/mutate.py --half A --jobs 1
python3 tests/mutate.py --half B --jobs 1
```

## Integration step

The port is a new canon module; production code is not touched.

1. File: `goose/cpu/goose-meta-backend.goose`, containing all 23 exports with
   their current names. A new file is needed because no canon module owns
   `ggml-backend-meta.cpp`, and the `goose/cpu/` tree already carries the
   backend API and job-pool modules.
2. Order inside the file: S1, S2, S3 as written; helpers `put_byte` and
   `put_meta_empty` stay private.
3. `goose/MANIFEST.json`: add an entry with `goose` set to the new path,
   `native_targets` = `ggml/src/ggml-backend-meta.cpp`,
   `ggml/src/ggml-threading.cpp`, `ggml/src/ggml-cpu/ggml-cpu.c` (cpumask and
   plan clamp) and `runtime/core/src/config.cpp`, and `experiment` set to the
   accepted EXP id. Regenerate with `tools/sync-goose-tree.py`.
4. Native adapter, one call site per getter, in this order:
   - `ggml_backend_meta_device_get_name/description` -> `mb_name` /
     `mb_description` with the joined member names as the blob argument (the
     blob is currently ignored, see the defect note);
   - `get_memory` -> `mb_free_sum` and `mb_total_sum` over the simple devices'
     `free` and `total` arrays;
   - `get_props` -> `mb_caps_and` over five-bit capability words, and
     `mb_device_type` for `props->type`;
   - `supports_op` -> `mb_supports_op` over per-device support flags;
   - `supports_buft` -> `mb_supports_buft` with both meta flags and both
     ordered device-id arrays;
   - buffer-type getters -> `mb_alignment_max`, `mb_max_size_min` (pass `-1`
     for `SIZE_MAX`), `mb_alloc_size_max`, `mb_is_host`;
   - `get_host_buffer_type` -> `mb_host_buft` over host buffer-type ids, `-1`
     meaning "none";
   - registry constructors -> `rg_key_eq` for cache lookup and
     `rg_memo_slot` for insert-or-find; `rg_buft_slot` for the buffer-type map;
   - `ggml_threadpool_params_init` -> `th_params_default`; matching ->
     `th_params_match`; worker placement -> `th_cpumask_next` and
     `th_cpumask_valid`; plan and compute -> `th_plan_threads`;
   - `validate()` -> `rt_n_threads_ok` and `rt_io_threads_ok` before the
     existing error strings.
5. `rt_*` is a temporary home. When the `runtime/core/src/config.cpp` slice
   lands, those two predicates move to that module; the error strings stay in
   the native validator, so no caller contract changes.

## Compatibility checks

- `goose check source/runtime.goose`: parsed and typechecked, 83
  specializations.
- Gate: 67/67 at `-O1` against the base tree; `tests/gate.txt` is the recorded
  run.
- No duplicate exports: `mb_`, `rg_`, `th_` and `rt_` prefixes do not occur
  anywhere under `goose/` at the base commit.
- The generated ABI is plain C11 (`sl_u8`, `sl_i64`), so the adapter can call
  it from the existing C++ without a shim.

## Limits

- No GPU and no driver work: meta `graph_compute`, split-state callback,
  buffer allocation and tensor data movement, backend init, events and comm are
  out of this slice.
- `ggml-opt.cpp` (optimizer/epoch policy) is not covered; it is a separate
  slice of P4-12.
- OS affinity syscalls and thread creation/lifetime stay in the native
  boundary; only the pure placement policy is ported.
- The gate cannot prove the intended `Meta(...)` join because the base code
  never executes it; that is recorded, not hidden.

## Hashes (SHA-256)

Inputs, base `df3ce73a`:

```
890f527a231068c1a3f6cad4515dbeba36d4d32889f12f20fc35af7bf97c3216  upstream/llama.cpp/ggml/src/ggml-backend-meta.cpp
ecc4c52d8347f0d9ea2aab4c54bafafb3ead573c8badec6042fd65dffb19d20d  upstream/llama.cpp/ggml/src/ggml.c
b924bc58d82442718f62300b1fc5ee13d04d6c7f6fe77a82529f0867d3ec52e3  upstream/llama.cpp/ggml/src/ggml-cpu/ggml-cpu.c
731d7f81ec350ddaeed3d6cc032e86a144c2f89b8a8e3519bfd358698df7765d  runtime/core/src/config.cpp
f6a05f3f2ea84d95dce33c19956320c4ea6df338b0df88771f5cda06b782a83d  runtime/core/include/bmoe/config.h
8cce13c85f19330096002140c401601bcf050fe7c16c255ef83e33d0eb7ad3dc  libggml-base.so (oracle library)
```

Artifacts:

```
81a2e06bfc8d700e111a2f4177d37cbcc4fdc61d23672ade04eafae2170f2ade  source/runtime.goose
51ac8411352a4b23e3c05a83d41fab45eecd821b580197cec0f0d322ec28edd7  tests/fixture.cpp
068dc121b07c3eef67004f7063be35a6c54b399f74d13cab9a30ded880d543f8  tests/build.sh
b2c86b692e31705d21e6db07900b48030ba192ffd6133a50c83f342587b8cd8e  tests/mutate.py
b5093032aedcc7771964f852f06a903fc1aaa6fbd86f7c15a345e6b22d297e68  tests/gate.txt
2a1f3dd541a9f83ea481c8e2bdb8912b3f12af438b973749127e619748e9dde6  tests/mutations.txt
```

Toolchain: goose runtime `1e714e6146bc`, gcc/g++ 13.4.0. Generated port C:
183063 bytes, not committed.
