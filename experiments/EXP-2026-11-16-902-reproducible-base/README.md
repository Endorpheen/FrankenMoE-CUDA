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

## Base task closed: saved, reproducible, and the running runtime IS the preserved set

```
running libllama  3047d195e09aee68
stored  libllama  3047d195e09aee68   -> IDENTICAL
```

So restoration is not a claim to be tested later; what executes now is byte-for-byte the preserved artifact.

Store: /home/end0/AI/LlamaServer/runtime-store - a local git outside the source repository, no LFS, no remote, no
ignore rules for binaries, 107 binary files committed as bytes.

| set | files | size | contents |
|---|---|---|---|
| 2026-11-16T15-20-pre-consolidation | 29 | 235 M | library, server, 7 runtime libs, 18 objects |
| 2026-11-16T15-31-clean-build | 87 | 6.1 M | tree-built library, 80 objects, build recipe, PROFILE.txt |

Both verified by extraction: `git archive HEAD | tar -x` then `sha256sum -c` -> all files match.

Consolidated tree: /home/end0/AI/LlamaServer/runtime-build-tree-v6, 390 input files with SHA256SUMS - our sources
and 35 headers, 36 resolved modules, the bridges, 44 flint objects, the extra_mod objects, 7 libraries. 252 MB,
down from 9.6 GB when every library symlink was resolved into a full copy.

```
bash tools/consolidate-runtime-tree.sh        freeze the inputs
TREE=<tree> bash tools/build-from-tree.sh     build in a clean directory
```

Implicit dependencies removed, each one real: module selection by "newest experiment on disk" replaced with
build.py's actual rule (prefixes from `nm -u` of our objects plus the explicit 25-bridge exclusion -> 41 needed,
36 resolved); stale objects in the build directory no longer picked up (fg367s.o existed although fg367s is a
bridge); non-PIC leftovers excluded; library copies limited to the seven actually loaded.

What the base still does not contain: the 42 tok/s point. No preserved binary for it exists, and it is not called
that here. The measured result on the preserved set is decode 31.30 / prefill 1788.41 / hash c2ff42cc1219, with no
improvement claimed over 27.53 because the difference sits inside this host's run-to-run spread.
