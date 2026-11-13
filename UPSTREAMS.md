# Pinned upstream revisions

Pinned on 2026-08-30.

| Project | Repository | Branch | Commit |
| --- | --- | --- | --- |
| llama.cpp | https://github.com/ggml-org/llama.cpp | `master` | `0b5be7e4a25862bc2777d0c47eae18788a8c963a` |
| BigMoeOnEdge | https://github.com/Helldez/BigMoeOnEdge | `main` | `f24bd5aeccfc8b6a3c9782ab94ef2ea6d7437f37` |
| llama.cpp expert tier | https://github.com/01554/llama.cpp | `expert-tier` | `4aaad5d318a790a42c2197975ec8fadbad42602b` |
| BigMoeOnEdge llama.cpp submodule | https://github.com/Helldez/llama.cpp | `bmoe/expert-ready-hook-b10666` | `0e8c83e512b99ccf83e50798b86f0b0ec40a7b0a` |

Related work:

- Qwen3.8-Flash-Next/Qwen4Exp support: https://github.com/ggml-org/llama.cpp/pull/27742
- CUDA expert hot tier: https://github.com/ggml-org/llama.cpp/pull/26563
- OMLX SSD expert streaming: https://github.com/jundot/omlx/pull/3260

The clones are created under ignored directories. Build scripts verify these revisions so upstream changes cannot silently alter a result.
