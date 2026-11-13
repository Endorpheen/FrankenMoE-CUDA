# MMQ Q8_1: the rejection rested on a 5-point-spread measurement

Checked EXP180's own `apex-summary.json` instead of the summary line that has been repeated since.

## What the Apex numbers actually were

| pair | prefill | decode |
|---|---:|---:|
| forward | +0.46% | **+0.15%** |
| reverse | +0.71% | **−4.97%** |
| mean | +0.58% | −2.41% |

All 128 token ids, text and every returned top-5 probability were equal in every run.

The decode reading swings **5.1 points** between the two orders of the same pair. That is the exact
artifact this project identified later and documented in `census-status.md`: "whichever arm runs first
pays a warm-up of roughly 1.3 tok/s … the sign reversed when the order reversed." The mean −2.41% is
an average of two contaminated readings, not a measurement.

So the component was rejected on a number the project subsequently learned not to trust, and it has
been sitting as "not promoted" ever since on the strength of that number.

## What has changed since

EXP222 pinned `ne2`, which is worth **−3.69%** at operator level on the same harness (goose 2078.8 →
2002.0 ns), and brought the kernel from 400 to 164 SASS instructions against native's 157. EXP180 was
measured **before** that fix.

## Why it cannot simply be re-measured

EXP180's DSO is built on the **EXP179 lineage**. It predates the KV cell work, the loader, the
tokenizer text layer and the Unicode modules — everything from EXP199 onward. Running Apex against it
would measure MMQ on a runtime that is 40 commits stale, and any comparison against `exp220` would be
between incomparable builds. That is the mistake this project has already made twice.

## The correct next step, stated concretely

1. Build the ne2-pinned MMQ q8_1 route into the **exp220 lineage**, not EXP179's.
2. Run the full interleaved protocol (old → new → new → old, more than one pair) at 32K/128.
3. Accept only if the decode delta is inside 2% on a measurement whose own arms agree.

Only then does the "MMQ not promoted" verdict have a basis that is still valid. Until then the honest
status is: **rejected on a measurement later shown unreliable, never revisited on a comparable build.**

This is the largest open item — the MMQ family plus its q8_1 feeder is 45.12% of kernel time — so
re-doing that one measurement properly is worth more than any further work on the small host modules.
