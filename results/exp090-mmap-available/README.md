# EXP-2026-10-30-090: remove the raw-free-memory veto

Delivery fix requested by Igor after EXP089's manual launch reported insufficient host headroom. MemFree was0.76GiB while MemAvailable was48.87GiB and Cached46.85GiB: the hard MemFree>=8GiB requirement rejected reclaimable file cache.

The corrected budget does not read MemFree. It requires MemAvailable>=the actual registered page union+8GiB and union<=half physical RAM. Existing resident-page, supported-mode, ownership/rollback and disabling controls remain. This changes eligibility only; all subsequent registration, arithmetic, tensor addresses/types, batching, kernels and prefix-cache code are unchanged. It does not force cache eviction or change system memory settings.

The new isolated build is /home/end0/AI/LlamaServer/exp090-mmap-available/build/bin. Only libllama differs from EXP089; the other eight runtime components are identical. The old build and the currently running server are untouched. Restart manually with this directory in both executable path and LD_LIBRARY_PATH. Launch profiles are recorded here; installed application profiles are unchanged.

Compile/link and the adjusted existing ASan/UBSan tests pass: interval merging/overflow, available-memory/physical-capacity boundaries, rollback at every failure, repeated cleanup and exception cleanup. The incremental patch replays exactly to the compiled source hashes. No model is started, no inference is run, and no new speed or low-free-memory resource claim is made by this delivery. EXP089's prior five-pair measurements remain scoped to their measured conditions; Igor subsequently launched the candidate manually: his supplied stdout confirms120 mapped expert ranges registered (18600.47MiB), no weight copy, lazy reservation active, context262144 and server ready on8081. This confirms activation, not a new speed measurement; see manual-launch.json.

Apply patches/mmap-expert-available-memory.patch after the three EXP083/EXP087/EXP089 patches listed in source-provenance.json. Earlier accepted builds and evidence are preserved.
