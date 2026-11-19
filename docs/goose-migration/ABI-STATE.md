# ABI state, and the two things P10-08 asked for

## Compatibility, as measured now

| check | result | what it means |
|---|---|---|
| exported symbols against the newest accepted checkpoint | 4551 before, 4594 after, **0 lost** | nothing the project accepted has disappeared from the shipped library |
| consumer/provider signatures (`tools/check-abi-pairs.py`) | `fg386s_pick` agrees on five parameters, `fg386s_enabled` on none | the doors the host calls by signature still match |
| undefined symbols (`tools/check-unresolved-symbols.sh`) | ok | the library loads |
| SONAMEs | `libllama.so.0`, `libllama-common.so.0`, `libllama-server-impl.so`, `libggml.so.0`, `libmtmd.so.0` | as recorded in EDGE0-120B-ADAPTATION.md |

## The SONAME item, decided

`libllama-server-impl.so` is the one library whose SONAME carries **no version suffix**, while its siblings do. That is the
mismatch the item refers to. The decision is to **keep it**: it is the name every existing checkpoint, release archive and
`ldd` record uses, and versioning it now would break the ability to restore them - which rule 34 forbids for a cosmetic
change. It becomes part of the naming migration instead: when `libllama-server-impl.so` is renamed to `libgoose-server.so`,
a versioned SONAME is added **as an alias**, with the unversioned name kept working.

## The gate that was red, and why

`tools/check-no-symbol-loss.py` reported two missing symbols:

```
LOST _ZN18llama_model_loader19llama_tensor_weightC1EPK10llama_filetPK12gguf_contextP11ggml_tensor
LOST _ZN18llama_model_loader19llama_tensor_weightC2EPK10llama_filetPK12gguf_contextP11ggml_tensor
```

They are the two constructors of an **internal** class, whose signature gained a `ggml_tensor *` in the vendoring commit of
2026-11-16. The gate's default baseline was a checkpoint **older** than that commit, so it had been comparing the present
with a superseded past.

A symbol diff only means something against the build the project last accepted, so the default is now the
**newest checkpoint by modification time** (by name it picked `chain`, which sorts after `EXP1412` in ASCII and is older).
Against that baseline the result is `0 lost`.

## What P10-08's "host wiring" means now

The header and enum slices the item mentions were verified in EXP646 (43 type-table entries, 187 public constants, the name
tables). Wiring them into the runtime would replace native constants with port lookups - a change with no effect on
compatibility, which is what the item's acceptance asks for. It is deferred to the naming stage under rule 34, where the
public surface changes anyway and both names must work at once.
