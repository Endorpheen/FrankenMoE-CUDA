# P3-17 failure probe: first results

Built from the real bridge (`EXP-196/source/bridge.c`, which includes its generated Goose code) so the functions under
test are the ones that execute in the backend. No model run needed.

    gcc-13 -O2 -I<EXP-196/source> fixture.c <EXP-196/source>/bridge.c -o p317_fail -lm

## Results

| case | input | plan[0] | verdict |
|---|---|---|---|
| normal | disabled=0, 1 MiB | 1 | OK - allocates |
| disabled, 1 MiB | disabled=1 | 0 | OK - refuses |
| disabled, 0 | disabled=1, size 0 | 0 | OK - refuses |
| **zero size** | disabled=0, size 0 | **1** | **MISMATCH - approves a zero-byte allocation** |
| negative size | size -1 | abort | contract: `ensure(size>=0, code 21)` |
| huge size | 1 TiB | 1 | OK - plans, failure would come from the allocation result |
| fallback | null pointer | 1 | OK |

Final counters from the module itself: `host_plan=5 host_fail=0 fallback=1 native=0`.

## Two findings worth acting on

1. **Zero size is approved.** The plan says "go ahead" for a 0-byte request instead of refusing. On the live path this is
   harmless today because the caller never asks for zero, but the criterion explicitly names empty routes, and a plan that
   approves nothing is the kind of thing that becomes a real bug once another caller appears.
2. **Negative size aborts rather than refusing.** That is the module's stated contract (`ensure`), and it is recorded as
   such - but if a future caller can compute a negative size from configuration, an abort is a worse failure mode than a
   refusal. Whether that is acceptable is a contract decision.

## What is proven and what is not

- Proven: the decision to allocate or refuse on the pinned-host path is Goose's, it reacts to `disabled`, it handles large
  sizes, and it invokes the fallback decision.
- Not proven: the device-reset and asynchronous-lifetime parts of P3-17; and the zero-size case is a defect to fix, not a
  feature to accept.

The probe exits 1 while the zero-size mismatch stands, so it cannot silently become green.
