# EXP-2026-11-18-960: top-n-sigma, in Goose

Status: differential PASS, 17112 checks, bit-identical mask. Ported and gated;
not in the shipping chain, which needs a generation run.

## Hypothesis fixed before changes

`P6-10` still lists XTC, DRY, mirostat v1/v2 and top-n-sigma as native. All are
decisions over a candidate array, so none needs a model to port. Top-n-sigma was
chosen because it is the one that **masks rather than reorders**, and because its
boundary is a strict compare against `max - n*std` in `float` arithmetic - the
kind of thing that silently diverges if the port uses `double`.

## What was done

`goose/misc/sampler-top-n-sigma.goose` owns `tns_apply` and `tns_is_empty`,
reproducing `llama_sampler_top_n_sigma_apply` (src/llama-sampler.cpp:3325-3357):

- `n <= 0` or a candidate count `<= 1` is the empty sampler;
- `mx` is seeded from candidate 0 and then maxed, so a leading `-INFINITY`
  participates and does not poison the max - native's `std::max` does the same;
- mean and deviation skip `-INFINITY` candidates; with none valid, mean and std
  are 0;
- std is `sqrt(acc / valid_count)`, the population deviation;
- a candidate is masked when `logit < max - n*std`, strict;
- masked candidates become the most negative finite float, and the array keeps
  its order.

Everything is `f32` because native is `float` throughout. The gate compares
**bit patterns**, not values.

## Result

| check | result |
| --- | --- |
| `goose check` | ok, 62 specializations |
| gate | **17112 checks, 0 failures** |
| random cases | 300 with n 2..64 and n-sigma 0.1..4.0 |
| tie cases | 200 quantised to 0.25 steps, where std rounding decides the mask |
| shapes | ascending, descending, all-equal, pair, single, `n=0`, wide spread, huge n, leading `-INFINITY`, all `-INFINITY` |
| masked candidates | compared as a decision, then the surviving values bit for bit |

## A wrong assertion, and the native behaviour behind it

The first run had one failure: I asserted that an all-`-INFINITY` list keeps zero
survivors. It reports **two**, and that is correct: native seeds `mx` from
candidate 0 (`-INFINITY`), computes `cut = -INFINITY`, and the strict
`logit < cut` is false for every element, so nothing is re-masked and the function
returns normally. The module already matched the library; my assertion did not.

I briefly "fixed" the module instead of the test - adding a sentinel check that
would have *changed* the behaviour - then reverted it and corrected the test,
because the library is the authority here, not my expectation. The comment in the
module now states this explicitly so the next reader does not re-derive it.

## Not claimed

Not wired into the shipping chain: that needs a generation run to accept, so
`P6-10` stays PARTIAL at 1/4. XTC, DRY and mirostat v1/v2 remain native.

No model was started; every array is generated in the gate.

## Reproduction

    OUT=/home/end0/AI/LlamaServer/exp960-topnsigma-20261118
    python3 tools/compose-goose.py "$OUT/unit.goose" goose/misc/sampler-top-n-sigma.goose
    <goose> check "$OUT/unit.goose"
    <goose> --standalone -o "$OUT/runtime.c" --header "$OUT/runtime.h" "$OUT/unit.goose"
    gcc-13 -O1 -fPIC -shared -Wl,-z,defs "$OUT/runtime.c" -lm -o "$OUT/libgoose.so"
    g++-13 -O1 -I"$OUT" experiments/EXP-2026-11-18-960-goose-top-n-sigma/tests/gate.cpp \
        "$OUT/libgoose.so" -Wl,-rpath,"$OUT" -lm -o "$OUT/gate"
    "$OUT/gate"
