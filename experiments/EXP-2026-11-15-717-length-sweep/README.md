# EXP-2026-11-15-717 — the same configuration at 128, 256 and 512 tokens

Base: `3e6ec1d6`. Cache 4608 MiB, everything on. Rule 20 requires all three lengths before a
configuration is judged.

## 1. Result

| length | prefill median | decode median | decode range | reference | vs reference |
|---:|---:|---:|---|---:|---:|
| 128 | 2280.0 | 37.23 | 36.63-39.84 (4 runs) | 39.6 | **-6.0%** |
| 256 | 2331.5 | 35.88 | 35.21-36.50 (3 runs) | 41.0 | **-12.5%** |
| 512 | 2313.3 | 38.65 | 38.64-38.84 (3 runs) | 41.94 | **-7.8%** |

VRAM 7804-7810 MiB at every length, RAM 20.8 GiB, unchanged.

## 2. What the length sweep proves, and what it does not

**It proves the measurement was being read wrongly.** Decode at 256 (35.88) is lower than at 128
(37.23), which is the opposite of the amortisation trend the reference shows (39.6 -> 41.0 ->
41.94). So this build does not simply "lack 4 tok/s everywhere"; its per-length profile is
different from the reference's, and a single-length comparison was never going to describe it.

**The 512 figure is the most stable measurement in the whole session** - 38.64, 38.64, 38.84,
a spread of 0.2 tok/s on 1536 generated tokens per run. That is the number to trust for
optimisation work, and at 512 this build is 7.8% off the reference's 41.94.

**It does not prove where the difference comes from.** The 256-token dip is not explained. It is
recorded rather than smoothed over: three runs all land in a 1.3 tok/s band, so it is not noise,
but no candidate cause has been measured.

## 3. Balance across all four axes

| axis | 128 | 256 | 512 | reference band |
|---|---:|---:|---:|---|
| prefill | 2280.0 | 2331.5 | 2313.3 | 2228.76 |
| decode | 37.23 | 35.88 | 38.65 | 39.6 / 41.0 / 41.94 |
| VRAM MiB | 7804 | 7810 | 7810 | 6778 |
| RAM GiB | 20.8 | 20.8 | 20.8 | not recorded |

Prefill is above the reference at all three lengths (+2.3% to +4.6%). Decode is below at all
three. Memory is 15% above and constant, RAM is flat. That is a coherent picture: this build
computes the prompt faster and generates slower, with a fixed memory cost.

## 4. What is recommended

Cache 4608 MiB, everything on, is the configuration to keep for now:
- best measured decode at 128 (37.23) and a stable 38.65 at 512;
- prefill above reference at every length;
- memory cost known and constant.

It is NOT claimed to be optimal against the reference, and the 256-token dip means the
configuration has not been characterised well enough to call it stable.

## 5. Next, per rule 20

The candidates filtered out at 128 need re-testing at 512, where the measurement is most stable,
before any of them is rejected. Specifically: cache 3584 versus 4608 at 512 tokens. At 128 the
difference was 36.23 versus 37.23, inside the spread; at 512 it may separate cleanly.

All ten rows are in `benchmarks/experiment-history.jsonl` with their length.
