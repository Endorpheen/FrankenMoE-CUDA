# P6-10: the sampler chain is not reached by the shipped build, and that was not recorded

Found 2026-11-19 by `tools/production-doors.py`: of the doors of
`goose/sampling/sampler-chain-full.goose`, **none** is referenced by any source under
`vendor/flint`, `runtime` or `integration`.

## The two copies of the sampler disagree, and the build takes the silent one

`runtime/reconstructed/build.py` compiles thirteen carrier sources from
`runtime/reconstructed/src`, and `llama-sampler.cpp` is one of them. The two copies differ exactly at
the doors:

| copy | `fg_sampler` references | compiled by the build |
|---|---|---|
| `runtime/reconstructed/src/llama-sampler.cpp` | **0** | **yes** |
| `vendor/flint/src/llama-sampler.cpp` | **30** | no (it is in the carrier list, so the vendor glob skips it) |

So the shipped build compiles the copy that never asks the Goose sampler, and the copy that does ask
it is not compiled. `llama_sampler_chain_apply` (`llama-sampler.cpp:845`) runs the native chain.

## What this means for P6-10 and for earlier statements

- **Sampling is not migrated in the shipped build**, whatever the module exports: fifteen doors,
  thirty call sites in the copy nobody compiles, zero in the copy that ships.
- The **grammar** part is reached: `llama-grammar.cpp` calls the grammar door directly, which is why
  the grammar work of this session could be gated at all.
- Any earlier sentence that treated "the sampler module is in the unit" as "sampling is served by
  Goose" was wrong for this build. Rule 33 says a module that compiles is not a module that is used;
  this is that rule finding a second, larger instance.

## Why the carrier rule did not catch it

The rule added today compares the symbol sets each copy *defines* and refuses when they differ. For
`llama-sampler.cpp` the defined symbol sets are equal - the difference is in what the copy *calls*,
which a definition comparison cannot see. The production-reach tool sees it because it asks about
references, not definitions.

## What closing this needs

A decision, then a measurement:

1. compile the copy that calls the doors (`vendor/flint/src/llama-sampler.cpp`) or add the same calls
   to the carrier - the choice changes which sampler implementation ships, so it is not a mechanical
   fix;
2. gate the sampler doors against the native chain with fixed seeds (P6-10 asks for exactly that);
3. only then the differential on generation, and only then the grammar cost item.

Until step 1, the honest statement is that the shipped sampler is native and the module is debt in
the unit.

## Third correction: the sampler IS served, by a different module family - and my own tool undercounted it

The paragraph above is wrong, and the reason is worth keeping: it equated "no door of
`sampler-chain-full.goose` is called" with "the sampler is native".

The compiled carrier `runtime/reconstructed/src/llama-sampler.cpp` calls a **different** generation of
the port:

```
fg212s_min_p_scan  fg380s_decide  fg384s_softmax  fg386s_pick  fg387s_decide  fg388s_decide
```

and those are provided by the experiment bridges that `build.py` links explicitly
(`('EXP-2026-11-11-380-flint-penalties-accept-live', 'b380.o')` and its neighbours at lines 149-155).
Each bridge is the generated host shim over a Goose module - `bridge.c` in EXP380 ends with
`#include "host-qualified.c"` and calls `fg380s_generated_main`, so the decision underneath is Goose.

So the shipped sampler is served by Goose through the per-sampler slice family (EXP212, EXP380,
EXP384-388), with a C bridge as the host, which is debt but not native logic.
`sampler-chain-full.goose` - the consolidated module - is what nothing calls, and that is the real
state: a superseded consolidation, not a native sampler.

## What this says about tools/production-doors.py

It counts doors of the modules **in the unit list**, so it cannot see a decision that reaches the build
through an experiment bridge. Its 72-production figure is therefore a lower bound on what is served,
and its "none" column can contain a door whose function is served by a bridge under another name. The
tool needs the bridge objects in its index before its numbers can be quoted as migration coverage -
recorded here so the number is not used as if that had been done.
