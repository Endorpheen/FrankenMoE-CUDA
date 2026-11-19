# EXP-2026-11-17-934 — Maxwell-V1-Flash Q4_0 on real weights

Status: ACCEPTED (load + generation); long-context MBSA not implemented

## Artefact

`maxwell-v1-flash-q4_0.gguf`

- size: 21,418,677,984 bytes (21.4 GB)
- sha256: `8d53f1e39d9b1940f2639b364a01c420aec99cbf26320f1fdeb8a662945c11f3`
- tensors: 981, metadata keys: 44, `pre=qwen35`, merges 199,740
- source: `maxwell-model/Maxwell-V1-Flash` (35 BF16 shards), converted with
  `tools/maxwell-convert.py`; Q4_0 for 2-D weights, F16 for mHC projections and
  embeddings/LM head, F32 for every 1-D tensor.

## Checks on real weights

- Load: `n_vocab=200704 n_embd=2048 n_layer=45`; `llama_decode` returns 0;
  logits finite.
- Raw completion via `POST /completion` (bypasses the chat parser), prompt
  "The capital of France is", 16 tokens, temperature 0:
  ` Paris.\n\n**Constraints:**\n- The user asks: "What is`
- CPU timings at 16 threads: prompt 23.39 tok/s, decode 9.47 tok/s
  (5 prompt tokens, 16 predicted; not a benchmark profile).
- GPU profile (RTX 4070 12 GB, `-ngl all --cpu-moe --moe-cache-mib 4992 -fa on
  -c 4096 -b/-ub 512 -t 8 -tb 16`), raw `/completion`, prompt 601 tokens,
  128 generated, temperature 0: prefill 236.98 tok/s, decode 26.17 tok/s,
  TTFT 2.54 s, VRAM 11617 MiB, RSS 21009 MiB (one run, not three).

## Known limitations (stated, not hidden)

- Verified context is 4096 tokens. Above it the builder refuses until MBSA
  lands; there is no silent dense fallback.
- MBSA graph is not implemented yet; the indexer contract (tensors, keys,
  converter) is in EXP-933 and `docs/goose-migration/MBSA-HANDOFF.md`.
- Chat path: the model's Jinja template is embedded, but the CLI's PEG parser
  rejects the generated output; the raw completion endpoint works.
- No GPU benchmark was run (requires Igor's approval); no Apex regression run
  yet.
- Tokenizer pre-type maps to `qwen35`; Han/Thai splits and `\p{N}{1,3}` from the
  original file are a known difference.

## Same-load comparison with Apex (method)

Maxwell is verified to 4096 tokens, so the common profile is 2048 prompt /
128 generated, identical token ids for both models (first 2048 tokens of
`/tmp/req512.json`), `ignore_eos=true`, temperature 0, seed 42.

Both runtimes use the same flags where they apply: `-ngl all --cpu-moe
--moe-cache-mib 4992 -fa on -b 2048 -ub 2048 -t 8 -tb 16`, context 4096 for
both so the KV and compute buffers are the same size. Apex is the accepted
reference directory; Maxwell is the Q4_0 GGUF. Three repeats each, medians.

Numbers are added below only after both runs complete.

## Same-load result: Maxwell Q4_0 vs Apex Q4_K_M

Profile: 2048 prompt tokens (identical ids) / 128 generated, ctx 4096 for both,
`-ngl all --cpu-moe --moe-cache-mib 4992 -fa on -b 2048 -ub 2048 -t 8 -tb 16
-np 1`, 3 repeats, medians. Maxwell ran on the candidate runtime
(our-src/EXP906 + Maxwell objects); Apex on the accepted reference `run/`.

| axis | Maxwell Q4_0 | Apex Q4_K_M |
|---|---:|---:|
| prefill tok/s (median) | 1512.06 | 1396.30 |
| prefill range | 1262–1579 | 466–1463 |
| decode tok/s (median) | 54.08 | 39.53 |
| decode range | 48.93–54.56 | 37.34–48.76 |
| RSS MiB | 21136.8 | 18485.5 |
| VRAM total MiB | 10892 | 10685 |
| GGUF size | 21.42 GB | ~21 GB |

Caveats, stated rather than hidden:

- this profile is NOT the accepted 32768/512 one; the Apex 55.5176 figure must
  not be compared to the 39.53 here;
- the first Apex run was cold (prefill 466) and the spread is wide because
  unrelated GPU processes were resident; medians of three are reported, not
  best-of;
- Maxwell is limited to the 4096-token window (dense path), Apex has no such
  limit in this run.

### Warm medians (warmup excluded)

| axis | Maxwell Q4_0 | Apex Q4_K_M | Maxwell vs Apex |
|---|---:|---:|---:|
| prefill tok/s | 1895.80 | 1348.55 | +40.6% |
| decode tok/s | 65.92 | 46.11 | +43.0% |
| RSS MiB | 20310.1 | 18770.7 | +8.2% |
| VRAM total MiB | 11134 | 10503 | +6.0% |

Ranges: Maxwell prefill 1887-1907, decode 65.6-66.1; Apex prefill 1305-1350,
decode 46.0-46.9. Warmup passes (excluded): Maxwell 1524.65/58.29,
Apex 838.81/38.83.

The trade is explicit: Maxwell is faster on both compute axes here and uses
more RAM/VRAM; Apex keeps the much larger context (262144 vs 4096 verified).

## Chat error: root cause and fix (2026-11-17)

Symptom: `POST /v1/chat/completions` returned HTTP 500
`The model produced output that does not match the expected peg-native format`.

Evidence:

- server log: `common_chat_peg_parse: unparsed peg-native output:
  <|maxwell_start|>assistant\n<think>...`;
- a raw completion with the chat prompt produced token `200001`
  (`<|maxwell_end|>`) but `stop_type=limit`: 200001 was not end-of-generation;
- `generation_config.json` declares `eos_token_id: [2, 200001]`, while the GGUF
  carried only `2`;
- restarting the same server with an equivalent plain template
  (`<|maxwell_start|>{role}\n{content}<|maxwell_end|>\n`) made the chat endpoint
  return HTTP 200 with the assistant text, still finishing by length because of
  the EOS defect.

Cause: (1) the upstream Jinja uses namespace/split/raise_exception/tojson,
which the server's PEG auto-parser cannot turn into a grammar; (2) the second
EOS id was missing, so the model never stopped at `<|maxwell_end|>` and the
parser saw an unterminated reply.

Fix: the converter writes the equivalent plain template (upstream kept with
`--chat-template hf`) and writes eos/eot/eom from the generation config list.
Checks: 89 converter checks, PEG-safe template asserted.

Verification of the rebuilt GGUF (Russian dialogue, long prompt, HTML/Three.js,
finish reason) is recorded below once the conversion completes.
