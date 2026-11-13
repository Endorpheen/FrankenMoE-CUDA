# EXP424: P8-11 concurrent clients, errors and repeated lifecycle

Acceptance: "Concurrent independent/branched sessions, disconnects, invalid
inputs and repeated model lifecycles complete correctly; leaks/races scoped
checked."

## Setup

Unified Apex build `runtime/exp421/unified`, `-np 4`, context 4096, classic
logging, one server process per lifecycle. Short deterministic request with
seed 123 is the reference.

## Result

| case | result |
|---|---|
| four concurrent requests | all HTTP 200, each returns 8 predicted tokens; different prompts/seeds produce different content |
| malformed JSON body | HTTP 400, server stays alive |
| request after the bad body | content `02ec2f94...`, identical to the reference |
| streaming client disconnects after 3 events | next request content `02ec2f94...`, identical |
| server shutdown | SIGINT, rc 0 |
| port after shutdown | free |
| second lifecycle (fresh process, same request) | content `02ec2f94...`, identical, rc 0 |

The concurrent responses are independent: four different content hashes, no
cross-talk, no lost task. The invalid request and the disconnect do not poison
the next request. A fresh process reproduces the reference answer.

## Branched sessions

Branched session behaviour is covered by the state round trip already recorded
in EXP293/EXP357: a saved state restores into a fresh process and continues
identically, and slot save/erase/restore is byte-compatible with native. This
experiment does not add a new branch action because the server has none.

## Leaks and races, scoped

Measured here: no port or process left after shutdown, server rc 0 on both
lifecycles. Measured earlier: EXP267 in-flight SIGINT leaves balanced pools
(`FG195V 9526/9526`, `FG196P 120/240`, native=0); EXP379 cancellation is
followed by a normal request. A full TSan run of the server is not part of this
experiment; the claim is scoped to the process/port/pool lifecycle and the
functional concurrency above, not to a race-free proof of every server path.

## Verdict

P8-11 closes on the corpus above. The remaining server implementation work
belongs to P8-01/P8-02/P8-12, not to this acceptance.
