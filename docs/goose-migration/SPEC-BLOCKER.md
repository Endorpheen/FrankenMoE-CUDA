# Blocker resolved: the server is built from a merged copy, not from the vendor source

## The answer

`vendor/flint/tools/server/CMakeLists.txt` compiles

```
${FRANKEN_REPO}/runtime/reconstructed/server-merge/server-context.cpp
```

and **not** `vendor/flint/tools/server/server-context.cpp`. The vendor file is the upstream ancestor
kept for reference; the four ported forks were merged into the file under `runtime/reconstructed/`
and that merged file is what runs.

Every instrumentation I added over four iterations went into the vendor file, which the build never
opens. That is why no print appeared, why `strings` found none of my format strings in the library,
and why the behaviour kept contradicting the code I was reading - I was reading dead code.

## Proof

```
strings libllama-server-impl.so | grep -c FG_ACCEPT_BRANCH   -> 0     (before, in vendor)
                                after editing the merged file -> 1
```

and the run then printed, first time:

```
FG_SEQ_RM type=2 (0=no, so speculative is disabled)
FG_ACCEPT_BRANCH synth=0 replay=0 reject=1 draft=8
```

## What the numbers say about the speculative path

- sequence removal is available (`type=2`), so speculation is enabled - my earlier hypothesis that it
  was disabled was wrong;
- the **rejection** branch runs with 8 draft tokens, because `use_spec_rejection()` is `temp > 0`;
- `synth=0`: synthetic probabilities are empty for ngram drafters, which contradicts my earlier
  conclusion that the synth branch was taken. That conclusion was drawn from the dead file.

So the live acceptance decision is the **rejection** routine, and the prefix decision applies only to
the `temp = 0` greedy route, which is a second, separate measurement.

## Lesson, and it is the expensive one

Three of the four iterations on this thread were spent reasoning about a file that is not compiled.
The cheap check that would have caught it at once, and should come first every time:

```
cmake --build ... -v | grep server-context.cpp     # or find the .o and read its source path
```

The project already records "an empty result is not proof"; this adds its sibling: **reading a file is
not reading the build**. A `strings` check for one of your own format strings in the produced artefact
answers it in a second.
