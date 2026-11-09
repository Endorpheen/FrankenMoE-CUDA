# EXP183: CUDA FWHT in Goose

Hypothesis: implement the warp-path FWHT kernel (N = 64/128/256/512) in actual Goose
and enable it by default. It is on the executed Apex path, so unlike EXP182 this is
measurable. Verdict: **ACCEPTED**, no regression beyond the 2% threshold.

## Scope transferred

`fwht_cuda<N, T>` for F32 sources and N in {64,128,256,512}, reached through
`ggml_cuda_op_fwht`. Goose owns the row decode, the scaled register load, the
warp butterfly (`shfl_xor` plus the `(lane & h) == 0` add/subtract rule), the
in-register butterflies above the warp width, and the store.

The wide path `fwht_cuda_block` (N >= 1024, shared-memory staging) and half sources
fall back to native inside the candidate, as does any other size.

## Apex pair, same DSO, only FWHT switched

Order native → goose → goose → native, one component changed:

| arm | prefill tok/s | decode tok/s |
|---|---:|---:|
| native-rollback f | 2594.76 | 26.64 |
| goose f | 2563.64 | **27.53** |
| goose g | 2575.05 | 26.40 |
| native-rollback g | 2603.63 | **27.83** |
| native median | 2599.19 | 27.24 |
| goose median | 2569.35 | 26.97 |
| delta | **-1.15%** | **-0.99%** |

Both figures inside the 2% threshold. Content sha256 identical in all four runs
(`e68df05b`), and the model witness `FG183F_CUDA_FWHT ... paths=0x1` appears only in
the Goose arms, so the two arms really do differ.

## Local gate

| check | result |
|---|---|
| `gpu-fixture` Goose | 15 checks / 0 failures / 5 cases |
| `gpu-fixture` native rollback | 15 / 0 / 5 |
| byte-for-byte output | 5 of 5 (n64 r1/r17, n128 r4, n256 r9, n512 r33) |
| compute-sanitizer memcheck | 0 errors |
| export surface vs EXP182 | exactly the two diagnostics |

Byte equality is the right criterion here: the transform is a fixed sequence of adds
and subtractions scaled by `1/sqrt(N)`, so a float tolerance would hide a wrong
butterfly stage. Row counts deliberately include values that are not a multiple of
the 4 rows per block.

## Kernel microbenchmark

Because the first pair looked like a decode regression, the kernel was measured in
isolation before any conclusion was drawn:

| rows | N | goose us | native us |
|---:|---:|---:|---:|
| 128 | 64 | 2.848 | 2.752 |
| 512 | 64 | 3.008 | 2.944 |
| 128 | 256 | 3.040 | 2.944 |
| 512 | 256 | 3.168 | 2.912 |

3 to 9% slower per launch, at roughly 3 microseconds against a 39 ms per-token
budget. That cannot account for a 5% decode difference, and the corrected pair below
shows there was no such difference.

## Two measurement defects in my own work

1. **The first pair was invalid.** The inherited runner had
   `if mode=='native-rollback': env['GGML_GOOSE_CUDA_SET_ROWS']='0'` from EXP174, so
   the native arm switched off *both* SET_ROWS and FWHT while the Goose arm switched
   off neither. The arms differed by two components, and the apparent -5.67% decode
   regression was the SET_ROWS removal, not FWHT. Fixed so that only FWHT is
   switched, and the whole protocol was re-run from scratch.
2. **The runner crashed in its own cleanup.** A missing `;` concatenated two
   `write_text` calls together, so `finally` raised `TypeError: 'int' object is not
   callable`, the server was never stopped, and the atexit witness line was never
   emitted. This is why early Goose runs showed no `FG183F` line at all. Fixed; the
   witness now appears for every Goose arm.

Neither defect would have been visible without checking the witness and the env
record per run rather than trusting the label.

## Rollback and reproduce

`GGML_GOOSE_CUDA_FWHT=0` restores the private-renamed native
`ggml_cuda_op_fwht`; any size or type the Goose body does not cover also falls back
inside the candidate. Reproduce with `python3 emit_sources.py`,
`python3 build_runtime.py`, `python3 test_local.py`.

## Remaining scope

`fwht_cuda_block` for N >= 1024 (shared staging across warps), half and bfloat16
sources, and the rest of the per-op set stay native.
