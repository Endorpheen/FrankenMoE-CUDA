# EXP177: actual Apex CUDA CPY in Goose

Hypothesis: replace the CUDA `cpy` element path and its host branch selection with
actual Goose for the profile Apex runs, keeping every earlier module and the
native implementation behind one switch. No production or compiler-source
changes. Current state: qualified on the fixed Apex profile and preserved as a
checkpoint, not promoted to the working runtime. P3-03 remains open.

## Scope transferred

`ggml_cuda_cpy` is the single entry behind `GGML_OP_DUP`, `GGML_OP_CPY` and
`GGML_OP_CONT`; `ggml_cuda_dup` wraps it. Both native symbols are private-renamed,
so every route reaches one switch.

| Native | Goose |
|---|---|
| `ggml_cuda_cpy` branch order: same-type-contiguous memcpy, 2D memcpy, typed paths | `run()` in `host.goose` |
| `ggml_cuda_cpy_as_memcpy_2d` (prefix walk, rows dim, upper-dims-1, pitch tests) | `memcpy2d()` |
| `can_be_transposed` and the scalar/transposed choice | `can_tr` / `route` |
| `cpy_scalar<cpy_1_scalar<float,float>>` | `fg177c_scalar` |
| `cpy_scalar_contiguous<float,float>` | `fg177c_contiguous` |
| grid/block sizing | `run()` |

The scalar body decodes the flattened index against both operand extent sets and
sums both byte-offset arms exactly as native does. Extern symbols are only raw
`float` loads/stores and the PDL primitives.

Route coverage observed on the model: `host=0x7`, i.e. route 0 (same-type
contiguous device memcpy), route 1 (2D memcpy) and route 2 (Goose scalar walk)
all executed. Route 3, the contiguous element kernel, is unreachable for
same-type copies — native sends same-type-contiguous to memcpy and everything else
to the scalar walk — and it serves only different-type copies, which are outside
this stage's F32→F32 scope. That is stated rather than claimed as covered.

## Defects in my own work, found and fixed

1. Operand source — I read the two tensors from `dst->src[]`, but `ggml_cuda_cpy`
   receives them as arguments. The packet now carries `arg(0)=src1` and
   `arg(1)=src0` and the host reads those.
2. Packet query — `ti(s0, 9, 0)` passed a tensor address to a packet-level query,
   so the memcpy byte count came from the wrong memory and route 0 copied nothing.
   It now passes `packet`.
3. Route return — the 2D branch asserted `launch(packet)==2` against a route
   number, but the adapter returns the CUDA status. It now checks `==0`.
4. `ggml_cuda_dup` — privatising only `ggml_cuda_cpy` left DUP resolving to the
   native function inside the same object. Both symbols are renamed now.

## Local gate

| check | result |
|---|---|
| `gpu-fixture` Goose | 18 checks / 0 failures / 6 cases |
| `gpu-fixture` native rollback | 18 / 0 / 6 |
| byte-for-byte Goose vs native destinations | 6 of 6 identical |
| compute-sanitizer memcheck | `ERROR SUMMARY: 0 errors` |
| compute-sanitizer racecheck | `0 hazards`, 0 errors, 0 warnings |
| export surface vs EXP176 | exactly the two diagnostics added |

Cases cover fully contiguous (route 0), source-padded and destination-padded
(2D memcpy and scalar), both-padded, and a larger strided multi-plane case.

## Apex result

Two opposite-order pairs, only CPY toggled inside one DSO:

| Pair | Native pp/tg | Goose pp/tg | Δ prefill | Δ decode |
|---|---|---|---|---|
| forward | 2624.59 / 26.160 | 2599.55 / 26.675 | -0.954% | +1.968% |
| reverse | (same) | 2597.52 / 27.140 | -1.032% | +3.746% |
| mean | 2624.59 / 26.160 | 2598.54 / 26.908 | **-0.993%** | **+2.856%** |

All three runs return exactly the saved original C++ 128 token IDs, response text
and every returned top-5 probability (sha256 `e68df05b`). Both deltas are inside
the 2% prefill tolerance, and decode moves in Goose's favour.

## Rollback and reproduce

`GGML_GOOSE_CUDA_CPY=0` restores the private-renamed native `ggml_cuda_cpy` and
`ggml_cuda_dup`. The immutable EXP176 runtime remains the previous checkpoint.
Reproduce with `python3 emit_sources.py`, `python3 build_runtime.py`,
`python3 test_local.py`, then `FG_STAGE=exp177 FG174_MEASURE_MODE=goose` and
`native-rollback` for `run_apex_pair.py`. `FG177C_DUMP=<dir>` writes raw
destinations for an external comparison.

## Remaining P3-03 scope

Still native: `GGML_OP_SET` (`set.cu`), `PAD`, `PAD_REFLECT_1D`, `ROLL`, the
transposed CPY tile kernel, quantized and different-type copy pairs, and the
quantized `GET_ROWS` dequantizing paths with `GET_ROWS_BACK`. Those depend on the
dequantization and cast modules and remain under P3-03/P3-07.
