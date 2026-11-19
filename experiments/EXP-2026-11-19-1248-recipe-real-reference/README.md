# EXP-2026-11-19-1248: the recipe registry against the real native code, not a transcription

Every previous check of this registry compared the port with something I transcribed by hand, and the
transcription was wrong twice (wrong table, then a hyphen). The native registry builds on its own:

```
cmake -S runtime -B builds/bmoe -DCMAKE_BUILD_TYPE=Release   # llama.cpp present = OFF
cmake --build builds/bmoe -j 8                              # [100%] Built target bmoe_core
```

and it exports exactly what is needed:

```
T bmoe::find_moe_recipe(char const*)
T bmoe::n_moe_recipes()
T bmoe::moe_recipe_at(int)
```

`diff.cpp` links `libbmoe_core.a` and loads the ported doors from `libllama.so.0`, then compares:

```
recipe differential: cases=106 fails=0 recipes=9
```

106 comparisons, no transcription anywhere on the native side: row→arch, arch→row, `ar_exps`, all 27 suffix
slots (with the nullptr slot required to answer -1), and the matcher on names built from the **native** suffix
strings, including the `.scale` companion that must be rejected.

## What this changes

The previous evidence was "the port agrees with my reading of the table". This is "the port agrees with the
table". Those are different statements, and only the second one survives a mistake in the reader - which is
exactly what happened twice.

## The pattern for the rest of the P9 family

`bmoe_core` builds standalone and exposes the pure-policy decisions (registry, LRU policy, planning) without
llama.cpp. Any of them can be verified the same way: link the real library, `dlopen` the doors, compare. That
removes the transcription hazard from the whole family, and it needs no model and no GPU.

## Not covered

`match_expert` is static in `router_hook.cpp` and is not exported, so the matcher's native side is still my
transcription. The matcher cases here are stronger than before (they use native suffixes) but the native
matcher itself is not in the comparison.
