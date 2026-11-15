# EXP-2026-11-15-664 — D0: the M1 integration layer lives in the repository

Base: `6659beb1`. No model, no server, no GPU run. Build only.

## 1. The problem this fixes

Every accepted Goose stage of this phase exists twice: as a canonical module in
`goose/` and as a *carrier* that swaps a native body for the composed unit. The
canonical half is tracked. The carrier half was not: the eight carrier patches,
the eight stage bridges, the host adapter, the IQ descriptor header and the ten
gates existed only inside `/tmp/opencode/sub-659` and `/tmp/opencode/sub-661`.
`P11-04-REMOVAL.md` states the consequence in its first line: *nothing in the
tracked tree can be deleted now, because the staged tree is not in the tracked
tree*.

A `/tmp` tree is not a migration. It cannot be reviewed, it cannot be rebuilt
after a reboot, and the removal steps D1–D4 that P11-04 schedules cannot start
from it. D0 of the removal order is exactly this step: put the carriers into the
canonical tree, keep the Apex reference, and prove that the tree builds.

## 2. What was added

| path | content |
|---|---|
| `integration/m1/patches/` | the eleven carrier patches plus `TARGETS`, the patch-to-path manifest |
| `integration/m1/common/` | the eight stage bridges, the three decide units, the stream policy, the two carrier headers |
| `integration/m1/host/` | `goose_host.c` (the one adapter, 51 primitives, one `pthread_once`), `goose-host.h`, `goose-iq-tables.h` |
| `integration/m1/gates/` | the ten accepted gates plus `m1` (host smoke) and the IQ seam gate |
| `integration/m1/CMakeLists.txt` | the M1 project: ten stage flags, one unit, one adapter, `libfg-goose-runtime.a` |
| `tools/stage-m1-tree.sh` | assembles the whole staged tree from tracked inputs |

The carriers are byte-identical to the ones the accepted experiments measured:
they were generated with `diff -u` between `upstream/llama.cpp` at its current
revision and the frozen M1-b tree, so the patches carry the same bodies the
reports describe. The unit itself is **not** committed: `--with-unit` composes it
with `tools/build-main-runtime.sh`, and generated C stays out of the repository
by the storage rule.

## 3. Reproducibility proof

```
bash tools/stage-m1-tree.sh /tmp/stage-test2 --with-unit
cmake -S /tmp/stage-test2 -B /tmp/stage-test2/build-off -G Ninja \
      -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc-13 -DCMAKE_CXX_COMPILER=g++-13
cmake --build /tmp/stage-test2/build-off -j 16
cmake -S /tmp/stage-test2 -B /tmp/stage-test2/build-on  <same> -DFG_GOOSE_*=ON (all ten)
cmake --build /tmp/stage-test2/build-on  -j 16
```

| check | result |
|---|---|
| carrier patches applied | 11/11, no rejects |
| configure, OFF and ON-all | rc=0 both |
| build, OFF | rc=0, 0 errors |
| build, ON-all | rc=0, 0 errors |
| `llama-server`, OFF | 17 794 640 B |
| `llama-server`, ON-all | 22 136 040 B |
| `goose_init` definitions, OFF | 0 |
| `goose_init` definitions, ON | 1 |
| gates, OFF (10) | 10/10 green, `goose_calls=0` in every one |
| gates, ON (10) | 10/10 green, counters match the accepted cards |
| `fg-host-smoke` | `threads=2 calls=512 goose_init_runs=1 iq_bound=1 bind_errors=0` PASS |
| `fg-iq-seam` | `unbound_aborts=1 bind_rc=0 replace_rc=-4 checks=15360 failures=0` PASS |
| Apex reference binary | untouched |

The OFF size is **byte-identical** to M1's recorded 17 794 640 B, which is the
strongest available signal that the tracked carriers reproduce the measured tree.
The ON size is larger (22 136 040 vs 22 050 592, +85 448 B) because the unit is
composed from the current 110-module list instead of M1's frozen 98 modules; the
difference is unit content, not carrier behaviour. Every gate counter that the
M1 report records is reproduced exactly.

## 4. Gates, staged tree rebuilt from the repository

`evidence/gates.tsv`, 23 rows:

| gate | OFF | ON |
|---|---|---|
| `fg-goose-smoke` (S1.1 schema) | 30 cases / 182 rules, 0 failures, calls 0 | same, `goose_calls=30` |
| `fg-gguf-smoke` (S1.2) | 14 cases / 41 checks, calls 0 | 14 / 55, `goose_calls=394` (write 13, meta_size 14, meta_data 14, split 347, layout 6) |
| `fg-goose-quant-smoke` (S1.3) | 46 cases, calls 0 | 58 cases, `row_calls=24`, `blck_calls=7` |
| `fg-goose-metrics-smoke` (S1.4) | 53 cases / 91 values, calls 0 | same, `goose_calls=53` |
| `fg-goose-tokenizer-smoke` (S1.5) | 182/182, calls 0, pins 0 | 182/182, `goose_calls=182`, pins 2 |
| `fg-goose-stream-gate` (S1.6) | 71 cases, calls 0 | 71 cases, `goose_calls=74` |
| `fg-config-smoke` (S2.1) | all checks passed | all checks passed |
| `fg-spec-smoke` (S2.2) | 33 runs, 567 emitted | same |
| `fg-model-smoke` (S3.1) | 84 cases / 198 values, calls 0 | same, `goose_calls=84` |
| `fg-session-smoke` (S3.2) | 191 cases / 929 checks, calls 0 | same, `goose_calls=376` |

## 5. What this does not do

1. It does not flip any flag ON in the shipped build: all ten still default OFF
   in `integration/m1/CMakeLists.txt`, as rule 10 requires them to default ON
   only once the switch is a runtime decision rather than a build decision.
2. It does not remove a native body. D1–D4 of `P11-04-REMOVAL.md` stay open.
3. It does not run a model, so no performance or decode-correctness claim is made.
4. The CUDA half is out of scope here: this tree is CPU only, as M1 was.

## 6. Why this had to come first

`P11-03`, `P11-04`, `P11-05`, `P11-07` and `P11-08` all consume the staged tree.
While it lived in `/tmp`, every one of them could only be verified inside a
throw-away copy, and a reboot would have destroyed the only copy of the carrier
work. With this commit the tree is a function of tracked inputs, and each
remaining removal step becomes reviewable as a diff against it.
