# EXP-2026-11-17-883 — Apex's own architecture plan is composed into the working runtime

EXP-882 stopped at `undefined symbol: fg200g_ld_u32`. This finishes it the way that note said it had to be
finished: a dependency walk instead of a guess.

## The walk

Listing every `U fg*` symbol in the two generated objects gave the exact missing set, and it was larger than
the modules' own `extern` declarations:

| object | undefined, beyond libc and the Goose runtime |
|---|---|
| `fg-q35.o` | `fg158_rope_multi`, `fg200g_fail`, `fg200g_ld_u32` |
| `fg-qg.o` | `fg158_fail`, `fg158_view4` |

`gs_*` were already satisfied by the SLRU runtime object. Four of the five missing symbols live in
`integration/goose-standalone/graph-bindings.c` and the fifth in `integration/m1/host/goose_host.c`.

## What was linked, and what was not

Linking those two files whole would have pulled in the standalone's entire binding surface and risked clashing
with symbols the DSO already owns. Instead the five functions were extracted verbatim into
`fg-apex-plan-ffi.c` — it exports exactly five symbols and nothing else:

```
fg158_fail  fg158_rope_multi  fg158_view4  fg200g_fail  fg200g_ld_u32
```

That is the minimal C boundary: translation only, no logic of its own.

## Verified

```
FG_Q35_PLAN layers=41 attn=10 recr=30 mtp=1 PASS
```

Asked of the **shipped library**, through `dlopen`/`dlsym`, not of a standalone build. 41/10/30/1 is the answer
`gdn_plan` and `stack_dispatch` already proved, and the agreement is the point: the same plan now comes out of
the runtime a user runs.

The first attempt at this check printed 41/0/0 because the parameter vector was filled from guessed indices -
`p[10]` is 10/P_FF_SHEXP, not expert_used, and `p[13]` is DSD state, not the interval. With the real indices
(`apex_shape.c`) and Apex's GGUF values it returns the right split. Recorded because a plausible-looking number
from a wrong vector is exactly the kind of result this project has been burned by.

Apex with the pinned argv: reference hash `c2ff42cc1219`, decode 37.84 (36.79-40.00) — inside the session's
observed spread, no regression from composing the module.

## State

Apex's layer kinds, shapes and rope decisions were in C++ inside the shipped runtime while their Goose
implementation sat gated and unused. They are now present and answering in the DSO. **The call site is still
C++**: `llama-graph.cpp` continues to make these decisions natively, so this closes the composition half of the
gap and leaves the wiring half.

Cost: one build, one Apex run of three repeats, 168-case gate already in place from EXP-880 for the sibling
module.
