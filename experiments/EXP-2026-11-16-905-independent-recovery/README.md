# EXP-2026-11-16-905: independent Apex runtime recovery

Status: **functional checkpoint, performance parity NOT accepted**. Further engine changes were deferred at Igor's request so he can run the recovered server manually. No performance improvement is claimed.

## Scope and immutable inputs

Base commit: `7c4f8ae37193890820f09686faadb85a9949e78e`, initially on `main`. Model: `/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf`. Fixed profile: context 262144, all eligible GPU layers, CPU MoE experts, GPU expert cache 4992 MiB, batch/ubatch 4096, t8/tb16, masks 0f0f/ffff, strict affinity, KV q4_0, Flash Attention, one slot, mmap, no speculation. No model/profile tuning was performed.

The original tracked working-tree patch, restored model sources and binary copies were preserved in `Backup/recovery-20261116-codex/` before edits. The `apex-47-reference` directories were never changed or used as build/link inputs. Both reference copies passed all 13 manifest entries; the independent runtime-store set passed its one entry; reference-45 passed 33 entries. Reference core SHA-256: `77c3f17b37bb0bfa63c0978acbe6ff518a284fabf694cc0d95f337dc1a7b5658`. Broken independent core: `906f92ddd2f43c33ded158d2fa08ccc9c40a5d1dfad032d357b6fe4ec33e95b1`. The preserved recipe and old binary names were treated as provenance hints, not as proof of functionality.

## Established startup failure

The old independent core was reproduced with the fixed profile. It occupied GPU memory and kept one CPU core busy without reaching readiness. Process swap was zero; there was no OOM. External debugger attachment was denied by the host. A separate diagnostic preload captured the main thread from inside the process. The stack passed through `gs_fault_handler`, `slru_plan + 0x1f3`, `llama_moe_cache::impl::copy`, and warmup decode. Diagnostic timings were not performance results.

The native caller passes raw uint64 addresses for int32 arrays. The canonical `goose/cpu/goose-moe-cache-slru.goose` has that interface, but the old independent recipe compiled `experiments/EXP-2026-11-16-756-slru-policy-goose/build/slru-policy.c`: an obsolete generated file with many `sl_i64` arguments. The exported C symbol name stayed `slru_plan`, so normal shared-library linking and dlopen did not detect the mismatch. The faulting instruction read an invalid argument as an int64 array element.

The repair generates policy C, runtime C and the public header into each candidate directory from the canonical Goose source. The C++ caller includes that generated header instead of declaring the ABI manually. A negative compile check with the stale header now fails at the first incompatible argument. The SLRU differential gate passed 400/400 random sequences (seed 99, protected fraction 7/8), including final cache state. The accepted 7/8 policy and optimization switches were retained.

## Additional build defects repaired

1. The previous restoration was unfinished: 106 model objects existed, while the library was still the older `906f92dd` build. Restored dispatch cases, models.h and 166 model source files were preserved. The source compile failed at lfm2.cpp because vendor and reconstructed copies of pragma-once headers were included together. Vendor units now consistently use vendor headers. No non-Apex model was run, and restored architecture coverage is not a runtime compatibility claim.
2. `src/llama.cpp` and `src/models/llama.cpp` both mapped to `llama.o`. The core object now has a distinct name.
3. The source-built MoE copy bridge already supplies `fg263c_enabled`; the duplicate in gates.c was removed. The switch remains enabled by default.
4. The mmap bookkeeping object lacked `fg286m_fail` and `fg286m_st_i64`. The new small C bridge provides those two host primitives, matching the existing differential fixture.
5. All required extra objects are now compiled from repository source. Required modules/objects fail explicitly when unavailable. The core linker rejects undefined symbols. The recipe refuses to overwrite an existing build directory and logs its commands. Its output has `$ORIGIN` RUNPATH.
6. The old temporary-file-based test harness could select the wrong server/request and its port-release regex did not match the normal ss output. The harness now accepts explicit runtime/request selection, records arguments, library hashes, ldd, process maps, memory/swap and client duration, and checks the actual port. It retains a lifecycle flock, fixed port 9101 and cleanup limited to its own process group.
7. The source-only support build imports the missing common/server/mtmd/CMake/vendor dependencies with an upstream hash manifest. It compiles GGML/CUDA, common, mtmd, server and existing accepted server decision bridges. No ready reference object or library was linked. Build outputs remain under `/home/end0/AI/LlamaServer/franken-recovery-20261116/`. The pure vendor backend uses native C/C++ implementations; it does not claim preservation of every inherited Goose backend port.

## Build and link evidence

The core and 376-target support build were compiled in clean directories. Subsequent corrections were limited to the mmap bridge compile and a link rejecting unresolved symbols; unchanged native/CUDA units were retained. The final build script describes that same source closure. During this pass the generated version-header input was moved out of an ignored old build directory; the new template expansion is byte-identical to the input used for compilation.

Reproduction, in a NEW output directory (compiler/toolkit/system libraries must be installed):

```bash
bash tools/build-independent-apex.sh /home/end0/AI/LlamaServer/franken-independent-new-build
```

The entire final orchestration command was not run a second time end-to-end: the support/core clean builds and targeted final relink are preserved separately in the evidence. No second build was performed solely to produce a report. `runtime/compiler/current.json` pins the Goose compiler; gcc/g++ 13, CUDA sm89, Release/O3 were used.

`runtime-manifest.json` records every final executable/DSO hash and size. `evidence/independent-dependencies.txt` records the pre-correction link diagnostic; the later functional run's maps are the authoritative loaded-library evidence. Every non-system runtime DSO in the real generation came from `run-fixed`, with no reference/external runtime mixed in. The executable is unchanged across the manual handoff and the saved copy.

## Functional and performance results

A real `/v1/chat/completions` request with the embedded template reached `model loaded` at 2.506 seconds and returned Russian text: `Привет! Да, я тут. 😊\n\nЧем могу помочь?`. Finish reason: `stop`; 66 generated tokens; diagnostic decode 48.74 tok/s. This short response is not comparable to the 512-token benchmark. EOS was not suppressed for this chat.

The original control request is preserved as `request-32k512.json` (SHA-256 `484a12de00ec057e4b78d480ebb185153bb10af607eb23192a72995ba3026d0f`). It uses 32768 explicit input token IDs and 512 generated tokens, seed 123, fixed sampling, no prompt reuse, and the original ignore_eos/probability settings. Those settings were not changed. Two approved independent server lifecycles ran three requests each, with 100% model residency and no overlapping build. Every run actually evaluated 32768 tokens and generated 512.

| Lifecycle | Decode, single-response tok/s | Prefill tok/s |
|---|---|---|
| A | 40.84 / 34.27 / 36.54 | 1901.5 / 1756.6 / 1774.1 |
| B | 41.91 / 38.48 / 41.29 | 1911.2 / 1738.7 / 2197.6 |
| Historical reference | 47.34 / 47.32 / 47.64 | 2317 / 2515 / 2508.6 |

Across the six new responses: median decode 39.659 tok/s (range 34.269–41.906), median prefill 1837.808 tok/s. Relative to the supplied historical median decode 47.34, this is 16.2% slower. This is not a newly paired reference A/B comparison and must not be described as a statistically isolated regression. All six full response SHA-256 values equal `c2ff42cc1219ea4dfe6d69f02e008d0f90c0fc56260736a0aafb82cec362924f`, and all 512-token sequences match the saved reference exactly.

Final RSS after each lifecycle: 21,857,632 / 21,857,768 KiB (about 20.85 GiB). VmSwap 0 KiB; system swap-out delta 0 pages in both. Total GPU usage after the blocks: 11426 / 11455 MiB, including other desktop/services usage. No OOM, hang or failed request occurred in these six requests. See raw status/vmstat/VRAM files for limits of these snapshots; they are not an isolated allocation measurement. Client full-response durations are preserved separately and are not TTFT.

## Remaining limitations and stop point

- Startup and generation are restored. Full speed parity is not accepted. The saved set is a functional checkpoint, not a new best reference.
- The fresh vendor CUDA backend gates host registration behind `GGML_CUDA_REGISTER_HOST`, which defaults off; the core's accepted mmap expert pinning calls that facility. This is a concrete mechanism mismatch to investigate, not a proven explanation of the full speed gap. No change to its default or any new optimization was made after Igor requested manual testing first.
- The source support build has no embedded web UI assets (`LLAMA_BUILD_UI=OFF`, `LLAMA_USE_PREBUILT_UI=OFF`). API generation is proven; browser UI parity is not.
- No other model, architecture or alternate profile was run. Context capacity beyond the tested request was not stress-tested.
- The initial `.gitignore` change, the unrelated EXP398 bridge edit and unrelated untracked user files were not included in this checkpoint. No destructive Git or cleanup was used.

## Manual launch

The parameters are exactly those supplied for the reference. Only the library and executable directories differ. Leave the port free and keep one Apex server at a time.

```bash
LD_LIBRARY_PATH=/home/end0/AI/LlamaServer/franken-recovery-20261116/run-fixed \
/home/end0/AI/LlamaServer/franken-recovery-20261116/run-fixed/llama-server \
-m /home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf \
--alias Apex-35B-V1.0 \
-c 262144 -ngl all -fit off --cpu-moe -fa on -ctk q4_0 -ctv q4_0 \
-b 4096 -ub 4096 -t 8 -tb 16 \
--cpu-mask 0f0f --cpu-mask-batch ffff --cpu-strict 1 --cpu-strict-batch 1 \
--poll 0 -np 1 --cache-ram 0 --ctx-checkpoints 32 --load-mode mmap \
--spec-type none --jinja --temp 1 --top-p 0.95 --top-k 20 --min-p 0 \
--host 127.0.0.1 --port 9101 --moe-cache-mib 4992
```
