# EXP-2026-11-18-966: the Goose typical filter agrees with the live sampler

Status: differential PASS through the public sampler API, 202 checks.

## Hypothesis fixed before changes

EXP959 compared `tp_apply` against a hand-written transcription of the native
function, feeding probabilities directly. That answers "is the maths the same" but
not "does it agree with the sampler the runtime actually constructs". Those differ:
the native sampler owns its own softmax, sorts the candidate array descending
first, rewrites `cur_p` in place and sets `sorted = false`.

This experiment builds real `llama_sampler` objects and real candidate arrays, so
the comparison runs through the same public entry points the server uses. It is
the closest thing to "wired" that is possible without a model: no context, no
GPU, no weights, but a genuine sampler instance.

## What was done

200 seeded cases, each with 2..48 candidates, a random `p` in 0.1..0.95 and a
random `min_keep` in 0..2:

1. build `llama_token_data` with logits;
2. native: `llama_sampler_init_typical(p, min_keep)`, then `llama_sampler_apply`,
   then read the surviving ids out of the rewritten array;
3. Goose: compute the same softmax (`exp(logit - max)` normalised) and call
   `tp_apply`;
4. compare the **sets** of surviving ids.

Sets, not sequences, and the reason is in the code: the native sort is
`std::sort` on a score array, which is unstable, while this module breaks ties by
index. Two implementations can therefore disagree on the order inside a tie and
both be correct. The surviving set is what has to match, and it does in all 200
cases.

## Result

| check | result |
| --- | --- |
| cases | **202 checks, 0 failures** |
| random trials | 200 |
| sampler under test | the real `llama_sampler_init_typical` from the accepted build |
| `p = 1.0` | native keeps 4 of 4; `tp_apply` keeps 4 of 4 |
| witness | `FG384S_SOFTMAX` fired 201 times, so the chain path really ran |

The softmax witness matters: it shows the native side did not quietly skip its
own normalisation, which is what would have made the comparison trivial.

## Not claimed

The shipping chain still calls the native sampler. Wiring `tp_apply` into
`llama_sampler_chain` means replacing a `llama_sampler_i` implementation, and that
acceptance needs a generation run to compare output - which needs Igor's approval.
So P6-10 stays PARTIAL.

What this experiment removes is a doubt, not a step: it shows that the ported
filter produces the same decisions as the live sampler on the same inputs, so the
wiring is a mechanical change rather than a behavioural risk.

No model was started.

## Reproduction

    OUT=/home/end0/AI/LlamaServer/exp966-typical-live-20261118
    python3 tools/compose-goose.py "$OUT/unit.goose" goose/misc/sampler-typical.goose
    <goose> --standalone -o "$OUT/runtime.c" --header "$OUT/runtime.h" "$OUT/unit.goose"
    gcc-13 -O1 -fPIC -shared -Wl,-z,defs "$OUT/runtime.c" -lm -o "$OUT/libgoose.so"
    g++-13 -O1 -I"$OUT" -Ivendor/flint/include -Ivendor/flint/ggml/include \
        experiments/EXP-2026-11-18-966-goose-typical-live/tests/gate.cpp \
        "$OUT/libgoose.so" <accepted>/libllama.so.0 <accepted>/libggml*.so.0 -o "$OUT/gate"
    "$OUT/gate"
