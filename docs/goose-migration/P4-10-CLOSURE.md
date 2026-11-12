# P4-10 closure: cross-device copy planning on the Apex scope

Criterion: "Goose owns split inputs, copy slots, events, waits and lifetime
edges; multi-stream/multi-sequence cancellation/failure fixtures pass."

## Apex / one-GPU evidence

EXP164 transfers all 37 normal scheduler/layout entry bodies:

- placement and compatibility, offload, split inputs and parallel copies;
- ordered allocator dependencies, reserve/reuse;
- compute/eval/event and expert orchestration, owned lifetime.

Fixtures: 44 native placement/reuse cases, two wide 72-input graphs, a compute
failure, 10 expert phase routes, 12 constructor OOM sites and 32 threaded
independent lifecycles, with rollback.

Apex 32K/128: prefill 2626.1, decode 27.3, RSS 21 315 MiB, VRAM 3246 MiB, exact
tokens/content/all probabilities, 127 graph reuses, scheduler witness and zero
owned heaps.

Multi-sequence cancellation is measured at the server/API layer: EXP379 client
disconnect, EXP424 stream disconnect and next-request exactness, EXP267 in-flight
SIGINT with balanced pools and clean restart.

## Boundary

- Multi-GPU and actual asynchronous execution across several physical devices
  are hardware-unavailable on this host (one RTX 4070). They stay in P3-18's
  hardware-deferred ledger, not claimed as tested.
- Synthetic callback order and expert cancellation are not presented as
  multi-GPU proof; they are scheduler-order evidence.
- The scheduler copy slots/events/waits themselves are Goose and exercised by
  the EXP164 threaded lifecycles and the Apex graph reuse.

## Verdict

P4-10 closes on the Apex/one-GPU scope. The multi-GPU half is explicitly
deferred and unverified; the owned copy planning and lifetime edges are Goose
with the evidence above.
