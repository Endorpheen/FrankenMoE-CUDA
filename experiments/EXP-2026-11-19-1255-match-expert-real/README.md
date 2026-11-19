# EXP-2026-11-19-1255: the matcher against the real static function, by white-box include

`match_expert` is `static` in `router_hook.cpp`, so no link could ever reach it - that was the reason the
matcher had been verified against my transcription of its source. A translation unit can include the file:

```cpp
#include "core/src/moe/router_hook.cpp"     // the static is now in scope
```

```
match_expert differential: cases=4160 fails=0 recipes=9
```

4160 comparisons: for every recipe row and every used slot, 40 layers, each with four name variants - the
positive name, the `.scale` companion, a wrong suffix, and a missing `blk.` prefix - compared slot **and**
layer against `ar_match` in the built library.

## Two stubs, and why they cannot affect the result

The included file pulls the whole `RouterHook`, which needs symbols the policy build does not carry:

- `bmoe::pio::major_faults()` - a hard-page-fault counter used in telemetry;
- `DeterministicWorkload::*` - the capture/replay feature.

Both are stubbed in the harness. Neither is read by `match_expert`, which parses a name against a recipe row and
returns a slot, so the compared decision path is byte-for-byte the native one. That is the condition under which
a stub belongs in a differential, and it is stated in the source rather than left implicit.

## What this closes

The recipe verification no longer contains a single transcription: the table comes from `bmoe_core`, the matcher
from the included native file, and only the ported side is loaded from our library. Every one of this session's
transcription errors would have been impossible in this arrangement.

## Reproduce

```
bash experiments/EXP-2026-11-19-1255-match-expert-real/build.sh
```
