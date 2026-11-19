# Apex handover — checkpoint EXP-2026-11-19-1463

Date: 2026-11-19
Revision: 889ddb3f23916df49e96483c360e29b53ffc8e59
Directory: builds/checkpoint-20261119-EXP1463

## Artifact

```
$ sha256sum libllama.so.0
429ca8089caac6514547a91f2e98ced41d49f6ecf38282a5846199999e5e5f81  libllama.so.0
$ wc -l SHA256SUMS
23 files hashed
```

The binary's RUNPATH points at the live build directory, so the launcher must set LD_LIBRARY_PATH to this directory; otherwise the checkpoint silently runs the current build's libraries. Verified: with LD_LIBRARY_PATH set, ldd resolves libllama.so.0 and libggml.so.0 from the checkpoint. patchelf is not installed here, so an $ORIGIN RUNPATH could not be written and the launcher carries the requirement instead.

## Launch

```bash
cd /home/end0/AI/FrankenMoE-CUDA
LD_LIBRARY_PATH=$PWD/builds/checkpoint-20261119-EXP1463 \
  builds/checkpoint-20261119-EXP1463/llama-server \
  -m /home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf --alias Apex-35B-V1.0 \
  -c 262144 -ngl all -fit off --cpu-moe -fa on -ctk q4_0 -ctv q4_0 \
  -b 4096 -ub 4096 -t 8 -tb 16 \
  --cpu-mask 0f0f --cpu-mask-batch ffff --cpu-strict 1 --cpu-strict-batch 1 \
  --poll 0 -np 1 --cache-ram 0 --ctx-checkpoints 32 --load-mode mmap \
  --spec-type none --jinja --temp 1 --top-p 0.95 --top-k 20 --min-p 0 \
  --moe-cache-mib 4992 --host 127.0.0.1 --port 9101
```

This is the accepted profile (rule 35): context 262144, batch and ubatch 4096, GPU expert cache 4992 MiB, `--cpu-moe`,
`-fa on` with `q4_0` KV, 32 context checkpoints, mmap load, no speculative decoding. Any run with other values is an
exploration and its numbers are not comparable.

## Measured on this artifact

Two acceptance runs of this exact binary, under `tools/apex-test.sh` with the lock and the component gate:

```
run 2026-11-19 18:24  decode [60.92, 55.49, 59.88]  prefill [2587.2, 2842.4, 2896.4]  sha c2ff42cc1219
run 2026-11-19 18:56  decode [60.63, 60.26, 62.9 ]  prefill [2630.7, 2845.9, 2966.9]  sha c2ff42cc1219
```

Medians: decode 59.88 and 60.26, prefill 2842.4 and 2845.9. The response hash is the record's `c2ff42cc1219` in both, so text
and token parity hold. VRAM 9733 MiB immediately after start and 11387 MiB after three requests; RSS 20.85 GB after start,
21.80 GB after work; the model 100% resident in RAM.

Behaviour beyond the acceptance run, measured in one server lifetime (`tools/apex-behaviour.sh`): KV reuse `cache_n=11697` of
11705 prompt tokens with `prompt_n=8`, a cancelled streaming request followed by `health status=ok` two seconds later, and the
subsystem list — encoder served with zero comparison disagreements, GGUF guard and load ok, architecture sweep
disagreement-free, five bulk allocations.

Probability parity: `tools/apex-probs-pair.sh` gives the same digest `d384807f4f2014e7` over 24 positions with the sampler
doors on and off.

## Dependencies

```
libllama-server-impl.so   this directory
libllama-common.so.0      this directory
libllama.so.0             this directory
libggml.so.0              this directory
libmtmd.so.0              this directory
libstdc++.so.6            system
libgcc_s.so.1             system
libc.so.6                 system
```

`SHA256SUMS` covers 23 files. The system libraries are the distribution's own; nothing else is required at run time.

## Rollback

The artifact is the rollback. To return to it:

1. stop whatever server is running and confirm the port is free (`ss -ltnp | grep :9101`);
2. launch with the command above, changing only `LD_LIBRARY_PATH` if the directory moves;
3. confirm `[pre-flight] accepted components present: 20/20` and the response hash `c2ff42cc1219`.

A file copied without its `SHA256SUMS` is not a rollback anchor; a symlink or a hardlink is not a copy (rule 22).

## What this handover does not claim

Qwen and other architecture compatibility is not part of it: those requirements are counted in C1 and C2 and continue in the
same runtime. No production promotion is implied - the acceptance here is the record profile on this machine.
