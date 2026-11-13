# EXP468: composite constructor base with fg159+fg156+fg160, patched and Apex-gated

## Build

The composite base was rebuilt from the recorded EXP160 recipe with the patched
constructor module:

1. `constructors.goose` regenerated with the recorded compiler
   (`stage3-f849…`, hash checked) and re-qualified, so
   `constructors.h` includes the patched implementation;
2. `shared-core.o` rebuilt with the recorded `shared-core-build` command;
3. `shared-scheduler.o` rebuilt with the recorded `shared-scheduler-build`;
4. base linked with the recorded `shared-base-link` argv into a diagnostic file.

The result exports the symbols the CUDA backend needs (`fg160_fusion_enabled`,
`fg160_query`) and the constructor unit `fg156_constructor_seen`.

## Constructor gate

| probe | flint | composite patched base |
|---|---|---|
| `probe.c` full surface (1D-4D, views, permute, transpose, reshape, row/element size) | completes | **identical output, `diff` empty** |
| `probe-errors.c` case 7 (view_3d Q4_K, `ne0 = 128`) | no abort | **no abort** |
| cases 1, 2, 4, 5, 6 | abort | abort |
| case 3 (overflow of 2^40 x 2^40) | abort | no abort — **pre-existing** in the deployed base too, recorded as an open compatibility gap |

## Apex gate, measured under load

A parallel compiler build ran during these measurements (`loadavg` 3.4-4.3 on 32
threads); the numbers are **measured-under-load** and are compared against the
deployed-base control from EXP467 rather than against the idle record.

| profile | content sha | tokens | probabilities | prefill tok/s | decode tok/s |
|---|---|---|---|---|---|
| 32K/128 composite patched | `9d90a010…` (identical) | 128/128 identical | 128/128 identical | 2219.76 | 36.82 |
| 32K/128 deployed control | `9d90a010…` | reference | reference | 2161.14 | 36.33 |
| 32K/512 composite patched | `c2ff42cc…` (identical) | — | — | 2227.76 | 39.29 |
| 32K/512 deployed control | `c2ff42cc…` | — | — | 2140.41 | 39.06 |

Content, token ids and all 128 probability entries match the accepted base
exactly; the paired decode rates are equal or higher than the same-load control.
Absolute figures sit below the idle record (39.24 / 43.04) because of the load and
are not presented as clean numbers.

## Artifacts

- probe outputs: `tests/probe-flint.out`, `tests/probe-comp.out` (identical)
- Apex results: `tests/p403-comp-128.result.json`, `tests/p403-comp-512.result.json`,
  and the deployed controls `tests/p403-128.result.json`, `tests/p403-512.result.json`
- runner: `tests/run-comp.sh`
