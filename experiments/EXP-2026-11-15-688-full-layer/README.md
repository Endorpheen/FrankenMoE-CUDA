# EXP-2026-11-15-688 — one Apex attention layer, wired end to end

Base: `161f43c8`. No model run, no GPU. Reads the model file for extents only.

## 1. The result

```
FG_APEX_LAYER nodes=15 K=2048 H=16 HKV=2 HD=256 HDKV=256 out=[2048 1]
              shape_ok=1 status=0,0 nonfinite=0 changed=2048 nondet=0 PASS
```

Fifteen nodes, built by `gb_common` plus the QG split module, executed on the CPU
backend as one graph at the model's own geometry:

```
x -> rms_norm -> wq -> QG split -> q, gate
              -> wk -> view -> k   -> rope
              -> wv -> view -> v
rope(q) x rope(k) x v -> flash_attn -> * sigmoid(gate) -> wo -> + x
```

`changed=2048` means every output element differs from the residual input, so the
graph is not a pass-through. `nondet=0` means two runs of the same graph are
bit-identical. `nonfinite=0` means no NaN or inf anywhere.

## 2. Four things the model corrected, each caught by an assertion

Every one of these would have produced a graph that runs and computes the wrong
thing:

1. **`n_embd_head` is 256, not 512.** The q projection emits two head widths per
   head (query and gate interleaved), so it is `wq_n/(2*n_head)`. Using `wq_n/n_head`
   made the split test agree with itself while describing a layout the model does
   not have.
2. **The gate is `sigmoid`, not `silu`.** `qwen35moe.cpp:352` uses `ggml_sigmoid`.
3. **The gate needs a reshape before the multiply.** The attention result is
   `[HD, 1, H, 1]` while the gate view is `[HD, H, 1, 1]`; ggml's `mul` asserts the
   shapes match, and the gate is reshaped to the attention layout (same 4096
   elements).
4. **mode 26's argument order.** `arg7` is the mask pointer, `arg8` the scale. Passing
   the scale in `arg7` made the module treat a float bit pattern as a tensor pointer
   and crashed in `fg158_prop` — a loud failure, which is the point of the binding
   reading handles rather than guessing.

## 3. What the gate does and does not prove

**Proves**: the fifteen nodes are wired in the right order with the right shapes, the
graph executes, the output is finite, it is not the input, and it is deterministic.
Each node's arithmetic is proven bit-exact against the substrate in EXP-681/685.

**Does not prove**: that the layer matches the reference implementation numerically.
That needs a differential run against llama.cpp's own layer on the same weights,
which is the next meaningful check and does not require a model run if the layer is
fed identical inputs on both sides.

## 4. Where this leaves the runtime

| layer | state |
|---|---|
| one attention layer, 15 nodes | **works**, deterministic |
| one recurrent layer | the GDN nodes are proven individually; not yet wired |
| 42-layer stack | not built |
| KV buffers, prefill, decode | not built |
| CUDA | not built |
| tokenizer, HTTP | not built |

## 5. Cost

One arch-unit build (for `qg_split`), then gate compiles only. No model was started.

## 6. Reproduce

```
./builds/goose-standalone/apex_layer <apex-model.gguf>
```
