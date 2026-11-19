# Replacing the loader's GGUF context with the ported reader

Status: **started** - integral keys are served by the port (EXP-2026-11-19-1340); strings, arrays, tensor
descriptors and the split-file path remain native. Every measurement it depends on exists (below); the edit itself is large enough
that it must be done as a block with its own acceptance, not appended to a session.

## What already exists

| piece | evidence |
|---|---|
| the ported reader, proven against the native one on the real 20 GB file | header, 52 typed keys, 744 231 array elements, 753 tensor names, 753 descriptors, 753 tensor sizes - all equal (EXP-2026-11-19-1327) |
| the reader composed into the shipped unit | 15 `gr_*` doors, including `gr_tensor_info` (EXP-2026-11-19-1337) |
| a call path from the loader | `fg_gguf_check` in `llama_model_loader`, behind `FG_GGUF_DIAG` (EXP-2026-11-19-1335) |
| a load-time gate | 1610 comparisons per start, refusing the load on any disagreement |

## Gate coverage as of 2026-11-19 (745 857 comparisons per start)

```
FG_GGUF_VALUES   integer=26/26
FG_GGUF_STRINGS  keys=13/13
FG_GGUF_ARRAYS   elements=744231/744231
FG_GGUF_TINFO    descriptors=753/753
FG_GGUF_LOOKUP   keys=52/52 tensors=753/753
FG_GGUF_PARSE    nk=52 nt=753 align=32 off=10990944 disagreements=0
```

Every value the loader can read is now compared on the load path - scalars, strings, arrays and tensor descriptors -
so the plan's first risk item is closed for every data class, and each conversion step starts from equality rather
than from hope. The cost is one pass over the window, paid only under `FG_GGUF_DIAG`.

Remaining risks: the split-file path has no port comparison, and a conversion that returns a default on a missing key
must be forbidden rather than tolerated.

## Use-site shadow read (in place)

Beyond the file-level gate, `GKV::get_kv` - the single funnel through which the loader reads every typed key - now
reads the same key through the port and compares, throwing on a disagreement:

```
FG_GGUF_USES compared=16 differed=0
```

So a converted site cannot read the wrong field silently: the comparison happens at the moment the loader uses the
value, which is what the plan asked for and what the file-level gate could not provide.

The crash that this step caused first is recorded in its card: the port records had been moved into a store and three
comparisons still read the moved-from vector, which segfaulted the load. Running the thing is what caught it.

## What has to change

`llama_model_loader` holds `struct gguf_context * metadata` and reads everything from it:

- **17 call sites** of `get_key(...)`, which today resolve a key name to a value through the native context;
- **1** place that uses `gguf_find_tensor` / `gguf_get_tensor_type` for a descriptor field - my earlier count of six
  came from grepping two patterns that matched the same site twice, and the corrected number is recorded here rather
  than left as an overstatement of the work;
- the split-file join, which builds additional contexts per shard;
- `n_kv` / `n_tensors` counters.

The replacement is therefore a **loader port**, not a substitution: the port's records become the store, and each of
those 23 sites reads from the doors instead of the context. That is the unit the rule-33 goal needs - one source of
truth, no parallel implementation.

## Order of work

1. keep the window and the port records alive in the loader after `fg_gguf_check` (state, kv, tensors, slots);
2. add a private `fg_key_int(key)` / `fg_key_string(key)` pair backed by `gr_find_kv` + `gr_meta_int` / `gr_val_span`,
   and a `fg_tensor_desc(idx)` backed by `gr_tensor_info`;
3. convert the 17 `get_key` sites **by kind**: integers first (26 keys are proven equal), then strings, then arrays;
4. convert the 6 descriptor sites;
5. split files last: a shard is a separate context and needs the same treatment, or the split path stays native and is
   recorded as such;
6. remove `gguf_init_from_file` from this path only when the gate has nothing left to compare.

## Arrays are not convertible through GKV alone: the scope, measured

`GKV<ArrayInfo>::getter` returns `(type, length, data)` and a **null** data pointer for string arrays; the consumers
then iterate the elements themselves:

```
llama-vocab.cpp     3 x gguf_get_arr_str     (token list, merges, token types)
llama-impl.cpp      1 x gguf_get_arr_data + gguf_get_arr_str in the same walk
llama-adapter.cpp   2 x gguf_get_arr_data / gguf_get_arr_n
```

So an array conversion is **consumer-side**: each of those three files has to walk the port's spans instead of the
native array. That is why arrays are not the next one-line conversion they look like, and why the `get_key` funnel
cannot carry them.

## Progress

| kind | served by the port |
|---|---|
| integral keys | yes |
| `std::string` keys | yes |
| vocabulary array | yes |
| merge list, token types | yes |
| tensor lookup and type (metadata-only path) | yes |
| two array walks outside the vocabulary, split files | no |
| `const char *` keys | **nothing to convert** - the specialisation is never instantiated (`GKV<const char *>` has no call site), so it is dead template code rather than a native path |

## The gate that guards each step

`FG_GGUF_DIAG=1` already refuses the load on any disagreement, so every conversion step is checked by the load itself:
a site that reads the wrong field fails immediately rather than at generation. After each step:

```
FG_GGUF_DIAG=1 <server> ...        # must load, 1610 comparisons, 0 disagreements
apex-test.sh <tag> 512 4096 4096 4992    # hash c2ff42cc1219, decode within the 60.4-63.5 spread
```

## Risks, in the order they are likely

1. **strings and arrays**: only 13 string keys and 5 arrays are proven element-wise; the loader reads a string
   (`general.architecture`) and arrays (tokenizer tables), so those conversions need their own comparison at the
   moment of use, not only at file level;
2. **split files**: the shard path has no port comparison at all today; converting it is a separate unit;
3. **silent fallback**: any `fg_key_*` returning -1 on a missing key must fall back to nothing rather than to zero -
   a zero default would load a model with silently wrong parameters;
4. **cost**: the port parses the same header the native reader does; the window is already read for the gate, so the
   added cost is the parse itself, which must be measured on load time, not assumed.

## Rollback

The conversion is behind nothing but itself: reverting `llama-model-loader.cpp` restores the native context. The
gate, the doors and the module stay as they are, so a rollback loses no evidence.
