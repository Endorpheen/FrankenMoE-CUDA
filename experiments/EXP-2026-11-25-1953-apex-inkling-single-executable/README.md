# EXP-2026-11-25-1953: Link Apex and Inkling in one Goose executable

**Verdict: both end-to-end diagnostic gates pass.** The production
acceptance gates remain open. One byte-identical Linux executable served
both GGUFs in separate processes with the same CLI and HTTP layer.

## Integration

- Keep the EXP-1880 Goose main unit, cached Apex decode entry points,
  graph bindings, dense kernels, routing mailbox and prefetch images.
- Select the model backend using `general.architecture` during the common
  GGUF load. The Inkling Goose model unit exposes only its model functions,
  initializer and its two-plane SLRU prepare entry. The host initializes
  that unit in the Inkling branch and uses the Inkling tokenizer identified
  by GGUF metadata. Apex retains its cached decode scope.
- Accept Inkling's named attention and MoE custom nodes in the common
  graph executor. Retain the Apex graph bind and update operations.
- Add the Inkling Q8_0/IQ4_NL/IQ4_XS layout and the two indexed expert
  formats to the common Goose dispatcher, behind the Inkling mode.
  Keep the accepted Apex fused MoE epilogue in the same CPU unit.
- A zero middle plane in Inkling's expert layout is valid. The Apex
  planner attempts to upload it and fails. The Inkling planner already
  handles that entry. Route to it when Inkling is selected.

The intermediate startup attempts exposed separate boundaries. First,
the localized Inkling unit had no initialized globals. Then inference
reached a custom operation absent from the Apex executor. After adding
the graph hooks the old CPU dispatcher remained on the slow path and
a request timed out at 120 s. Enabling the experimental Inkling CUDA
formats reached `MUL_MAT_ID`, which failed until the Inkling expert
planner was wired into the common driver. All are addressed in the
recorded binary.

## Results

| Fixture | Reference prefill / decode, tok/s | Combined prefill / decode, tok/s | Client time, s |
| --- | ---: | ---: | ---: |
| Inkling 8192/256 | 276.325 / 18.386 | 277.894 / 19.840 | 43.926 → 42.617 |
| Apex 32768/512, pageable | 1744.212 / 46.374 | 1747.591 / 46.755 | 29.902 → 29.776 |

One fresh candidate run per model, compared with the existing EXP-1952
and EXP-1950 reference measurements. This is a correctness and
integration test, rather than a paired performance acceptance block.
For each model all returned IDs, text and final `/props` matched exactly.
Inkling also matched all 8703 intermediate state fingerprints and the
prefill expert traffic of 5389 fills / 23243231232 bytes. Board peaks
were 11710 and 10694 MiB for Inkling and Apex, respectively. Process
peak RSS was 14112392 and 21476164 KiB. Neither request had process swap
or system swap-out. Startup did incur system swap-out, outside the
request window. The pinned Apex performance gate has yet to run on
this executable.

## Reproduction

Build with `bash source/build.sh /absolute/new/scratch` while in the
repository. It uses KeelWing v0.3.0 and GCC 13, the frozen EXP-1880 and
EXP-1946 artifacts, local Apex host and CPU source snapshots and the
EXP-1951 driver. A clean scratch rebuild reproduced the exact executable
hash. See `inputs.sha256` and `results.json` for hashes, full argv, env,
memory and timing. Raw runs are under
`build/inkling-apex-gate/single-link-1953`.

For each model start only the combined executable with its recorded
arguments and environment. After `/health`, POST the corresponding
fixture to `/completion`. Use
`build/checkpoint-apex-exp1880-before-inkling/evidence/request.json` for
Apex or
`build/inkling-exp-baseline/device-attention/inkling-moe-tile32/request-8192-256.json`
for Inkling. Do not mix the Inkling and Apex environment or memory
profiles. Production binaries and launchers are still unchanged.
