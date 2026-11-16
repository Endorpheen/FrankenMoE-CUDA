# The reproducible base works: built from a fixed tree, same reference answer

The library built by `tools/build-from-tree.sh` from the consolidated tree was installed over the working one and
run on the fixed profile.

```
FG_MODULES 8/8 PASS
decode 31.30 tok/s   prefill 1788.41 tok/s   sha c2ff42cc1219
```

Two things this settles and one it does not.

Settled: the consolidated tree produces a working runtime - same reference hash, modules answering, dependencies
resolved - so the base is functionally equivalent to what was running before it, not merely a build that links.

Settled: the swap was done with a backup first (rule 22). The replaced library is saved both locally
(`/tmp/libllama-before-clean.so`, sha `a698eb54...`) and in the store as set `2026-11-16T15-20-pre-consolidation`.

Not settled: throughput. 31.30 against the previous 27.53 is inside this host's measured spread - two identical
runs of one binary were recorded differing by 38% earlier in this session - so no improvement is claimed and the
42 tok/s point is still not reproduced. The honest reading is "no regression demonstrable either way at this
sample size", not "faster".

Prior numbers on the same profile for comparison, none of them a controlled pair with this run:
  27.53 / 1561.54   current working library before the swap
  28.43 / 1619.93   restored c5e6ff02 sources
  31.30 / 1788.41   this run, built from the consolidated tree
  42.01 / 2436.63   guarded2_512, the target, no preserved binary exists for it

What would make any of these comparable: interleaved paired runs of the two binaries, several repeats each, at the
same generation length. Sequential runs on this host cannot support a conclusion.
