# EXP-2026-11-18-961: DRY repetition penalty, in Goose

Status: differential PASS, 2440 checks, bit-identical logits. Ported and gated;
not in the shipping chain, which needs a generation run.

## Hypothesis fixed before changes

XTC was the next candidate, but it needs an RNG that Goose does not have: there is
no `mt19937`, no `rng_`, nothing in the tree. DRY is the opposite - fully
deterministic, no RNG, no model - so it is the last of the four listed samplers
that can be ported and gated without a run. It is also the largest: four steps and
a reverse Z-algorithm.

## What was done

`goose/misc/sampler-dry.goose` owns `dry_apply` and `dry_is_empty`, reproducing
`llama_sampler_dry_apply` (src/llama-sampler.cpp:3473-3682):

1. **restart sequences** - scan the window from its start for a breaker whose tail
   matches the tokens before position `i`, take the longest, and set
   `rep_limit = i - len`. An empty tail matches trivially at every head. A
   `rep_limit` below `dry_allowed_length` abandons the penalty;
2. **reverse Z-algorithm** - the two standard Z-box cases fill
   `counts[last - k]`, clamped by `rep_limit`;
3. **max repeat per token** - only counts of at least `dry_allowed_length` qualify,
   keyed by the token that would extend the run, largest wins;
4. **penalty** - `multiplier * base^(repeat_len - allowed)`, with the exponent
   clamped to `88.7228391 / ln(base)` when `base > 1.000001`, and a single-token
   breaker never penalised.

Everything is `f32`, because native is `float` throughout and the gate compares
bit patterns.

One language note: `match` is a reserved word in Goose - `var match: bool` fails to
parse with "expression expected, found ':'". The variable is named `hit`.

## Result

| check | result |
| --- | --- |
| `goose check` | ok, 66 specializations |
| gate | **2440 checks, 0 failures** |
| random cases | 300, history 3..40 tokens over 7 symbols, multiplier 0 and 0.5..0.9, base 1.75 and 1000 |
| shapes | the documented `a b c c b c y a b c` example, plain repeat, single-token breaker, two-token breaker, short window, clamped window |
| comparison | logits bit for bit against a `float` transcription |

## Not claimed

Not in the shipping chain: that needs a generation run, so `P6-10` stays PARTIAL
at 1/4. XTC and mirostat v1/v2 remain native, and both are blocked by the missing
RNG - which is now a recorded blocker, not a guess: XTC's `apply` draws from
`std::mt19937`, and Goose has no generator to draw from.

No model was started; every history and logit array is generated in the gate.

## Reproduction

    OUT=/home/end0/AI/LlamaServer/exp961-dry-20261118
    python3 tools/compose-goose.py "$OUT/unit.goose" goose/misc/sampler-dry.goose
    <goose> check "$OUT/unit.goose"
    <goose> --standalone -o "$OUT/runtime.c" --header "$OUT/runtime.h" "$OUT/unit.goose"
    gcc-13 -O1 -fPIC -shared -Wl,-z,defs "$OUT/runtime.c" -lm -o "$OUT/libgoose.so"
    g++-13 -O1 -I"$OUT" experiments/EXP-2026-11-18-961-goose-dry/tests/gate.cpp \
        "$OUT/libgoose.so" -Wl,-rpath,"$OUT" -lm -o "$OUT/gate"
    "$OUT/gate"
