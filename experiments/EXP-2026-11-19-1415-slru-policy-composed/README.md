# EXP-2026-11-19-1415: the expert-cache replacement policy is composed into the unit

## What was there

`goose/model/slru-policy-goose.goose` contains a 195-line `slru_plan` door - the cache's replacement policy: dedupe the
requested experts, promote hits, choose victims from probation first, fill. It was in no build set, so it had never been
compiled against the runtime's own state structures and had no symbol in the artefact.

```
nm -D libllama.so.0 | grep -c 'T slru_plan'   -> 1
```

## Why this is the shape of P9-03

P9-03 asks for 25 call sites in the expert cache, dense-weight placement and row streaming to be Goose decisions. The
cache's *policy* is the part with a pure decision and a clean input/output shape - the slot map, the two LRU lists, the
protected flags - and that part now exists as a reachable export. Placement and streaming are the parts that move bytes,
and they will be judged by whether the policy's decisions reach them.

## What is not done

- **no comparison with the native `plan()` yet.** The native function dedupes, promotes hits, sorts misses and picks
  victims; `slru_plan` takes the same state as slices and writes its own outputs through `scal` and the mutated arrays. The
  next step is a shadow call at `plan()` and a diff of the fills and evictions - and the door's output contract has to be
  read from its source rather than guessed, because a guessed contract produced a plausible wrong answer earlier in this
  project.
- **no call site**: the native path still decides.

## Protected

The component gate names `slru_plan`, so a rebuild that drops the module is caught before any benchmark:

```
current library      -> ok
EXP1412 checkpoint   -> FAIL, 1 accepted component(s) absent: goose-slru-policy
```

## Shipped path

Inert until the comparison exists: acceptance unchanged, hash `c2ff42cc1219`.
