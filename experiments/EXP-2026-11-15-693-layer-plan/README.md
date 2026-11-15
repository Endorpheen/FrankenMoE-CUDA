# EXP-2026-11-15-693 — the type of every one of the 42 layers, decided by Goose

Base: `40894016`. No model run, no GPU.

## 1. Result

```
layers: 1 1 1 0 1 1 1 0 ... 1 1 1 0 1 2
FG_GDN_PLAN n=42 attn=10 recr=31 mtp=1 bad=0 | expected 10/31/1 PASS
```

`q35_plan` fills the kind of all 42 layers from the parameters read out of the GGUF:
10 full-attention, 31 recurrent, 1 MTP. That is the input the stack needs before a
single node of it can be built.

## 2. Where the rule comes from

`qwen35moe.cpp:22-28`:

```c
if (!ml.get_key_or_arr(LLM_KV_ATTENTION_RECURRENT_LAYERS, hparams.is_recr_impl,
                       hparams.n_layer_all, false)) {
    for (...) is_recr_impl[i] = (i < hparams.n_layer())
        && ((i + 1) % full_attn_interval != 0);
}
```

This GGUF carries no per-layer recurrent array, so the interval formula applies.
`block_count` is 41 and `nextn_predict_layers` is 1, so 42 layers in total, of which
the last is MTP.

## 3. Why the gate recomputes the rule itself

The gate does not take the module's word for it. It recomputes

```c
if (i >= n_layer) mtp++;  else if (interval > 0 && (i + 1) % interval == 0) attn++;  else recr++;
```

and compares the two distributions and the total. A gate that only checks "the module
returned something" would pass on a formula that shifted every boundary by one - which
is the exact failure mode here, since an off-by-one at `(i + 1) % interval` would move
every attention layer into a recurrent one and the count would still be plausible.

## 4. Parameters, read rather than assumed

| key | value | used as |
|---|---|---|
| `block_count` | 41 | `n_layer` |
| `nextn_predict_layers` | 1 | adds the MTP layer |
| `full_attention_interval` | 4 | the interval rule |
| `ssm.conv_kernel` | 4 | recurrent conv |
| `ssm.state_size` | 128 | SSM state width |
| `ssm.group_count` | 16 | recurrent groups |
| `ssm.time_step_rank` | 32 | dt rank |
| `ssm.inner_size` | 4096 | recurrent inner width |
| `expert_count` / `expert_used_count` | 256 / 8 | MoE |

## 5. What this does not prove

That the stack builds, or that a layer sequence produces the reference tokens. It
proves the dispatch input - without which every subsequent graph would be wrong - is
correct and comes from Goose.

## 6. Cost

Gate compile only. No unit rebuild: `q35_plan` and `q35_kind` were already exported.
