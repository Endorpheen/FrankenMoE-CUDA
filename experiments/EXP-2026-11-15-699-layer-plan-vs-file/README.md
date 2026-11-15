# EXP-2026-11-15-699 — the layer plan against the file: an off-by-one corrected

Base: `9711f20d`. No model run, no GPU.

## 1. Result

```
layers:  1 1 1 0 1 1 1 0 ... 1 1 1 0 2
FG_GDN_PLAN      n=41 attn=10 recr=30 mtp=1 bad=0 | expected 10/30/1 PASS
FG_GDN_PLANFILE  layers=41 mismatch=0 missing_both=0 mtp=1 last_kind=2 PASS
```

## 2. The bug

EXP-693 reported 10 attention / 31 recurrent / 1 MTP over 42 layers and called it PASS,
because the gate recomputed the interval rule and agreed with it. Both were wrong the same
way.

The GGUF has `block_count = 41` and `nextn_predict_layers = 1`. `blk.40` carries **both**
the regular block tensors and `nextn.eh_proj` / `nextn.enorm` / `nextn.hnorm` /
`nextn.shared_head_norm` - it is the MTP block, stored at index `n_layer`. So

```
n_layer     = block_count - nextn = 40
n_layer_all = block_count         = 41
```

Passing `block_count` as `n_layer` made every layer past the last interval boundary shift
by one, and turned the MTP block into an ordinary recurrent layer.

The correct distribution is **10 attention (3, 7, ..., 39), 30 recurrent, 1 MTP at index
40** - which is exactly what the file says.

## 3. Why the new gate caught it and the old one could not

EXP-693's gate recomputed the same interval formula the module uses. Two copies of one
rule agreeing is not evidence - it is the same failure as EXP-688b, where a layer missing
two real steps still compared bit-exactly against itself.

This gate asks the file instead. A recurrent layer carries `blk.N.attn_qkv.weight`; a
full-attention layer carries `blk.N.attn_q.weight` (qwen35moe.cpp:87-88 and 47-95). Those
tensors exist or do not, independently of any interval arithmetic. Layer 40 was the only
one that disagreed, and it is the one that mattered.

The MTP layer is dense attention-only (qwen35moe.cpp:22-23), so its file signature is the
attention one; the gate accepts that for kind 2 rather than inventing a third signature.

## 4. What this changes

`gdn_plan.c` and `gdn_planfile.c` both derive `n_layer` as block_count minus nextn. The
EXP-693 card is marked corrected in place rather than quietly rewritten.

Any stack built on the old numbers would have used the recurrent builder for the MTP
layer, which has no `attn_qkv` tensor at all - it would have failed at load, or worse,
loaded the attention weights into a recurrent path.

## 5. Cost

Gate compiles only. No unit rebuild.
