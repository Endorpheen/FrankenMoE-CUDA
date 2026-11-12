# Apex Goose recovery snapshot

This clean snapshot preserves the full runtime migration source and optimisation evidence from the active worker without changing its worktree or index. It is a backup, not a new correctness or performance acceptance.

The source commit and file hashes are recorded in MANIFEST.json. The experiment history stays intact locally; bulk secondary reports are packed losslessly in experiment-evidence.tar.gz. Build outputs, raw prompts/responses, model weights, private agent state and credentials are excluded.

The complete reconstructed C++ source is included under runtime/reconstructed/src, including files previously untracked in the worker checkout. Goose modules and bridges remain under experiments/*/source. Existing build scripts retain their recorded paths and must be redirected to a new checkout/build directory before use. They are not advertised as portable, dependency-free builds.

The external Flint tree is pinned by commit in MANIFEST.json. Apply flint-memory-overlay.patch to that tree to restore the current compute/KV memory changes. User changes to common/jinja and tests were excluded. CUDA integration build scripts and historical lazy reservation/KV patches are retained.

Fried Goose compiler v61 is an external dependency identified by SHA in MANIFEST.json; its private compiler repository is not republished here. No models or benchmark conversations are included.

Current claims must be read from docs/goose-migration/apex-optimization.json. The 30+ decode GPU-cache variant remains experimental while its output difference is investigated. The reconstructed full-port tree includes the restored EXP087/EXP101 memory optimisations; preserve source ownership and correctness when integrating later work.

Selected runtime binaries are stored separately as draft-release backup assets, with independent SHA256 and recovery metadata. They do not replace the source or prove reproducibility.
