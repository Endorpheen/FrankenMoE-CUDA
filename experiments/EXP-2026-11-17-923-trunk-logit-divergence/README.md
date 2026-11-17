# EXP-2026-11-17-923: why the Goose trunk logits differ from llama.cpp

Status: in progress.
Base: EXP922, 2e68fa94.

## Finding this experiment starts from

EXP922 compared a composed Goose unit against llama.cpp's own tokenizer, model
graph and greedy choice. The token ids and the text agreed on every tested run,
including a stop on `<|im_end|>`, but the logits do not agree:

| comparison (one-token prompt "Hi") | mean abs diff | max abs diff | bitwise equal |
| --- | ---: | ---: | ---: |
| Goose vs native, Q4_0 KV | 0.1068 | 0.7492 | 0 / 248320 |
| Goose vs native, F32 KV | 0.0989 | 0.6374 | 0 / 248320 |
| native Q4_0 KV vs native F32 KV | 0.0769 | 0.5541 | 0 / 248320 |

A multi-token prompt gives mean 0.2010, max 1.3137.

The third row rules out "it is only the KV quantisation": native's own KV effect
is smaller than the Goose-vs-native gap, and Goose is not closer to native's
Q4_0 run than to native's F32 run.

## Hypothesis

The Goose trunk graph is a faithful transcription of what EXP918's fixture
believed the model computes, but it differs from llama.cpp's actual
Qwen3.5-MoE graph in at least one structural decision. Candidates, in the order
they can be tested cheaply:

1. how K/V are written into the Q4_0 cache (which op quantises, and on which
   layout) and how attention reads them back;
2. the attention branch: rope variant and sections, kq scale, mask semantics,
   flash-attention accumulation precision;
3. the GDN (recurrent) layers: convolution state update, gate ordering,
   normalisation placement;
4. the MoE block: router normalisation/scale, shared-expert placement, fused
   versus separate RMS-norm/SwiGLU.

## Method fixed before changes

Isolate by building the smallest comparison that separates one decision at a
time, using llama.cpp as the reference and keeping every measurement on the real
Apex weights:

1. single token, goose vs native, both F32 KV and both Q4_0 KV: already
   measured; the residual is ~0.10 with either, so KV is not the only cause;
2. per-layer isolation: compute the Goose frames for a one-token prompt and
   compare a layer's output against a native reference built from the same
   weights with llama.cpp's construction, one layer at a time, starting with
   layer 0 (a GDN layer);
3. once a layer matches, move to the next; the first divergent layer names the
   subsystem.

Acceptance: bitwise equality of the logits vector for a fixed prompt (or, if a
fused op makes bitwise impossible, equality of the tokens the profile produces
plus a stated, measured tolerance that the chain-level comparison can use).
No tolerance is chosen before the cause is known.

## Boundaries

CPU only, batch 1, mmap, no GPU, no server. The protected Apex bundle, the
baseline and the shipping runtime are untouched. Nothing here is a speed
result.

## Results

Pending.
