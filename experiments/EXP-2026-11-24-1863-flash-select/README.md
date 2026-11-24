# EXP-2026-11-24-1863 — Resolve only the selected causal Flash entry

**PROVEN and accepted as the research prefill checkpoint.** This
removes the repeatable 2 MiB VRAM delta in
[EXP-1862](../EXP-2026-11-24-1862-flash-clean-ab/README.md).
The working release is independent of this checkpoint.

The EXP-1861 driver resolved the original causal Flash entry and then
overwrote its handle after loading the SM89-specific embedded image.
Resolve the original entry only if the optimized kernel was disabled,
unsupported, or unavailable. The embedded cubin, the original image,
the launch arguments, and the prefill and decode arithmetic are unchanged
from the corresponding EXP-1862 arms.

## Full Apex 32K/512, RTX 4070

Context 262144, expert bank limit 6290 MiB, batch and ubatch 4096,
Q4_0 KV, decode split 4/count 1. Each fresh server took one warm-up and
one scored request. All four servers used the same binary. The control
set `GOOSE_FLASH_INTERIOR=0`. The candidate left the flag unset.

| Arm | Prefill tok/s | Decode tok/s | Client s | Sampled peak VRAM MiB | Peak RSS KiB |
| --- | ---: | ---: | ---: | ---: | ---: |
| B1 | 3383.74 | 82.83 | 15.942 | 8774 | 21491748 |
| A1 | 3248.84 | 80.64 | 16.512 | 8774 | 21491564 |
| A2 | 3238.39 | 81.74 | 16.459 | 8774 | 21492708 |
| B2 | 3348.92 | 83.59 | 15.987 | 8774 | 21491424 |

Mean prefill rose from 3243.62 to 3366.33 tok/s (+3.78%, 368 ms).
Mean measured decode was 81.19 vs 83.21 tok/s. The changed entry
is used for prefill. This experiment does not attribute the decode
difference to the patch. Sampled peak VRAM was 8774 MiB in every arm.
Peak RSS ranges overlap, with lower mean RSS for the candidate.
All eight replies matched the reference's 512 IDs and exact text.
All post-request properties, including the SLRU counters, matched
the reference. Runtime swap and system swap-out were zero.

## Reproduction

Use the embedded build from EXP-1861 and apply
[source/flash-select.patch](source/flash-select.patch) to its
`cuda-driver.c`. Reuse the same embedded image header, EXP-1856
include files and frozen infer-main. Compile only the driver object
with GCC 13 and relink the EXP-1861 archive as follows.

```sh
flags=(-O1 -std=c11 -D_GNU_SOURCE -D_FORTIFY_SOURCE=3 -fstack-protector-strong -ffunction-sections -fdata-sections)
includes=(-I "$exp1856/build-inputs" -I "$tree/vendor/flint/ggml/include" -I "$tree/vendor/flint/ggml/src" -I "$tree/integration/m1/host" -I "$tree/integration/goose-standalone")
gcc-13 "${flags[@]}" "${includes[@]}" -Werror=return-type -c cuda-driver.c -o cuda-driver.o
cp "$exp1861/lib.a" lib.a
ar rcs lib.a cuda-driver.o
gcc-13 "${flags[@]}" "${includes[@]}" -Wl,--gc-sections "$exp1856/build-inputs/infer-main.c" lib.a -lm -ldl -pthread -o frankenmoe-infer
```

The exact accepted research checkpoint is
`codex-20261124-flash-select-accepted`. Verify its
[SHA-256 manifest](SHA256SUMS) before reproducing the paired run.
Use the checkpoint's unchanged argv and env, with the flag only
on the controls. Diagnostic log selection, full timings, sample
counts and run IDs are recorded in [results.json](results.json).

Use this binary for the next Apex prefill comparison. At the
observed mean the remaining time reduction to 3500 tok/s is about
372 ms. The earlier full-window trace identifies dense Q4 MMQ as
a candidate for additional prefill savings. The independent decode
target remains 100 tok/s.
