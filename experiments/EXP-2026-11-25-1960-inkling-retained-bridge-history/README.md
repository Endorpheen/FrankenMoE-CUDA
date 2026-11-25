# EXP-2026-11-25-1960: Retain Inkling DepthMix history between decode visits

**Verdict: exact history reuse, inconclusive as a standalone speedup.**
Keep the opt-in mechanism for work on a resident decode graph.

The EXP-1959 bridge accepts the current residual and the full normalized
depth history on every visit. For a continuing visit the existing history
is valid and only the residual is needed. At a history boundary, the
bridge omits the residual add and needs only the appended history row.
A loop reset can replace the suffix after its prelude. The driver tracks
token position, logical execution ordinal and valid prefix length. It
does not use the cache slot ID for traversal order. The first validation
attempt caught that distinction when the graph jumped to slot 24.

The 48 visits require 168 uploaded rows in the original bridge and 48
in the retained path. At 9216 bytes per row that is 1548288 → 442368
bytes per decode step (-71.4%) for the bridge input alone, excluding
per-request setup. This is a byte count derived from the executed branch
structure and visit schedule. It does not include expert transfers or
estimate throughput as if transfers were fully exposed.

The corrected shadow check completed two 128/16 requests on one server.
Before each bridge execution it compared all active rows of the CUDA
buffer against CPU memory. All 1440 visits matched, as did both responses
and 318 logged state fingerprints against EXP-1959. The timing block
disabled this diagnostic readback.

## Inkling 8192/256

One fresh server and one request per arm, A–B–B–A. Same binary and
EXP-1959 settings, including the joined bridge, context 36864, logical
batch and ubatch 4096, Q4_0 K/V and 6720 MiB expert cache.

| Arm | Prefill, tok/s | Decode, tok/s | Decode, ms | Client, s |
| --- | ---: | ---: | ---: | ---: |
| A1 | 279.038 | 20.241 | 12598.468 | 42.265 |
| B1 | 278.763 | 20.414 | 12491.711 | 42.172 |
| B2 | 278.932 | 20.379 | 12512.693 | 42.175 |
| A2 | 279.030 | 20.378 | 12513.576 | 42.168 |

Averages are 279.034 → 278.848 prefill (-0.07%) and 20.309 → 20.396
decode (+0.43%). Decode saved 106.8 ms in the first pair and 0.9 ms
in the reversed pair. Client time improved 93.5 ms in the first
pair and lost 7.3 ms in the second. Do not promote this as a
throughput improvement.

All 256 IDs, text, 8703 state fingerprints and final `/props` matched.
The peak board maximum is 11735 MiB for each arm and process peak RSS
does not grow. There was zero process swap and system swap-out. The
modification is inside the opt-in Inkling joined bridge and there was
no new Apex test. The operational builds were preserved.

## Reproduction

Build from the repository with `bash source/build.sh /absolute/new/scratch`.
The compiler comes from EXP-1959. Set `ASSERT_SOURCE_DIR` to the
absolute original `build/inkling-apex-gate/bridge-history-1960`
directory to reproduce the exact binary including GCC assertion paths.
Launch with the argv and env in `results.json`.
Use `I2_BRIDGE_RETAIN_HISTORY=0` for control and `1` for the candidate.
Set `I2_BRIDGE_HISTORY_CHECK=1` only for the numerical shadow check.
Raw evidence is in `build/inkling-apex-gate/bridge-history-1960`.
