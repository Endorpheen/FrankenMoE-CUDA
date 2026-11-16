# Correction: 20/20 passes on real SHAPES, not on real WEIGHT VALUES

This corrects the conclusion I drew one commit earlier. The harnesses pass, and I described them as "correct
and deterministic output" - which is true of what they compute, but not the claim a reader would take from
it.

## What the harnesses actually feed the kernels

`grep` over the three that run layer-sized chains:

```
apex_layer       gguf_get_tensor_data: 0    synthetic pattern: yes
gdn_layer_full   gguf_get_tensor_data: 0    synthetic pattern: yes
apex_graph       gguf_get_tensor_data: 0    synthetic pattern: yes
```

`apex_graph` says it in its own header: *"It does not load the weights and does not start a model: the buffers
are filled with a deterministic pattern."*

So every one of the twenty runs **at the model's real extents, with fabricated values**. That proves shapes,
graph assembly, wiring, determinism and agreement with ggml. It does **not** touch a single Q4_K_M weight.

## The two halves are real but they do not meet

| | reads real bytes | feeds them to a kernel |
|---|---|---|
| `frankenmoe-v4` | **yes** - 21 702 472 192 B, 753 tensors, 0 mismatches | no |
| the 20 harnesses | no | no (synthetic values) |

So the standalone can read the model perfectly and can run kernels correctly, and **nothing joins the two.**
That is precisely what `layer_real` was written for - its header says it "reads the real blk.0 tensors out of
the GGUF and runs the attention chain on them" - and it is the one that fails, on a GQA shape assumption.

I marked `layer_real` superseded by `apex_layer` one commit ago. **That was wrong**: `apex_layer` does not do
the thing `layer_real` was for. `apex_layer` supersedes it on shape coverage, not on real values. The
retraction stands only for the MHA/GQA defect; the capability it was aimed at is still missing.

## What this means for the migration statement

"Every component produces correct output on the real file" is not supported. What is supported:

- the container, metadata, tensor directory and payload are read correctly (0 mismatches over 21.7 GB);
- every kernel chain assembles and runs deterministically at real dimensions and agrees with ggml;
- **dequantisation of a real Q4_K_M block and a matmul over it has never been executed in this runtime.**

That last line is the honest state, and it is a smaller claim than the previous card made.

## The next step this implies, and it is not composition

PASS-COMPOSITION ranked "compose the 41 layers" first. **This correction reorders it: feed one real tensor to
one kernel first.** A single dequantised `blk.3.attn_q.weight` through one `mul_mat`, compared against the
integrated runtime's result for the same tensor, would establish that the value path works at all. Composing
41 layers over synthetic values would prove even less than the twenty already do.

The `layer_real` harness is the right vehicle and needs its GQA reshaping fixed - not replaced.
