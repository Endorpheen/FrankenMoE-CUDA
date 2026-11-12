# P1-10 inventory: expert scratch, residency, staging and scatter

Status: inventory step, item stays IN_PROGRESS. Checked against the tree that actually runs today.

## What exists in the current (flint-based) tree

- `ggml-backend.cpp` in the flint tree has **no** "group consecutive experts and copy them together" path; that text lives
  in the old FrankenMoE patches, not here.
- Consequently the **pinned staging ring** (`GGML_EXPERT_PINNED_RING`, `ring-depth-3`) that P1-10's staging wording refers
  to is not present in the running configuration at all. Disabling or enabling it is not a live switch here - there is
  nothing to switch.
- What the current tree does have, and what our work actually uses:
  - `pin_mmap_experts` (restored by us): registers resident expert pages as host buffers, no weight copy;
  - `--moe-cache-mib` device cache, whose slot/budget planning EXP-150 moved into Goose;
  - EXP-145 graph scratch growth/reuse/failure policy in Goose;
  - EXP-087 lazy compute reservation and EXP-101 lazy KV VMM (C++ in our tree, not Goose).

## Consequence for the requirement

The requirement text mixes two mechanisms from different eras:

1. **Old-ring staging** (pinned slots, upload grouping, ring depth) - the substrate does not exist in the flint tree, so
   "complete expert staging ownership" cannot be finished as written. Either the mechanism is re-introduced deliberately or
   the requirement is restated for the current substrate.
2. **Current residency and cache ownership** - this is live and partially in Goose (bank budget/slot planning, scratch
   policy), with the cache constructor, tensor allocation, full upload-copy and resource lifecycle still native.

So P1-10 is not merely incomplete: part of it is **stale**. That is exactly what the audit was asked to surface, and it is
recorded here rather than being worked around by re-implementing a dead ring.

## What would close it for the current substrate

- inventory the live residency paths: `pin_mmap_experts`, the device cache (`llama-moe-cache.cpp`), the graph scratch
  policy, and the KV lazy mapping;
- for each: decide whether Goose ownership is required, then port it with the failure/resize/empty/overlapping/cancel
  checks the criterion names;
- restate or retire the ring-based staging clause by an explicit decision, not by silence.
