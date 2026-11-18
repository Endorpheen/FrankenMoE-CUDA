# EXP-2026-11-17-910: exact Goose PLE FP8 row decoding

Status: ACCEPTED as a standalone row decoder, not server-integrated.
Base: 1574d93451bb25ac9904b8b296ff6ff53ed1cbb3.

Hypothesis fixed before implementation: pure Goose can decode the pinned pack's
160-byte FP8 rows bit-for-bit, preserving the developer engine's signed-zero
handling of 0x7F/0xFF and applying the actual GGUF table scale. This closes a
numerical building block, not the I/O provider or model graph. No throughput
gain or model correctness claim is made.

Contract source: Edge0-AI/Edge0 commit
f6bfa5546bcf89919f86946ec669bcc7f4cd526d,
`edge0-strata/src/kernels/ngram.cpp` Fp8Table/fp8_e4m3_dequant_row.
Local GGUF metadata: strata-ple, f8_e4m3, scale 0.00019931793212890625,
one I8-storage tensor [160, 320001536], payload offset 352. NaN encodings
becoming signed zero is engine compatibility, not the general IEEE meaning
of E4M3fn NaNs. Other scale values are rejected explicitly by this pinned-pack
API rather than silently using an assumed scale.

Acceptance: exact FP32 bits against an independent mathematical oracle for
all 256 input codes, signed zeros, subnormals, finite extremes and special
codes; deterministic synthetic and bounded real-row samples; wrong lengths
and scale/NaN rejected before writes; caller buffers retained by the caller;
independent concurrent outputs; mutations of exponent/sign/scale behavior
rejected. Real reads stay bounded to the selected rows, no full-table map/load.

Use the experiment template's cheap check first, then a small -O1 generated-C
ABI gate in a fresh LlamaServer directory. Mutations compile at -O0. No full
runtime rebuild, server/model launch, C++ deletion or Apex binary change.

## Results

Goose check, -O1 local C generation/ABI build, numerical gate and six -O0
mutation gates all passed. The independent oracle uses binary decomposition
and Python ldexp followed by FP32 rounding; it does not call the source engine.
Since the decoded values and table scale are exactly representable dyadics,
the double-precision oracle product is exact before its final FP32 rounding.

The gate passed 89 row/input cases, covering all 256 FP8 codes, 44 synthetic
rows, 32 real rows, wrong input/output lengths and invalid/NaN/Inf scales.
Every valid output matched FP32 bytes exactly, including signs of zero and
the engine's special-code handling. Borrowed input bytes and output guard words
were unchanged. A further 4096 concurrent calls matched independent outputs.
Each real row was read with a bounded 160-byte pread: **5120 payload bytes**
total, plus the 352-byte metadata header. The table was not mapped or loaded.
The header's actual scale, encoding, dimensions, file size and payload offset
were checked before any row read.

Mutation outcomes: incorrect exponent bias, sign handling, subnormal scale,
special-code branch, table scale and scale-admission guard were each rejected
by a semantic assertion. No compiler error/crash counted as a killed mutation.
The existing successful receipts were reused; no model/GPU test was run.

Artifacts: `/home/end0/AI/LlamaServer/exp910-edge0-ple-fp8-20261117`, four
step logs with commands/outputs/return codes, generated C/header and isolated
36144-byte module. Library SHA256:
`61aee6ac560cbaa5fac4d671ebe25b094abd90d0cfe00b460d1269469319e1c8`.
Source SHA256:
`65eb68c5dbaaa321aa88f5d4ed156241f293f7b166246d9dbe149ee3632c0e9b`.
All receipt hashes are recorded in `evidence.json`.

The three redundant-cast warnings are retained. Export bodies do not allocate,
retain pointers or call host FFI; the compiler runtime still initializes itself.
This is not an I/O cache, tokenizer state implementation, PLE graph, latent MoE
or proof of Edge0 inference. There is no production C++ change/removal and no
runtime performance or peak-memory claim. Apex EXP906 remains unchanged.

## Next dependency

Compose the accepted descriptor and decoder behind a bounded row-provider API
with explicit caller-owned buffers, exact short-read/error handling and file
lifetime. Reuse an existing Goose-callable OS binding; avoid new C++ policy.
That provider then needs PLE hash/conv state and graph integration, alongside
the separate latent-expert/D-U path, before attempting model generation.

Selective reproduction follows EXP909's four `tests/build.py --step` modes;
use a new LlamaServer directory if an input changes. This module remains an
experimental dependency and is not counted as canonical server coverage.
