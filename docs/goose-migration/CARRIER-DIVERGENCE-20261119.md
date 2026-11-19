# Carrier divergence: the two copies of a source are not interchangeable

Found on 2026-11-19 while checking P9-03. Nothing shipped is wrong - the release was cut from
the accepted checkpoint, and no rebuild has run since - but a rebuild from `main` today would be
wrong in one direction or the other, and the build now refuses instead of choosing.

## What was found

`runtime/reconstructed/build.py` compiles thirteen "carrier" sources from
`runtime/reconstructed/src/`, and the same sources exist under `vendor/flint/src/`. For every one
of the thirteen the two copies differ, and for at least three the difference is not cosmetic:

| copy | what it has |
|---|---|
| `runtime/reconstructed/src/llama-model-loader.cpp` | calls 51 doors, **does not define** `fg_port_array_str` and the rest of the `fg_port_*` family |
| `vendor/flint/src/llama-model-loader.cpp` | **defines** the `fg_port_*` family (used by `llama-impl.cpp`, `llama-vocab.cpp`), calls none of the expert-cache doors |
| `runtime/reconstructed/src/llama-moe-cache.cpp` | calls `fg262a` (5), `fg263c` (11), `fg756s` (8) - the expert-cache decisions |
| `vendor/flint/src/llama-moe-cache.cpp` | none of those three |

Neither copy is a superset. That is why the earlier attempt at a rule - "compile the vendor copy
when the staged one looks pre-port" - was wrong twice: the first version tested for the literal
string `fg_`, which the expert-cache door names do not contain (`fg262a`), and the second version,
which counted doors properly, would have kept the staged copy and broken the link on
`fg_port_array_str`.

## What the build does now

It refuses. For each carrier it compares the `extern "C"` symbols the two copies define; if they
differ it prints both sets and exits 1, pointing at this document:

```
build.py: the two copies of llama-model-loader.cpp are not interchangeable
  defined only in the staged carrier: [...]
  defined only in the vendor source:  ['fg_port_array_str', ...]
  compiling either one loses the other's symbols; merge them in an accepted step
```

A refusal is the honest behaviour: a silent choice would ship a runtime with less of the port in
it, and the two doors that would disappear (`fg262a`, `fg263c`, `fg756s`) are the expert-cache
admission, copy and SLRU decisions - P9-03's subject.

## What the merge needs

1. Take the `fg_port_*` definitions from the vendor copy into the staged carrier (or the door
   calls the other way), so one file carries both.
2. Compile that translation unit alone and check both symbol families are present in the object -
   seconds, and the cheap pre-flight rule 38 asks for before any build.
3. Then a full build, then the acceptance run that P9-03 already needs.

Until step 3 exists, the released artefact `v0.10.0-exp1476` remains the current binary, and no
claim is made about building from `main`.
