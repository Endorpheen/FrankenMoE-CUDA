# Integration

| Directory | Role |
|---|---|
| [goose-standalone/](goose-standalone/) | Standalone inference host, C system and CUDA adapters, CLI, Windows launcher and test fixtures. The [Apex checkpoint recipe](goose-standalone/apex-checkpoint/README.md) identifies the CUDA driver used by the current Linux chat build. |
| [m1/host/](m1/host/) | Shared C system adapters used by the standalone build. |
| [m1/](m1/) outside the host adapters | Earlier integration patches and gates, retained as experiment evidence. |
| [edge0/](edge0/) | Edge0 research fixtures. See the [model registry](../docs/goose-migration/MODEL-ARCHITECTURES.md) for qualification status. |
| [modules/](modules/) | Experiment bridge sources retained for provenance and ABI checks. |
| [p1-12/](p1-12/) and [p6-10-accepted-src/](p6-10-accepted-src/) | Historical CPU SIMD and sampler prototype fixtures. |
| [p6-10-removal/](p6-10-removal/) and [p7-12-gguf-context/](p7-12-gguf-context/) | Historical native sampler and GGUF migration plans referenced by the ledger. |

Start with the [build guide](../docs/BUILD.md) when working on a current
chat package. The old CMake and bridge configurations belong to their
recorded upstream revisions.
