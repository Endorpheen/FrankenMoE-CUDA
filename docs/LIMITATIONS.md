# Limitations

- Full-model byte equality cannot be measured on the target machine because an 82 GB resident reference does not fit. The streamed path is byte-equal on the small synthetic MoE gate.
- The CUDA hot-store depends on an experimental `expert-tier` graph and non-stable `llama.cpp` evaluation callbacks.
- H2D uses pinned staging but remains synchronous at the public backend call. H2D and CUDA compute are not yet fully overlapped.
- The custom CLI currently uses a raw prompt for qwen4exp because generic `--chatml` can immediately select EOG. The interactive `llama-server` path uses its Jinja handling.
- GPU hot hits run on CUDA while cold experts use the CPU branch of the dual graph.
- The process-global expert-ready hook assumes one active streaming context per process.
- Background GPU allocations materially reduce the automatically selected VRAM hot-store size.
- System swap activity from unrelated workloads can distort measurements even when this process itself never swaps.
- Lossy expert dropping, substitution, route-ahead, and reduced top-k exist for research but are disabled in correctness runs.
- Performance numbers are machine- and prompt-specific. The 14–15 tok/s server result is an interactive observation, not the same workload as the instrumented JSON runs.
