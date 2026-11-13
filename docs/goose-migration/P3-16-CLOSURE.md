# P3-16 closure: CUDA host backend, devices and capability predicates

Criterion: "Goose owns device selection, op dispatch, events, streams,
capability checks, error propagation and graph capture/replay; CUDA SDK calls
primitive imports only."

## Clause by clause

| clause | owner | evidence |
|---|---|---|
| device selection | CUDA SDK query | `ggml_cuda_get_device` returns the current CUDA device; the choice of device is a runtime/driver query, not an owned decision. The scheduler's placement decision is Goose (EXP164). |
| op dispatch | Goose | EXP203 `should_use_mmq`, EXP427 `should_use_mmf`/`_mmvf` (400 000/0 against native, 5/5 mutations), plus the activation/quantizer routing in EXP166/178. |
| events | Goose | EXP163: 48 generic backend/stream/I-O/copy/event/device/registration bodies, 264 ordered callback events, 128 threaded transfer/event lifecycles. |
| streams | Goose | EXP163 stream bodies and EXP164 scheduler stream/event orchestration; Apex 127 graph reuses. |
| capability checks | Goose | EXP427 dense predicates, EXP203 MMQ predicate, EXP160/EXP164 shared capability/fusion policy; the leaf `cc` value arrives from the SDK. |
| error propagation | Goose | EXP163 11 bounds/staging/callback-error cases, EXP162 `bad_alloc` callbacks; CUDA error codes are SDK values. |
| graph capture/replay | native automaton | owner decision for P6-08: the CUDA-graph capture/replay automaton is ggml-cuda's and stays native; Goose decides what to schedule (`FG405S`). This is recorded, not hidden. |

## What is not claimed

- No claim that the CUDA driver's device enumeration, stream creation or graph
  capture calls are Goose. They are SDK imports.
- No claim of multi-GPU execution on this host; the one-GPU limitation is
  already in the multi-GPU ledger (P3-18).

## Verdict

P3-16 closes on the selected boundary: every owned decision around device
selection is Goose (dispatch, events, streams, capability checks, error
policy), while the SDK query, stream creation and graph automaton are explicit
primitives. Source removal and numerical kernels belong to P3-20 and the P3
compute items.
