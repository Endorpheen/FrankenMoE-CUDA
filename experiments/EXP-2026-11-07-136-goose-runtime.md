# EXP-2026-11-07-136: Self-hosted Goose runtime integration checkpoint

Verdict: EXPERIMENTAL, correctness demonstrated against unchanged native reference;
production performance acceptance remains open. A small unconfirmed slowdown under
competing load does not block larger isolated experiments.

This study progresses from actual CUDA arithmetic and a full quantized dot product
to free-block selection/coalescing, tensor dependency/view lifetime decisions and
an entire allocation-graph traversal controlled by Goose. All exercised Apex output
tokens and probabilities match the original, and rollback has been checked.
Physical CPU/CUDA executors and lower-level native services remain in place.

The checkpoint contains source, frozen self-hosted emissions, integration patches,
a reproducible standalone graph fixture and compact positive/negative evidence.
It excludes private instructions, models, runtime binaries and unrelated changes.

See [the checkpoint](EXP-2026-11-07-136-goose-runtime/README.md) for scope, exact
producers, memory/performance results, failures, rollback and remaining work.
