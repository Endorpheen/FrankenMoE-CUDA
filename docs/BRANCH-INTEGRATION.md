# GooseInfer branch integration plan

Inventory captured on 2026-11-27 against `origin/main` at
`0ca67da9cfcb1afeda79d48b56f229d9839c96ee`, after fetching all
published refs. Counts are behind/ahead by reachability. They measure
commit history, and individual changes may overlap even where commit
IDs differ.

| Published branch | Tip | Behind/ahead | Integration decision |
| --- | --- | ---: | --- |
| `codex/gigachat4-cumulative` | `6a46a6be0` | 10/143 | Integrate the GigaChat4 model path and validated MLA/KV checkpoints first. Keep pending certified DSA work separate until its isolation and regression gates pass. |
| `codex/inkling2-cumulative` | `ad0694c28` | 10/106 | Select only supported Inkling changes. EXP-2128 is diagnostic and needs paired confirmation. |
| `codex/multimodel-release-main` | `40bed304a` | 10/94 | Recover the source and build recipe for the published Vulkan distribution. Retain the package and device limits in the release evidence. |
| `claude/apex-100-prefetch` | `6791ef142` | 177/46 | Identify the tested EXP-1854 flash change and dependencies. Keep rejected expert-cache variants as evidence. |
| `codex/apex-mailbox-prefetch` | `e330b350e` | 165/33 | Compare with the other Apex line by patch and mechanism. Promote only reproducible improvements on the common reference. |
| `codex/inkling2-mini-device-attention` | `529559fd9` | 165/42 | Compare device attention and accumulation work with the cumulative Inkling branch. Resolve token divergence on matching placement profiles before selection. |
| `codex/apex-goose-preserved-20261112` | `fa73331a7` | 3292/2 | Retain as recovery evidence. Review unique artifacts before any closure. |
| `experiments/goose-runtime-20261107` | `6e1c0b8e3` | 3278/4 | Retain the early standalone checkpoint. Port an isolated method only when current work needs it. |
| `snapshot-2026-11-13` | `5362d3786` | 3414/1 | Retain the source snapshot and its provenance. |

Local inventory found additional unpublished Apex checkpoints, backup
refs, and dirty worktrees. Preserve those refs and working changes until
their owners or applicable work instructions identify their disposition.
Some local refs differ from their remote counterparts. Compare both SHAs
before marking either complete. Refresh the inventory before each
integration or closure.

## Integration order

1. **Freeze references on current main.** Record runnable binaries, model
   and fixture hashes, release profiles, output IDs, prefill, decode, RAM,
   VRAM and swap. Classify each candidate as model-local or shared-core
   work using its dependency graph. Reserve the isolated GigaChat4
   development worktree until its pending changes are recorded.
2. **Integrate GigaChat4 in coherent slices.** Bring over the model adapter
   and validated runtime dependencies, then the EXP-2185 lazy MLA/DSA
   KV and EXP-2189 indexed MLA path in dependency order. Compare the
   candidate on the fixed 32K/512 workload and run the complete
   cross-architecture gate before merging. Audit the pending certified
   DSA patch against the same source-built reference in a separate step.
3. **Consolidate Inkling.** Compare both Inkling branch diffs against the
   updated main. Pick one graph and residency implementation. Check the
   32K/512 input, token IDs, throughput and peak memory for each accepted
   mechanism before incorporating it. Preserve unmatched experiments
   in the registry.
4. **Integrate Vulkan source and verification.** Port the independent
   backend, model adapters and package checks from the published alpha
   lineage. Rebuild the runtime and preserve CUDA references. Record
   native AMD and native Windows GPU coverage explicitly. Use release
   labels that match measured coverage.
5. **Consolidate Apex follow-ups.** Review the local expert registration
   budget finding and both published Apex branches together. Apply
   verified mechanisms independently in their dependency order, then
   compare the composed result with the frozen Apex reference and run
   the complete cross-architecture gate.
6. **Close completed branches.** After verifying that each merged or
   superseded branch has no unique work left, close its PR and remove its
   working ref when its worktree is no longer active. Preserve published
   tags and recovery refs. For partial integrations, record retained
   SHAs and unresolved changes before retiring the source branch.

Each step starts from current main and retains the measured artifact
until the next is qualified. A failed gate returns the change to its
development line for isolation or correction.
