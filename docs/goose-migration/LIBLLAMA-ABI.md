# Why the reference libllama cannot be used

Base: `e60df3b9`. Diagnostic evidence, no speed claim.

## The observation that started this

The accepted CPU library we load is byte-identical to the one the reference used:

```
ours    runtime/goose-stage/lib/libggml-cpu.so.0     63ad49848812
ref     exp160 manifest, cpu entry                    63ad49848812
```

Same file, same hash, same 8 threads, same `--cpu-mask 0f0f`, same 18 `GGML_GOOSE_*` values,
39 threads in the process. Yet the reference log shows `FG148_QUANT calls=482400` and ours shows
none. Identical code, different counters, so the difference has to be in what calls into it.

## The remaining difference

| library | ours | reference |
|---|---|---|
| `libllama` | `abfe9400488e` (stage-run) | `2bc628a170a5` (libllama-exp244) |
| `fg*` symbols | 327 | 214 |
| module names | 240 distinct | **the same 240** |

The module *sets* are identical - 0 modules only in ours, 0 only in theirs. The count difference
is symbol cardinality (327 vs 214 bindings over the same module names), not different modules.

## Why it cannot simply be swapped in

Loading `libllama-exp244` in place of ours, with every other library already at its reference
build, fails:

```
ggml_backend_cuda_buffer_type_alloc_buffer: allocating 1124.03 MiB:
    cudaMalloc failed: out of memory
process_ubatch: failed to allocate graph
```

The GPU had 8.5 GiB free at that moment, and 1124 MiB is a normal graph allocation - so this is
not memory pressure. `libllama-exp244` computes a different buffer geometry than the ggml build
in stage-run expects: it was linked against a different ggml revision, and the two disagree about
how much to allocate for the same graph. The allocation it asks for does not match what the
scheduler reserved, and `cudaMalloc` fails on the resulting fragmentation.

That is an ABI mismatch between a library and the ggml it was built for, and it cannot be
resolved by ordering or flags: the two sides must be built together.

## What this means

The reference result (41.01-41.94 decode) was produced by a `libllama` + ggml pair built as one
unit. We have the ggml side and we have that `libllama`, but the pair does not link up: the
`libllama` needs the ggml revision it was compiled against, and stage-run has a different one.

So restoring the reference decode is not a matter of copying a file. It requires either
rebuilding `libllama` against stage-run's ggml, or bringing the matching ggml revision - and the
first is the same work as completing the Goose migration of the llama-side routes, which is the
goal anyway.

## What is still unexplained, stated plainly

That the same `libggml-cpu` produces 482400 FG148 calls in one process and zero in another is
not explained by "different libllama" alone. It is consistent with `libllama` deciding which
kernel to invoke - if it never selects the Goose quantised path, `fg148_row_*` is never called
and the counter stays at zero. But this is inference from two data points, not a measurement, and
it is recorded as such.
