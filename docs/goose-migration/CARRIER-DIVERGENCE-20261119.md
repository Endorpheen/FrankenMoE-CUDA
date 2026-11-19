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

## The evidence from the built objects

The failed build's own objects say exactly what is missing, which is better than reading two source
files: `nm -u` over `core/*.o` and `core/vendor-models/*.o` lists the references the link has to
satisfy. Of the vendor-only symbols, these are actually needed:

```
fg_port_array_str        2 references
fg_bpe_encode           1
fg_port_array_int       1
fg_sampler_adaptive_p_call, _dist_pick_call, _dist_words_call, _dry_call, _mirostat_call,
fg_sampler_penalties_call, _temp_ext_call, _top_p_call, _typical_call   1 each
```

So the gap is thirteen symbols, not one, and ten of them are the sampler door wrappers.

## The smallest correct step

`llama-impl.cpp` exists **only** in the vendor tree (there is no staged copy) and calls
`fg_port_array_str` twice. The staged tree serves the same question through
`llama_goose_arr_str(ctx, index, element)`.

**Done and verified** for the first of the thirteen: `fg_port_array_str(key_index, element, ptr,
len)` now exists in `integration/goose-standalone/gguf-reader-owner.cpp`, the file that owns the
mapping, answering through the `gr_arr_str_span` door from the current registration. Checked in
the cheap way rule 38 asks for:

```
g++-13 -O1 -c integration/goose-standalone/gguf-reader-owner.cpp   -> 0 errors, 0.46 s
nm -g --defined-only owner.o | grep fg_port_array_str             -> T (defined)
g++-13 -O1 -c vendor/flint/src/llama-impl.cpp                     -> 0 errors, 0.38 s
nm -u impl.o | grep fg_port_array_str                             -> U (satisfied by the owner)
```

**Still missing, and not guessed at:** the other twelve - `fg_bpe_encode`, `fg_port_array_int`,
and the ten `fg_sampler_*_call` wrappers. Each is defined in the vendor copy of a carrier whose
staged copy does not have it, and each must be either taken into the staged carrier or provided by
an adapter, then checked the same way: compile the one TU, `nm` the symbol. That is thirteen small
verified steps, not one long build.

Until all thirteen resolve, the build stays refused for those carriers, `v0.10.0-exp1476` remains
the current binary, and no claim is made about building from `main`.
