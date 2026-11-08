# EXP153: unified Apex Goose integration checkpoint

This is an experimental source checkpoint,not functional completion or performance acceptance. Production and compiler sources are unchanged. The main Apex migration GOAL remains active.

## Integrated and bounded evidence

The current shared cohort combines prior GGML/CPU/graph/scheduler/model/hybrid/recurrent work with source-linked Q4_K/Q6_K dot/repack/control,active lazy-VMM-compatible KV/state,and ordinary context plus canonical state IO. Existing native controls and immutable candidate libraries remain available for rollback.

The normal context constructor/cparams/host-state fixture agrees with its preceding cohort. Context-aware device IO handles pass repeat/rebind,geometry changes and independent-context cleanup with zero tracked canonical allocations. This is model-free boundary evidence.

The CUDA numeric packages are one shared library. One actual RTX4070 joined graph exposed Q4 byte-addressing and F16 raw-effect corruption that host fixtures missed. After the recorded root corrections,the staged SSM-conv/RMS/fused-GLU/F16/Q4 outputs are bit-exact with the preserved native CUDA DSO. The initial Nsight activity independently records Goose SSM/RMS/movement kernels;the corrected binary executes the same graph. No inference throughput or full Apex correctness is claimed from this fixture.

## Concrete integration corrections

- ON_DEVICE state reads use actual serialized metadata length,not unused buffer capacity.
- Canonical IO uses typed ABI boundaries and persistent handles keyed by context and sequence;checked publication and cleanup replace unsafe temporary lookups.
- Routed CPU activation quantization uses input K,not output width;tail Q8_K and GEMV iteration execute Goose.
- KV public state routes execute Goose and prepare the exact lazy tensor ranges;normal destructor unregisters the owned handle.
- The SSM bridge passes the matching six strides.
- Q4 copy uses separate scalar/block byte offsets and advances the input block correctly.
- CUDA scalar raw effects use aligned typed accesses after byte-wise builtin lowering produced incorrect F16 bits on hardware;Goose arithmetic and compiler are unchanged. The exact toolkit-level cause remains unproven.

## Remaining gates

A full ABI-consistent CUDA control cohort is building separately because appending its canonical context handle changes sizeof(context);mixing old/new context objects is forbidden. Resource-control ABI/OOM/lifetime fixes returned from the existing worker are included in that build. Loader context ownership transfer,normal vocab/sampler/server interfaces,canonical source/import closure and the full fixed Apex32K/128 gate remain pending. Do not count source-only modules as DONE.

Use APEX_GOOSE_MIGRATION_PLAN.md as the requirement authority. The owner superseded separate Batch A acceptance barriers:join the prepared Apex subsystems into one candidate and then run the shared full-model gate,with necessary local ABI/correctness checks retained. Qwen and other architectures remain deferred.

## Reproduction and provenance

The source snapshot excludes model weights,credentials,private instructions,private COMPILER_FEEDBACK.md,binaries and profiler databases. source-manifest.json locks included source bytes and the qualified self-hosted compiler SHA. commands/ retains exact local compiler/link invocations and results/ the bounded gate receipts. Absolute external dependency paths refer to the existing local checkpoints,not a claim of a standalone portable build. Reproduction helpers retain immutable-library refusal checks where present;review individual helper prerequisites before replaying an initial failed attempt. The live build/results remain under /home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107/.
