# EXP-2026-11-20-1559 — EXP-1476 CUDA compute arena

The independent Goose allocator's 64-to-384 MiB activation-arena
policy addresses host publications from an initially small device
arena. In frozen EXP-1476 revision
`e7c9155a01938b1825189240e7088620c6c7cd85`,
`llama-context.cpp::sched_reserve` uses the GGML graph allocator.
`ggml-alloc.c::ggml_gallocr_reserve_n_impl` reuses its current
device buffers while their chunks fit, then reallocates to the
requested graph size. Lazy prefill reservation sets GPU growth
limits to zero and leaves the GPU allocation exact. The release
has no threshold that widens a 64 MiB arena to 384 MiB.

The isolated experiment compiles `arena-overlay.c` using the
frozen tag's GGML headers. Both arms load this same adapter
alongside the same release files. Under
`ggml_gallocr_reserve_n`, it traces CUDA0 compute allocations.
Arm B rounds the first eligible CUDA compute-buffer request above
64 MiB and below 384 MiB up to 384 MiB. The real GGML allocator
retains ownership and still performs its existing lifetime
assignment. The experiment changes no weights, operations,
runtime libraries or sampler settings.

## Paired Apex 32K/512

RTX 4070, context 262,144, batch and ubatch 4096/4096,
same frozen Q4_K_M weights and canonical request. There is one
profiled run per arm. The transfer interval is from the first
to the last recorded CUDA kernel, including warm-up.

| Metric | A, native allocation policy | B, 384 MiB threshold |
|---|---:|---:|
| Prefill, tok/s | 2,618.408 | 2,634.004 |
| Reported decode, tok/s | 56.733 | 57.067 |
| H2D, bytes | 136,901,438,100 | 136,901,438,100 |
| D2H, bytes | 1,950,097,760 | 1,950,097,760 |
| Process RSS high-water, KiB | 22,517,948 | 22,516,436 |
| Sampled peak process VRAM, MiB | 8,214 | 8,214 |
| Sampled peak whole-card VRAM, MiB | 10,446 | 10,440 |
| Process swap and system swap-out growth | 0 | 0 |

Both traces show one initial CUDA0 compute allocation of
5,316,864 bytes, followed by eight allocations from
1,178,632,320 through 1,412,563,072 bytes. The initial allocation
is below the widening trigger and each prefill allocation already
exceeds 384 MiB. The adapter changes zero allocation sizes.
The prefill and decode variation cannot be attributed to the
threshold. Retain the original EXP-1476 allocation policy.

The responses match in all 512 token IDs, content and probability
rows and give the reference capacity-risk assessment. Content
SHA-256 is
`c2ff42cc1219ea4dfe6d69f02e008d0f90c0fc56260736a0aafb82cec362924f`.
The entire release SHA-256 manifest passes both before and after
the pair. The independent Goose activation arena is separate.

## Reproduction

Use the release and request paths in `run.py`. Compile the
overlay with GCC 13 and the three headers from
`v0.10.0-exp1476:vendor/flint/ggml/include`:

```bash
gcc-13 -O2 -std=c11 -Wall -Wextra -fPIC -shared -I include arena-overlay.c -ldl -pthread -o arena-overlay.so
python3 run.py a "$PWD"
python3 run.py b "$PWD"
```

Raw artifacts are under
`/home/end0/AI/LlamaServer/franken-exp1476-arena-ab.htNK9R`.
Overlay SHA-256 is
`03cf0d5e70e5d365e6b000b8d3a45f93bb0157edb8d9f1518277df22bed3d358`.
The A and B Nsight SHA-256 values are
`fcae2a50db81b126e188014ffa1f3f3a1c3f0e77ec486db0ced483b918752649`
and
`4c680e9d725809f64108eee89f3bf9fd1f4aba352259b924fa27f558c4cfb90f`.
