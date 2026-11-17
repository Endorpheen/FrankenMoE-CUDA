# EXP-2026-11-17-922: real text generation through the Goose runtime

Status: in progress.
Base: EXP921, e5548997.

## Hypothesis fixed before changes

A single composed Goose unit — GGUF reader, tokenizer, detokenizer, sampler,
Qwen3.5 trunk and KV/recurrent state — can drive a real generation loop on the
actual Apex weights with no native tokenizer, sampler or piece copy in the path,
and reproduce native llama.cpp exactly when both run the same profile.

Path under test:

    prompt text
      -> Goose QWEN35 pre-tokenizer + BPE (goose/model/bpe-tokenize.goose)
      -> Goose trunk graph (goose/model/qwen35-trunk.goose) executed by the
         generic GGML CPU executor
      -> Goose sampler (goose/sampling/sampler-chain-full.goose)
      -> Goose token_to_piece
      -> stop on the Goose end-of-generation set

The native reference is llama.cpp's own tokenizer, decoder and greedy choice on
the same GGUF with the same KV type and the same attention: Q4_0 keys and
values, flash attention enabled, CPU, no GPU, no server.

## Fixed contract

- The Goose unit is composed from the accepted modules only; the fixture calls
  its exports and nothing else for the model, vocabulary and sampling.
- The mapping is borrowed (EXP919 boundary); no weights are copied.
- Greedy choice is the same rule in both: the first candidate with a strictly
  larger logit.
- Full id equality, then full text equality, then the same end-of-generation
  behaviour.

## Results

### Profile matters: the first native control disagreed for a real reason

The first native control used default context parameters (F32 KV, attention
auto) and produced ` Paris. The capital of Spain is Madrid` — a different
continuation from the Goose path's ` Paris, a city rich in history and`. The
cause was the profile, not the port: the accepted Apex profile runs Q4_0 KV with
flash attention, and the Goose fixture runs exactly that. With `type_k =
type_v = Q4_0` and flash attention enabled, the native control produces the
same ids.

The first native harness also decoded with a position-less batch
(`llama_batch_get_one`), which on this hybrid model re-read the prompt instead of
continuing it; positions and sequence ids are now explicit.

### 8 greedy tokens, both paths identical

    prompt ids: 760 6511 314 9338 369        (both)
    tokens:     11751 11 264 3177 8807 303 3712 321   (both)
    text:       " Paris, a city rich in history and"    (both)

### Boundaries

CPU, mmap, batch 1, no GPU and no server; this is a functional gate, not a
speed measurement. The prompt is raw text, not a chat template. Only the greedy
choice is compared here; the stochastic chain is compared separately.

## Results: end-of-generation and the full comparison

Chat-marker prompt, greedy, 24 tokens maximum:

    prompt ids: 248045 846 24127 15131 13 248046 248045 74455 220   (both)
    generated:  16 198 248069 271 12675 1017 0 59720 233 2500 628 353 1438 488 3242 30 248046
    text:       "1\n</think>\n\nHi there! 👋 How can I help you today?<|im_end|>"
    stop:       eog id 248046 at step 16                             (both)

`tests/compare.py` reports RESULT PASS on prompt ids, generated ids, text and
stop position. The `<|im_end|>` stop is the Goose end-of-generation set, checked
before the next trunk step; the piece is still rendered, exactly as the native
detokeniser renders it.

A 32-token greedy run with the plain prompt produced continuous, coherent text
(` Paris, a city rich in history and culture. Established in the 9th century,
Paris grew from a small settlement on the Seine River to become one of`) and did
not stop, which is the model's choice, not a tokenizer decision.

## Correction: the ids agree, the logits do not

The id and text equality above is real, but it is NOT evidence that the trunk is
numerically faithful to llama.cpp, and this experiment found that it is not.
Dumping the last-position logits of the same prompt from both paths:

| comparison | mean abs diff | max abs diff |
| --- | ---: | ---: |
| one-token prompt "Hi", Goose vs native (Q4_0 KV) | 0.1068 | 0.7492 |
| one-token prompt "Hi", Goose vs native (F32 KV) | 0.0989 | 0.6374 |
| one-token prompt, native Q4_0 KV vs native F32 KV | 0.0769 | 0.5541 |
| multi-token prompt, Goose vs native (Q4_0 KV) | 0.2010 | 1.3137 |

Bitwise-equal logits: 0 of 248320 in every comparison.

The third row is the whole point: native's own KV-quantisation effect is smaller
than the Goose-vs-native gap, and Goose is not closer to native's Q4_0 run than
to native's F32 run - so the difference is not just "KV is quantised". Something
in the Goose trunk's arithmetic does not match llama.cpp's, and the greeding
token agreement is robustness of a confident model, not numerical equality.

Consequences, stated plainly:

- EXP918's "bitwise equal" gate compared the Goose trunk against a hand-written
  native graph inside the same fixture, not against llama.cpp's own model graph.
  Both can agree with each other and still differ from llama.cpp.
- The stochastic chain cannot be compared end to end until the logits agree,
  because any difference changes the probabilities.
- The end-to-end id/text/EOS agreement stands as a functional result for these
  prompts only.

Isolation continues in EXP-2026-11-17-923.

## What this proves and what it does not

Proves: on the real Apex weights, the composed Goose unit tokenises, decodes,
samples, detokenises and stops like native llama.cpp on the tested greedy runs;
the trunk path carries KV and recurrent state across steps; the end-of-generation
set works.

Does not prove: numerical equality of the trunk (see the correction above),
stochastic equality, or that the shipping server uses these modules.

Does not prove: that the shipping server uses these modules (it still does not),
grammar/chat-template handling, the stochastic chain end to end, batch > 1, GPU,
or any speed figure. TTFT, prefill and decode rates were not measured and are
not claimed.
