# EXP175: actual Apex CUDA CONCAT in Goose

Hypothesis: replace the CUDA `concat` device arithmetic and its host planning
with actual Goose, keeping the previous EXP174 modules untouched and the native
implementation reachable through one component switch. No production or
compiler-source changes. Current state: the whole active CUDA CONCAT operator is
Goose on the fixed Apex profile and is preserved as a separate checkpoint, not
activated as the working runtime. P3-03 overall remains incomplete.

## Verdict

**ACCEPTED** — paired Apex +0.573% prefill / +0.437% decode vs EXP174.

This is a verdict on the transfer in the experimental runtime, which is the only thing
this work decides. Installation into the production working runtime is left to Igor and
Codex, which is why the state line below says "not promoted" for several stages: it means
"not installed", never "not accepted".

## Scope transferred

`ggml-cuda/concat.cu` has one public entry, `ggml_cuda_op_concat`, and two
compute kernels plus one transport branch:

| Native | Goose |
|---|---|
| `concat_cont<T,dim>` for `dim 0..2` | `fg175c_cont` with compile-time `dim` wrappers |
| `concat_non_cont<T,dim>` for `dim 0..3` | `fg175c_nc` with compile-time `dim` wrappers |
| host type/alignment/contiguity asserts and width selection | `host.goose` `check_src`, `width_of`, `row_units` |
| per-`i3` slice pointer advance and grid/block sizing | `host.goose` `plan_cont`, `plan_nc` |
| `dim == 3` contiguous two-`cudaMemcpyAsync` copy | requested through the launch adapter as route 2 |

Goose owns the block/thread mapping, the index decode, the src0-inside test, the
dim-dependent source address selection, the destination address sum and the
element copy. The C++ bridge keeps only raw scalar loads/stores of the copy
width, `__umulhi`-free integer arithmetic, the plan arena, tensor-field reads and
the launch call. No original kernel, GGML struct arithmetic or C++ routine is
reachable from an enabled path.

The copy width follows the native entry exactly: 1 byte for a quantized type
(the tensors are viewed as bytes and `ne[0]` must be block-aligned), otherwise
`ggml_type_size` restricted to 1/2/4/8.

## Producer

Pinned selfhosted Stage3 `stage3-871aec9cc92d`, SHA256
`871aec9cc92db1b5b66c8e18b2eb591236f5b47f072a74713b42d78a3a73a48a`, from
`runtime/compiler/current.json`. Compiler sources were read only; nothing under
`/home/end0/AI/Fried-Goose` was modified.

Emission: `emit_sources.py` compiles `host.goose` and `device.goose` with the
pinned producer, mechanically extracts the emitted C bodies and rejects any
reachable Goose stack allocator. `emit_bridge.py` maps the emitted function
symbols to the seven exported wrappers (3 contiguous, 4 non-contiguous) from the
recorded entry order, because the C backend numbers functions by emission and
does not keep source names.

## Verified result

Local gates, each run in both arms of the same candidate DSO:

| Fixture | Goose | Native rollback |
|---|---|---|
| route/branch matrix, 8 shapes | 32 checks / 0 failures | 32 / 0 |
| element-width matrix, 7 typed cases | 28 / 0 | 28 / 0 |
| quantized byte view, 4 cases | 16 / 0 | 16 / 0 |

Quantized destinations were additionally dumped and compared byte-for-byte:
all 4 cases identical between the Goose route and the native op.

Two defects were caught and fixed before the model gate, and both are retained
in the record: a tensor/packet type confusion in the host accessor (fixed by
dereferencing the tensor address directly), and an overlap between the pointer
slots and the memcpy size slots in the plan arena (fixed by moving the sizes to
their own slot row). A third finding was in the fixture, not the candidate: a
`dim=2` quantized case declared a `src1` plane count that made `dst` planes
inconsistent, and both arms failed it identically until the case was corrected.
That case is now strict again; no reference check was weakened.

One fixed Apex 32768/128 pair toggles only CONCAT inside the same DSO:

| Arm | Prefill tok/s | Decode tok/s |
|---|---:|---:|
| Native CONCAT (`GGML_GOOSE_CUDA_CONCAT=0`) | 2198.147 | 18.989 |
| Goose CONCAT (default) | 2172.249 | 19.190 |
| Delta | -1.178% | +1.058% |

Both arms return exactly the saved original C++ 128 token IDs, response text and
all returned top-5 probabilities (content sha256 `e68df05b`). Peak RSS
21311.6 MiB, process and system swap 0, VRAM 3278 MiB. The Goose witness
`FG175C_CUDA_CONCAT host=0x100,cuda_status=0,paths=0x11,guards=0` shows all 8
route/dimension entries executed; the native arm shows no Goose CONCAT activity.
All 29 pre-existing `FG*` witnesses are identical between the arms, and
`FG175C_CUDA_CONCAT` is the only added one, so no previously qualified module
changed.

These are preliminary single-pair component observations under uncontrolled
external load, not stable whole-runtime C++ parity. The small opposite-direction
deltas justify no further series.

## Rollback

`GGML_GOOSE_CUDA_CONCAT=0` restores the private-renamed native
`ggml_cuda_op_concat` inside the same DSO. The immutable EXP174 runtime remains
the previous checkpoint. Production, the active runtime and the compiler are
unchanged.

## Reproduce

From this directory run `python3 emit_sources.py`, `python3 emit_bridge.py`,
`python3 build_runtime.py`, `python3 test_local.py`, then, with approval,
`FG175_MEASURE_MODE=goose python3 run_apex_pair.py` and the same command with
`FG175_MEASURE_MODE=native-rollback`. The build replays an immutable output or
keeps historical binaries; only the dedicated EXP175 run links move. Existing
Apex receipts cannot be overwritten.

## Remaining P3-03 scope

Not transferred and still native: CUDA `COPY`/`DUP`/`CONT` (`cpy.cu` scalar and
typed paths), `GET_ROWS`/`GET_ROWS_BACK` (`getrows.cu`, including the quantized
`kq` dequantizing path), `SET` (`set.cu`), `PAD`/`PAD_REFLECT_1D`,
`ROLL`, and the `dim == 3` quantized and `!contiguous` branches. The fixed Apex
profile executes `concat_non_cont<uint32_t,dim=0>` (the largest P3-03 item in the
GPU census at 4.5% of kernel time) and `cpy_scalar<float,float>`, so CONCAT is
the material one, but the task is not closed.
