# EXP-2026-11-25-2016 — Keep CUDA driver loading optional at startup

**Functional gate passed for the Linux experimental Bonsai build.**
No full Apex or Inkling acceptance is claimed. EXP-2014 linked the
Bonsai CUDA Driver API as an unconditional ELF DT_NEEDED dependency.
The frozen EXP-1969 executable instead opens the CUDA driver when
selected. Restore that startup behavior for the combined binary.

The Bonsai C adapter now resolves its 17 driver entry points with
`dlopen` and `dlsym` on first model open. It preserves CUDA header
versioned symbol names, scopes the library with RTLD_LOCAL, and
publishes the table only after all bindings succeed. A process-local
pthread-once keeps initialization consistent. Missing libraries or
entry points return a clear load error. No common model-host object,
Goose model graph, shared driver code, CUDA image, or production
launcher changes. The executable reuses the same archive and cubin.

## Evidence

- ELF dependencies in the new and frozen binaries have the same set
  (libm, libc and the dynamic loader). There are no undefined
  CUDA symbols in the new executable.
- With a deliberately incomplete `libcuda.so.1` and immediate
  relocations, the frozen executable and the new one enter the CLI
  and exit 2 with matching usage. EXP-2014 exits 127 before the CLI
  with an unresolved CUDA entry point. Attempted Bonsai open reports
  `Bonsai CUDA driver missing cuInit` and returns failure before a
  listener or GPU allocation.
- One 4097/64 comparison with the real driver and identical images,
  Q4_0 K/V, context 36864 and batch/ubatch 4096/1024 matches every
  generated ID and text byte. Peak VRAM is 8932 MiB in both arms.
  Peak RSS is 7,456,972 → 7,456,600 KiB with zero swap-out. The
  one-pair observations are 817.40 → 806.01 prefill and 35.11 →
  36.11 decode tok/s. This block is a functional gate only.
- The EXP-1970 warm Apex regression block was attempted first with
  the byte-identical frozen EXP-1969 control at context 262144,
  Q4_0 K/V and expert cache 6290 MiB. The control reported only
  6,228,172,800 B (~5940 MiB) available after model allocations,
  roughly 350 MiB short. The board started at 2520 MiB used.
  The server exited before the first request. No settings were
  relaxed and no unrelated process was stopped. See the saved
  [startup log](data/apex-control-startup.txt).

## Reproduction

```bash
ex=experiments/EXP-2026-11-25-2016-bonsai-deferred-cuda
out=build/bonsai-lazy-cuda-repro
bash "$ex/source/build.sh" "$out"
readelf -d "$out/frankenmoe-infer" | rg NEEDED
mkdir "$out/stub"
printf '%s\n' 'int incomplete_driver(void) { return 0; }' | \
    gcc-13 -shared -fPIC -x c - -o "$out/stub/libcuda.so.1"
LD_LIBRARY_PATH="$(realpath "$out/stub")" LD_BIND_NOW=1 "$out/frankenmoe-infer"
```

The last command prints usage and returns 2, matching the frozen
binary. Use the EXP-2014 normal launch options without the stub
for the real-GPU test. Use this experiment's executable, its
`pq2-int8.cubin` link and the same frozen PTX. Submit the first
4097 input IDs and generate 64 tokens as in EXP-2012.
[Measured responses and memory](data/results.json).
[Artifact hashes](data/SHA256SUMS).
