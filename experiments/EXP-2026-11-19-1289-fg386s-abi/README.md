# EXP-2026-11-19-1289: `fg386s_pick` is not a shipped-runtime item, and its ABI agrees where it is used

## Where the symbol lives

```
shipped candidate (builds/flint-acc/bin/libllama.so.0):     0 symbols matching fg386s
compiled there:                                             vendor/flint/src/llama-sampler.cpp
declares fg386s_pick:                                       runtime/reconstructed/src/llama-sampler.cpp
compiled in:                                                runtime/reconstructed/build.py (b386.o bridge)
```

So the item "ABI with `fg386s_pick`" does not apply to the candidate runtime at all: the file that declares it is
not part of that build, and the shipped sampler uses the slice-based doors (`fg_sampler_dist_pick`,
`fg_sampler_dist_words`) that are already verified through the public API. It is a real item for the **independent
build**, which compiles the bridge that provides it.

## The ABI, compared rather than assumed

```
consumer  runtime/reconstructed/src/llama-sampler.cpp
provider  integration/modules/EXP-2026-11-11-386-goose-sampler-dist-pick-live/source/bridge.c

int64_t fg386s_pick(const float *, int64_t, int32_t, float *, double)   both sides
int     fg386s_enabled(void)                                            both sides
```

Both are C linkage, and the parameter lists agree once the names are removed.

## Why this is now a check and not a paragraph

`tools/check-abi-pairs.py` performs the comparison, because a header and a definition can agree on the name and
disagree on a parameter type - the link succeeds, the call reads garbage, and nothing reports it. That is the
milder cousin of `mt_fmt_g6`, which was declared and never defined at all and made a library unloadable
(EXP-2026-11-19-1269). The check runs in a second and covers both symbols.
