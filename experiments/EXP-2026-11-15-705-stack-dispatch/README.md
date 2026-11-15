# EXP-2026-11-15-705 — the stack loop dispatches the right builder per layer

Base: `9facfc7b`. No model run, no GPU.

## 1. Result

```
FG_STACK_DISPATCH n=41 attn=10 recr=30 mtp=1 first_attn=3 last_attn=39
                  pos_bad=0 shifted_differs=20 bad=0 PASS
```

## 2. What this adds to EXP-699

EXP-699 established the plan itself - 10 attention, 30 recurrent, 1 MTP - against the file's
tensor signatures. It did not check the loop that consumes the plan. A plan can be right
while the walk over it is shifted, and every layer the walk builds would still be correct.

The audit here is **positional**, not just by total. The attention layers must sit at 3, 7,
11, ... 39 - the multiples of the interval counted from 1. If the loop had tested `il`
instead of `il + 1`, attention would land on 0, 4, 8, ... and the *count would still be 10*.
A total-only check would pass that.

`pos_bad=0` is that positional check: every one of the 40 ordinary layers has the kind the
interval rule puts there.

## 3. The shifted plan must be distinguishable

A second plan shifted by one is walked alongside. `shifted_differs=20` counts the layers
where the two plans select different builders - so the audit is sensitive to exactly the
off-by-one it exists to catch. If that number were 0, the comparison would be blind and the
PASS would mean nothing.

## 4. The MTP position is a constraint, not a count

The loop asserts that kind 2 appears only as the final layer. MTP sits above `n_layer` by
construction (`n_layer_all = n_layer + n_nextn`), so an MTP kind anywhere else means the
plan and the walk disagree about where the boundary is - which is the bug EXP-699 found in
the other direction.

## 5. What this does not prove

That the 41 layers, each built with its real builder over real weights, produce the
reference tokens. It proves the walk: every layer gets the builder its kind requires, at the
position its kind requires.

## 6. Cost

Gate compiles only. No unit rebuild.
