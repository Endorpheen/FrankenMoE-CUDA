# EXP-2026-11-14-583 — LoRA manifest and pairing (P6-09 S1)

Native counterpart: `llama_adapter_lora_init_impl()` in `src/llama-adapter.cpp`
(metadata gate, tensor suffix scan, pair table, shape rules, data bounds) and
`llama_adapter_lora_weight::get_scale` in `src/llama-adapter.h`.
Base commit: `a640bac9` (repository head moved on to `d8fe00ad` while this
experiment was prepared in an isolated directory; no repository file was
touched).

## What this slice covers

`source/lora-manifest.goose` owns the pure decisions of the LoRA loader. It
never opens a file and never touches a backend; the GGUF reader stays native
and is another slice.

- metadata gate in native order: `general.type == "adapter"`,
  `general.architecture` equal to the base model arch, `adapter.type == "lora"`,
  `adapter.lora.alpha` (0.0 when absent), `adapter.alora.invocation_tokens`
  (optional array of `u32`);
- tensor naming: `.lora_a` / `.lora_b` pairing under a base name with **every**
  occurrence of the suffix removed (`replace_all` semantics), `_norm.weight`
  skipped, any other suffix is a hard error on the first tensor in GGUF order;
- the pair table is a sorted map, so a second `.lora_a` for one base name
  replaces the first and validation walks the names in byte order;
- per-pair validation: missing component, absent from the base model,
  Hadamard-folded target, shape. `token_embd.weight` uses the flipped rule
  (`model.ne0 == b.ne1`, `model.ne1 == a.ne1`), every other name the straight
  rule (`model.ne0 == a.ne0`, `model.ne1 == b.ne1`) plus `a.ne1 == b.ne0` for
  the transpose check;
- data bounds: `offs = data_offset + tensor_offset`, `offs + nbytes` must stay
  inside the file and must not wrap;
- scale: `alpha == 0` keeps the caller scale, otherwise `scale * alpha / rank`
  with `rank = b.ne[0]`.

Flat layouts, no structs, no allocation: the caller passes a decoded GGUF view
(key/value records plus a byte pool), the tensor and base-model tables, and the
pair/arena buffers. Return codes are documented at the top of the source file.

## Gate

The native oracle is the real loader, not a transcription: the fixture writes a
synthetic adapter with the GGUF writer and calls
`llama_adapter_lora_init_from_file_ptr()`, which runs
`llama_adapter_lora_init_impl()` unchanged. No base-model weights are loaded.

`llama_model` is abstract in this tree (seven pure virtuals), so `TestModel`
implements them as unused stubs and carries only the fields this path reads:
`arch`, `tensors_by_name` (F32 tensors in a CPU backend buffer) and
`hdmd.weight_blocks` for the Hadamard case. The adapter tensors are generated
with a per-name payload, and the gate reads them back after the load, so a
swapped A/B pair or a wrong data copy cannot pass unnoticed.

For every case the gate compares the native outcome with the port outcome:
error class, alpha bits, aLoRA tokens, pair count, the A/B tensor name of every
pair, the `get_scale`/`lora_scale` value, and the name quoted by the native
error (which proves the sorted validation order).

```
FG583_LORA cases=26 values=139 failures=0
```

Cases 1–6 and 19–20, 23 are accepted adapters (alpha present/absent/zero,
flipped `token_embd` layout, aLoRA token list including ids above 2^31-1,
`_norm.weight` skipping, a name containing `.lora_a` twice, an empty adapter,
three pairs). Cases 7–18 and 21–22, 24–26 are rejections covering every code
from `-2` to `-12`, both ordering effects (suffix scan before the pair loop,
sorted pair order) and the error name.

The `contract` block covers inputs the native loader cannot accept: a too small
token buffer, a too small pair arena and inconsistent view lengths (`-14`), a
metadata value of the wrong GGUF type
(`-15`, native `GGML_ASSERT`), the bounds rule with a short file and with an
`offs + nbytes` wrap (`-13`), and both scale branches.

## Mutations

`tests/mutations.json` holds 12 mutations in two halves: A is the metadata gate,
B is naming, pairing and shape. Each mutation must still compile (`goose check`)
and must then fail the gate.

```
python3 tests/mutate.py --half A   # 6/6 caught
python3 tests/mutate.py --half B   # 6/6 caught
python3 tests/mutate.py            # 12/12 caught
```

## Recorded limits

- The GGUF reader is native and is not ported here. The port consumes a decoded
  key/value view, so the byte layout of the file (header, alignment, tensor
  offsets) is outside the gate; the fixture fills that view from the file it
  wrote so the offsets are real.
- The arch check compares the canonical arch name byte for byte. The native
  code parses the string into `llm_arch` and compares enums; the port does not
  own the arch table, and the two are equivalent for canonical names only.
- Metadata of the wrong GGUF type is `-15` in the port, while native
  `gguf_get_val_str` / `gguf_get_val_f32` assert and kill the process. The gate
  compares nothing for that input.
- Duplicate tensor names cannot be produced in a GGUF file: the writer aborts
  and the reader rejects the file, so the native `ab_map[name].a = cur`
  overwrite branch is unreachable through a real adapter. The port keeps the
  rule and the contract block checks it directly (port-only).
- Tensor data copying, buffer allocation, the `ggml_dup_tensor` calls and the
  adapter registration in `model.loras` are not ported: they belong to S2
  (application) and to the loader. The native side of this gate does execute
  them, which is what makes the A/B data check possible.
- The base-model tensor lookup is a linear scan in the port, as native
  `get_tensor` is; both return the first match. Base-model tensor tables with
  duplicate names are not part of any GGUF model and are not tested.
