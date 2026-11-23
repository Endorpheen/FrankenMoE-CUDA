# Current state

The standalone FrankenMoE chat runtime runs Apex-35B-V1.0 Q4_K_M and provides
a bundled web UI and an OpenAI-compatible API for OpenCode. Goose owns the model
execution and HTTP decisions; C adapters provide system and CUDA driver calls.

| Lineage | What is available |
|---|---|
| Published v0.12.1 chat alpha | OpenCode continuation and reasoning accounting on Linux x86-64 (EXP-1808) and Windows x64 (EXP-1805 CUDA host). Windows HTTP coverage is from Wine. |
| Cumulative Linux sources | EXP-1805 route prefetch, EXP-1806 early decode submission and EXP-1808 asynchronous prefill job packs. The build uses the pinned checkpoint archive and device images. |
| Other architectures | See the [model registry](goose-migration/MODEL-ARCHITECTURES.md) for the evidence and remaining qualification per model. |

The archived EXP-1808 standalone checkpoint recorded 3122.4 tok/s prefill
and 78.225 tok/s decode on Apex 32K/512, with a 262144-token context and
6290 MiB expert cache on an RTX 4070. The 81.292 tok/s decode result came
from a separate EXP-1806 candidate run. The current Linux chat build recipe
and the benchmark checkpoints are documented separately.

- [Launch and terminal interface](../README.md#run-the-linux-chat-server)
- [Build inputs and packaging](BUILD.md)
- [Runtime and integration map](../integration/README.md)
- [Scope and limitations](LIMITATIONS.md)
- [Current work and historical plans](../ROADMAP.md)

The earlier hybrid snapshots and their evidence are indexed under
[experiments](../experiments/EXPERIMENTS.md) and
[the migration archive](goose-migration/INDEX.md).
