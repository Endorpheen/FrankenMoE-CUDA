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

## Why "merge the two files" is the wrong description

The two copies do not differ by degree - they solve the same problem in two different ways:

| copy | how it reaches the Goose reader |
|---|---|
| `runtime/reconstructed/src/llama-model-loader.cpp` | calls `llama_goose_register/unregister/ready/find_kv/arr_str/...`, C entry points owned by `integration/goose-standalone/gguf-reader-owner.cpp` |
| `vendor/flint/src/llama-model-loader.cpp` | carries an in-file `fg_port_*` block (`struct fg_port_store`, `dlopen("libllama.so.0")`, `dlsym("gr_*")`) and defines `fg_port_array_str` for `llama-impl.cpp` |

Both are legitimate; neither contains the other, and pasting the vendor's `dlopen` block into the
staged file would put two competing mechanisms for one mapping into the same translation unit.

## The smallest correct step

`llama-impl.cpp` exists **only** in the vendor tree (there is no staged copy) and calls
`fg_port_array_str` twice. The staged tree serves the same question through
`llama_goose_arr_str(ctx, index, element)`.

So the smallest change that makes both lineages link is a four-argument
`fg_port_array_str(key_index, element, ptr, len)` implemented in
`integration/goose-standalone/gguf-reader-owner.cpp`, the file that already owns the mapping -
ten lines, no new mechanism, and it keeps the carrier's `llama_goose_*` design intact.

Its own step, in this order:

1. add that function to the reader owner;
2. compile the reader owner alone and check the symbol is defined - seconds, the cheap pre-flight
   rule 38 asks for;
3. compile `llama-impl.cpp` alone and check the reference resolves;
4. only then a full build, and then the P9-03 acceptance run.

Until step 4 exists, `v0.10.0-exp1476` remains the current binary and no claim is made about
building from `main`.
