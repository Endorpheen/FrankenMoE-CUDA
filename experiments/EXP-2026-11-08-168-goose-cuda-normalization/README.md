# EXP168 intermediate source checkpoint

Status: source checkpoint, not an accepted runtime integration. The active tested runtime remains EXP167 at `6dd3aae1fc5de7057eddda904f36584f5f15be3f`.

The eight normalization entry recipes, POD launch-plan ABI, complete draft Goose host/device algorithms, native oracle snapshot and previously accepted Goose RMS reference are pinned here. Missing multiply/add operands now fail explicitly rather than silently disabling fused arithmetic. The host FFI has explicit integer/float/bool prototypes for strict C compilation.

`python3 check_checkpoint.py --output checkpoint-checks.json` regenerates both modules using the pinned latest completed full self-hosted producer, checks emitted compute functions for VM allocation calls and checks host C/FFI syntax. This is not a CUDA build, memory-safety validation, inference result or performance claim. No accepted binary, production build or compiler source changes.

Remaining integration gates: independent host-plan review; complete CUDA projections/bridge; native/independent GPU reference and guard checks for every entry; real Goose-execution counters; one fixed Apex 32K/128 correctness and prefill/decode/RAM/VRAM check against the unchanged accepted runtime. Remove the old RMS preload only in the new candidate after the replacement is linked. Preserve the EXP167 immutable runtime and native disable switch.

Exactly two authorized Command Code workers use `z-ai/glm-5.3-flash`: host planning and independent GPU fixtures. They receive sanitized source snapshots in separate worktrees, with disjoint output ownership. Codex reviews code and ABI, performs the actual compiler/GPU/inference checks and integrates only accepted changes. No DeepSeek/Codex subagents and no silent model fallback.
