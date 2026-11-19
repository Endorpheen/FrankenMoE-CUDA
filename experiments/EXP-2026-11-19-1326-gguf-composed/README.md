# EXP-2026-11-19-1326: the GGUF reader's closure is composed; the unit grows and the baseline does not move

## What was done

The GGUF reader was built in the separate main unit and reached by nothing (EXP-2026-11-19-1324). Following the
compiler's errors added its closure to the shared unit: the header helpers (`gguf-headers`), the tensor helpers
(`gguf-tensors`) and the reader itself.

```
gr_* doors in the shipped library:   0 -> 14
declarations in the composed unit:  418 -> 492
```

Coverage after the change (`tools/check-module-coverage.py`):

```
14 / 14   goose/model/gguf-reader.goose
15 / 15   goose/model/gguf-tensors.goose
15 / 15   goose/sampling/sampler-chain-full.goose
21 / 21   goose/metrics/metrics-traces.goose
```

## Performance, three paired runs

```
run1  decode 62.25 63.04 63.07   prefill 2682.4 2953.9 2948.3
run2  decode 62.45 62.80 63.20   prefill 2690.5 2951.1 2944.9
run3  decode 61.98 62.72 62.55   prefill 2686.4 2940.6 2944.0

median of nine: decode 62.72, prefill 2944.0
against EXP1298 (62.73 / 2948.0): decode -0.0 %, prefill -0.1 %
```

The single run taken first showed 61.51 and would have been reported as a two-percent regression. Three paired runs
show the baseline is untouched, which is the reason the rule requires them.

Response hash `c2ff42cc1219` in all three.

## What is still missing

The doors exist and are referenced by no C code: the loader still parses GGUF natively. As with the detokenizer,
composing is the prerequisite, not the connection, and no accepted-component entry is added until a call path
exists.

## What this changes about the unit

It is now 492 declarations, and every acceptance run carries it. The cost is zero at run time for unused code and
nothing measurable in the numbers above, so the composition can stay while the call path is built.
