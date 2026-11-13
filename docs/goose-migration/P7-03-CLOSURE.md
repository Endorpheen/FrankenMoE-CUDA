# P7-03 closure record: model loader, mmap and file I/O ownership

Criterion: "All load modes, multi-shard, lazy/mmap/mlock, overrides, tensor validation, progress/cancel and error cleanup
implemented; Apex load witnesses."

## Clause by clause

| clause | implementation | evidence |
|---|---|---|
| all load modes | `fg201l_use_mmap_mode` / `fg201l_use_direct_io_mode` decide from `load_mode` when the loader is constructed (reconstructed `llama-model-loader.cpp:586-589`) | live in the accepted run: `FG201L_MODEL_LOADER` twice, `FG201L_KEYS`, `FG201L_RANGES` |
| multi-shard | EXP-284 module (postfix construction and suffix match, padding width derived from the digit count) + EXP-285 wiring in `llama.cpp`, default on, native kept as fallback | module gate 9/0, including six-digit index and count; the wired object adds exactly `fg284p_enabled` and `fg284p_split_prefix` to its undefined set. **Dormant**: this profile loads a single file, so the rule is implemented but not called |
| lazy / mmap | mmap choice as above; the per-tensor lock gate `has_locks && !is_lazy` and the per-mapping used-range min/max are EXP-286 | module gate 11/0 (four lock cases, seven range cases incl. the `INT64_MAX` empty sentinel); the wired loader object adds exactly `fg286m_enabled`, `fg286m_should_lock`, `fg286m_range_update` |
| mlock | same predicate; `llama_mlock::grow_to` stays an OS primitive | as above. `--mlock` is not in the accepted argv, so this path is dormant here by configuration |
| overrides | `fg404s` weight placement decides the per-tensor target | live: `FG404S_WEIGHT_PLACEMENT` fifteen times in the accepted run |
| tensor validation | `fg201l_shape_check` (shape and bounds), `fg201l_or_arr_too_many` / `fg201l_arr_length_ok` (array bounds), `fg403s` alignment decision (wired at `:722-724`), `fg389s` inventory (duplicate names, dominant type) | `FG389S_GGUF_INVENTORY` live; the shape and bounds helpers are called on every metadata read. Honest note: `fg403s` is decide-only and prints no witness, so its execution is argued from the call site, not from a log line |
| progress/cancel | `llama_progress_callback` returns whether to continue; `load_all_data` stops and reports | mechanism, not a decision: the policy belongs to the caller. Exercised whenever a caller passes a callback |
| error cleanup | RAII throughout (`ggml_context_ptr`, `unique_ptr`, buffer holders, `llama_mlock` in `llama_mlocks`) | no decision to port; a failure unwinds through destructors |

## What remains native and why that is correct

`mmap`/`munmap`, `mlock`/`grow_to`, `pread`, `fstat` and `snprintf` are OS or libc primitives. The decisions about them
are the three modules above plus fg201l, and those are gated.

## Why multi-shard and mlock are dormant rather than untested

The accepted profile loads one file with `--load-mode mmap` and no `--mlock`. Both rules are compiled into the
integration objects with the native fallback behind a default-on switch, so a split model or an `--mlock` run would
exercise them without a code change; neither exists in the agreed profile, and that is a configuration fact, not a gap.
