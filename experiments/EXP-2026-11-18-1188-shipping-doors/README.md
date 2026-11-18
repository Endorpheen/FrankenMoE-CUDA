# EXP-2026-11-18-1188: the accepted stages re-measured through the shipping doors

The chain now addresses its entry points by exported name (EXP-1187). That changed the ABI, so
the accepted numbers were re-measured against the library that is actually linked into
`llama-server`, not against an experiment build.

Method: the existing differential harness is forwarded to the shipping doors by a small C file,
one door per stage, and linked against `builds/flint-acc/bin/libllama.so`.

```
typical-p:      cases=1800 mismatches=0
top-p unsorted: cases=900  mismatches=0
top-p large:    cases=45   mismatches=0
xtc:            cases=720  mismatches=0
```

So typical-p, top-p and XTC are bit-exact through the exported doors, and the accepted results
carry over to the new calling mechanism.

## Two harness defects found on the way, both mine

1. **Stale op numbers.** The harness called the dispatcher with `op=2` for XTC, which is top-k
   in the current module, and my first forwarder compounded it by passing arguments in the wrong
   order. One door per stage, addressed by name, removes the possibility: there is no op number
   left to be stale.
2. **An aliased scratch buffer.** The typical door writes into its `scr` parameter; my first
   forwarder passed the logits array as that scratch, so the comparison ran against mutated
   input. A separate buffer fixed it.

## DRY is not re-measured here, and why

The harness's DRY case calls `gate_dry_c`, which returns the repetition limit position. That
forwarder no longer exists anywhere in the tree - the earlier experiment that provided it was
not kept as source - so the call was re-pointed at `fg_sampler_dry_call`, which implements a
different contract, and 153 of 200 cases disagree because of that, not because of the module.
Recorded as a gap: the DRY differential needs its forwarder written again against the exported
door before its numbers can be claimed for the new build.
