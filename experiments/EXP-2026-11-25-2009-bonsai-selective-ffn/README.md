# EXP-2026-11-25-2009 — Bonsai PQ2 audit and selective gate/up diagnostics

**Diagnostic only.** EXP-2006 stays the accepted FP16 prefill reference.
This commit contains a comparative source audit plus the evidence from
the preceding execution of a selective integer projection pilot.
No release binary or existing architecture path changed.

- [Comparative audit, exact source paths, and three candidates](AUDIT.md)
- [Fixed-history full-vocabulary metrics](data/history.json)
- [Four independent generation and scoring fixtures](data/plain-quality.json)

## Selective FFN pilot

Compile the historical EXP-1994 and EXP-1996 integer kernels beside the
EXP-2006 CUDA image. Apply them only to FFN gate/up with at least 128
input tokens. Attention, GDN, FFN down and scalar decode use the accepted
kernels. The same quantized activation is reused across gate and up.
The candidate borrows the existing pack and partial arenas. All modes
allocate 1,720,400,640 bytes of GPU scratch at context 36864, ubatch 1024.
Peak RAM/VRAM and paired request performance were outside this diagnostic.

| Mode | Matches on 196 fixed-history choices | At output 60 / 196 | One diagnostic prefill, ms |
| --- | ---: | ---: | ---: |
| FP16 | 196 | 15 / 16 | 617.542 |
| Plain Q8, all 64 gate/up pairs | 196 | 15 / 16 | 531.962 |
| Q8 + residual, all 64 pairs | 193 | 11 / 17 | 596.847 |
| Q8 + residual, layers 48–63 only | 196 | 15 / 16 | 611.471 |

The common history starts with the 440-token code-trace prompt and uses the
same 195 output tokens as the historical EXP-1996 fixture. All vocabulary
logits are sampled every eight steps plus the initial and historical
boundary rows. In plain mode maximum absolute differences are 0.35790 at
output 60 and 0.20094 at output 196, even though both choices agree.

The four independent free-generation tasks return exactly the same
23, 63, 258 and 47 IDs as the FP16 build. An additional 39 sampled
teacher-forced rows all preserve top-1, with worst absolute logit delta
0.97089 and minimum top-10 overlap 9/10. Code tracing scores 18/63 cells
in both arms. Ledger and scheduling answers are equally incorrect.
Incident extraction is correct in both. These tests preserve observed
task scores and choices on a limited suite. They do not establish bitwise
equivalence of logits, broad quality equivalence, or a 32K/512 speedup.

## Reproduction

Build the diagnostic with the accepted EXP-2006 artifacts present:

```bash
ex=experiments/EXP-2026-11-25-2009-bonsai-selective-ffn
out=build/bonsai-selective-ffn-repro
model=/home/end0/AI/LocalAI/Models/Ternary-Bonsai-2.5-27B-PQ2_0-mtp.gguf
bash "$ex/source/build.sh" "$out"
OPENBLAS_NUM_THREADS=1 python3 "$ex/source/quality.py" plain "$out/quality-plain" \
    "$out" "$model" build/bonsai-integration/inventory.json \
    build/bonsai-integration/vocabulary.gguf \
    /home/end0/AI/LlamaServer/llama.cpp/build/bin/llama-tokenize
```

For the two historical boundaries, use prompt IDs from EXP-1996
`quality/code-trace/prompt.json` and the first 195 IDs from its
`f16.jsonl`. Save that history one ID per line. Run the same 196 steps with
`BONSAI_REPLAY_IDS` and `BONSAI_LOGITS_FILE` set, substituting
`BONSAI_FFN_QUANT=off/plain/residual/last16`. Keep the Flash flags,
CPU affinity, context, ubatch and EOS treatment from the quality harness.
Raw rows and run logs are retained locally in
`build/bonsai-selective-ffn-2009-boundary`.
