# The merged candidate

`assemble_candidate.py` builds `runtime/reconstructed/candidate-20261112` from three pieces:

- the EXP340 stage as the base, which carries the older but reproducible `libggml-base`, `libggml-cpu`, `libggml-cuda` and the modular support
  libraries;
- the merged `libllama-exp371.so.0.7.0`, which now contains the batch chain, the context reorder, the batch container plan, the sampler, vocab,
  graph work, plus `fg291b` (batch reject), `fg286m` (lazy/mmap) and `fg262a`/`fg263c` (MoE cache admission and copy decisions);
- the merged `libllama-server-impl.so` and `llama-server`, which contain `fg268p` (checkpoints), `fg270q` (queue), `fg272s` (slot selection),
  `fg274b` (batch compatibility) and `fg276t` (stop strings).

## Smoke test, without Apex

With the tiny `stories260K` model and `-c 256`: the server starts, `/health` answers `{"ok":...}`, `/props` returns the default generation
settings, and the log reports `n_slots = 4, n_ctx_slot = 256` followed by `model loaded`. That is a load-and-serve check on a 260K model, not a
measurement; nothing here says anything about Apex behaviour or throughput.

## What the candidate still lacks

The three `libggml-*` libraries are still the older ones, because their sources live in the flint tree and the post-244 work does not touch
them. That means the candidate is complete on the llama and server side and unchanged on the ggml side -- and the Apex run that would tell
whether the eleven merged bridges behave is the next step, with agreement first.
