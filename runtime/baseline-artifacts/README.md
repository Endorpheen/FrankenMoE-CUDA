# Preserved baseline artifacts

`libggml-base.so.0.e213fa19` is a byte copy of the base library the accepted
Apex baseline loads (`runtime/reconstructed/build/stage-run/libggml-base.so.0`,
sha256 `e213fa19e1697a37daaf22dd294a0659d95ebd4d57df65b5b4a72bebc3c3d118`).

It is kept here read-only-in-spirit because the library cannot be rebuilt from
checked-in sources with an identical result: the exact object set used for the
deployment was only partially archived, and a faithful rebuild produces a
different artifact and different numbers (see
`docs/goose-migration/BLOCKER-20261112-sched-cuda-split.md`).

Do not replace this file. A new accepted base must be added as a new artifact
with its own hash and its own object archive next to it.
