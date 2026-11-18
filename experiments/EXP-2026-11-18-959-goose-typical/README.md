# EXP-2026-11-18-959: locally typical sampling, in Goose

Status: differential PASS, 2535 checks. The filter is ported and gated; it is not
yet in the shipping chain, which needs a model run to accept.

## Hypothesis fixed before changes

`P6-10` lists unsorted top-p, typical, XTC, DRY, mirostat and top-n-sigma as
open. All of them are pure decisions over a candidate array, so none needs a
model, a socket or an RNG to port - only the chain around them does. Typical was
chosen first because it has the most subtle ordering rule: a score sort followed
by a cumulative cut with a `min_keep` floor.

## What was done

`goose/misc/sampler-typical.goose` owns `tp_apply` and `tp_is_empty`. It
reproduces `llama_sampler_typical_apply` (src/llama-sampler.cpp:1990-2043):

- `p >= 1.0` is the empty sampler: nothing is filtered, the identity order is
  returned so a caller can chain unconditionally;
- entropy is `sum(-p ln p)` over the whole array, before any cut;
- each score is `|(-ln p) - entropy|`, sorted ascending;
- the cut keeps candidates until the running sum exceeds `p` **and** at least
  `min_keep` are kept;
- a zero-probability candidate scores `+inf` and sorts last, matching
  `fabsf(-logf(0))`;
- survivors stay in score order.

Ties are settled by index through an insertion sort. `std::sort` does not promise
that ordering; this module does, so its output is at least as deterministic as
the native one.

One finding worth recording: **Goose has no built-in `log`, `exp`, `fabs`, `sqrt`
or `floor`** - all six report `unknown function`. The shipped unit already
declares `extern "exp" fn exp64(x: f64)` for perplexity
(`build/goose-main/main.goose:17157`), so this module follows that pattern with
`extern "log"` and `extern "fabs"`. Any other port that needs arithmetic on
reals will hit the same wall.

## Result

| check | result |
| --- | --- |
| `goose check` | ok, 62 specializations |
| gate | **2535 checks, 0 failures** |
| random distributions | 200, seeded, n 1..64, p in [0.05, 0.99], min_keep 0..3 |
| shapes covered | flat, peaked, single, pair, zero-probability, `min_keep` 0..3, `p` 1.0 and 2.0 |
| bad arguments | `n` past the array, negative `min_keep`, short `keep` - all refused |

The oracle is a transcription of the native decision on the same probabilities,
using `std::stable_sort` so the comparison is not decided by sort instability.
Softmax is supplied by the caller in both, because the chain owns it; this gate
tests entropy, the score sort, the cut and the survivors' order.

## Not claimed

The filter is not in the shipping sampler chain: wiring it needs a generation run
to accept, which needs Igor's approval. So `P6-10` stays PARTIAL at 1/4. The other
listed samplers (top-p unsorted path, XTC, DRY, mirostat v1/v2, top-n-sigma) are
still native.

No model was started; the arrays are generated in the gate.

## Reproduction

    OUT=/home/end0/AI/LlamaServer/exp959-typical-20261118
    python3 tools/compose-goose.py "$OUT/unit.goose" goose/misc/sampler-typical.goose
    <goose> check "$OUT/unit.goose"
    <goose> --standalone -o "$OUT/runtime.c" --header "$OUT/runtime.h" "$OUT/unit.goose"
    gcc-13 -O1 -fPIC -shared -Wl,-z,defs "$OUT/runtime.c" -lm -o "$OUT/libgoose.so"
    g++-13 -O1 -I"$OUT" experiments/EXP-2026-11-18-959-goose-typical/tests/gate.cpp \
        "$OUT/libgoose.so" -Wl,-rpath,"$OUT" -lm -o "$OUT/gate"
    "$OUT/gate"
