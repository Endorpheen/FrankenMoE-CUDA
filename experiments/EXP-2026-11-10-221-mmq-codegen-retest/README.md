# EXP221: MMQ Q8_1 codegen retested on the newest compiler — NO CHANGE

EXP180 ported the MMQ Q8_1 quantizer byte-exactly but was 11.4% slower, and the recorded cause was
the emission itself: 824 SASS instructions against native's 160, mostly address arithmetic. Since
then the compiler shipped nonlocal-return and named-result fixes. This retest asks one question —
**did the emission change?** — with a reproducible measurement rather than an impression.

Verdict: **no. Identical instruction counts.** MMQ remains blocked.

## Measurement

Both sides go through the same path: emit `device.goose` → extract the device functions
(`emit_sources.py`'s mechanical extraction) → `nvcc -O3 -arch=sm_89 -cubin` → `cuobjdump -sass` →
count instruction lines.

| emission | total | IMAD | BRA |
|---|---:|---:|---:|
| recorded (EXP180 baseline) | 824 | 238 | 29 |
| **subset-7ef08c2d (newest)** | **824** | **238** | **29** |

Native reference from EXP180: 160 instructions.

Byte size of the extracted header did grow: 33529 → 36506 bytes (+8.9%), and the raw generated C
grew 1808 → 2144 lines with more `%`, `*` and shifts. **C size and SASS instructions disagree**, and
the SASS number is the one that matters — the extra C is not becoming extra instructions. Worth
knowing before anyone reads growth in the emission as a regression.

My first attempt counted IMAD as 238 where EXP180's note says 206. I did not reconcile the two:
likely a different match (all `IMAD` mnemonics vs a counted subset). The comparison that matters is
like-for-like between the two emissions, and that is exact.

## The baseline is a file, not a build

The pinned compiler `subset-84f31960f9f3031e` **cannot compile today's `device.goose`**:

    device.goose:188: error: expected identifier
        if (iqs&(15 as i64))==0&&iqs<96{

The parser fix that made this source reproducible landed after that build. So the 824-instruction
baseline is the extraction already recorded in `source/device-functions-generated.cuh`, not a fresh
run of the old pin. That is the artifact which produced the measured 11.4% slowdown, so it is the
right baseline, but it means the "old" side cannot be regenerated from source on this machine.

## What would unblock MMQ

The instructions are address arithmetic, not the quantizer's arithmetic. Native computes all
addresses once per block; the emission recomputes them per access. A fix on the compiler side, or a
rewrite that passes precomputed byte offsets in so the kernel body is `base + i0`, is what the
number says is needed. Nothing measured here shows any of that has happened.

## Reproduce

    /tmp/sass180.py   # copies cuda-runtime.cu beside each generated header, then nvcc + cuobjdump

Kept as a script rather than prose so the 824 figure can be re-derived after any compiler release
instead of being quoted.
