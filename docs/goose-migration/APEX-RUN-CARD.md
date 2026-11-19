# The Apex run, ready to start on one word

Prepared 2026-11-19 evening, not started. Everything below was verified without starting a server, so that
Igor's approval turns into one command instead of half an hour of preparation.

## Prerequisites, each checked

| prerequisite | state |
|---|---|
| model file | `/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf`, 21 713 463 136 bytes |
| residency helper | `/tmp/res` present (rule 23 requires the check before measuring) |
| server binary | `builds/checkpoint-20261119-EXP1476-loadable/llama-server`, verified to run (`--version`, exit 0) |
| the set it runs from | **the loadable copy**, not the raw checkpoint: the raw one lacks `libmoe_mmid_tiled.so` and cannot start |
| accepted components in that set | pass (`check-accepted-components.py`, run as pre-flight inside `apex-test.sh`) |
| port 9101 | free: `ss -ltn` reports **0** listeners |
| GPU | 2795 MiB used by another process, 12 GiB total - enough for the profile |
| lock | `/tmp/apex-test.lock` handled by `apex-test.sh` itself (rule 30) |

## The command

```
cd /home/end0/AI/FrankenMoE-CUDA
APEX_SERVER_DIR=$PWD/builds/checkpoint-20261119-EXP1476-loadable \
  bash tools/apex-test.sh json-baseline-1476 512 4096 4096 4992
```

What each argument means: tag `json-baseline-1476`, 512 predicted tokens, `-b 4096`, `-ub 4096`, MoE cache
4992 MiB. The profile (ctx 262144, `-ngl all`, `-fit off`, `--cpu-moe`, KV q4_0) is fixed by the script and
must not be changed - rule 35.

Expected duration: readiness plus generation, a few minutes. Every run writes its own
`results/apex-runs/<tag>-<timestamp>/` with the log, so nothing mixes.

## What this run establishes, and what it does not

It establishes that **the accepted artefact, made loadable, answers a real request** with the profile the
recorded numbers were taken under - which is the third of the three claims this repository keeps apart:

1. the files match their hashes - checked;
2. the set loads and starts - checked;
3. **it answers correctly and at the recorded speed - this run.**

`REPRODUCTION-VERDICT-20261119.md` stays true afterwards: this measures the accepted artefact, it does not
make it reproducible. Nothing here replaces the decision about which artefact becomes the baseline.

## If it fails

`apex-test.sh` preserves stderr, PID, parameters, VRAM and lock status on CUDA OOM and verifies port
release afterwards. The first thing to read is `results/apex-runs/<tag>-<timestamp>/diagnostics.txt`, not
`nvidia-smi` - on 2026-11-16 the GPU was blamed three times for what was a leftover server.

## What it would let me close

With one successful run, these have everything else already done: P7-06 and P7-07 (tokenizer and
detokenizer doors, both wired), P8-06 (model-manager doors), P9-07 (metrics formatting), and the acceptance
half of P6-10 (grammar). The measurement is the only missing evidence for each.
