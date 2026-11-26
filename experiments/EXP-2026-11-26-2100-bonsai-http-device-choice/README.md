# EXP-2026-11-26-2100 — Resident GPU choice in Bonsai HTTP decode

**Experimental alternative.** The GPU selection path runs in the
normal Bonsai HTTP generation loop. EXP-2096 remains the primary
cumulative checkpoint because the rebuilt legacy sampler is slower
than the frozen reference. Other architecture packages are unchanged.

## Implementation and source recovery

EXP-1728 and EXP-1734 retained identical generated C and Goose
inputs. Their `main.o` matches the object extracted from the EXP-2096
runtime archive, SHA-256 `111b329d…9a6`. Use that historical source,
rather than the independently composed current tree, for this
experiment. The current-tree typecheck first exceeded KeelWing's
default 512 MiB AST admission limit. The scored build uses the
recovered source with a 2048 MiB AST budget and GCC 13 at -O1.
Its generated public header matches the recovered header byte-for-byte.

The first sample uses the ordinary prefill result. For subsequent
samples, opt in only for greedy Bonsai without typical filtering,
repeat penalties or requested probability rows. Require a matching
EOG list before using device choice. The CUDA graph exposes logits
in the existing device output buffer. Reuse EXP-2034's two exact
selection kernels and copy one 12 B result back. If it reports a
nonfinite logit or no valid token, copy the real full logits and
use the legacy sampler without another model step. Hard CUDA
failures follow the normal generation failure/reset path.
The one-row choice workspace requests 33,968 B at model load.

## Correctness

The isolated Goose-generation fixture passes five cases: enabled,
disabled, EOS behavior, probabilities and seeded non-greedy fallback.
It injects a completed-step nonfinite fallback and verifies that the
evaluation is not repeated. In four short real HTTP scenarios,
the same binary with device choice on and off returns identical
IDs and text. The probability scenario returns identical four rows.
Its short EOS scenario exercises the unmasked route; it does not
happen to generate an EOG token. The fixture covers the stop branch.

## RTX 4070, HTTP 32K/512

Both arms use the same rebuilt executable, combined CUBIN and frozen
PTX, with only `BONSAI_HTTP_DEVICE_CHOICE` toggled. Identical GGUF,
EXP-1977 raw prompt, context 36864, batch/ubatch 4096/1024, Q4_0 K/V,
greedy, seed 123, EOS masked, MTP off and CPU flags. Both pass the
cold gate with zero reused inputs and all 32,768 computed. All 512
output IDs and text match the archived PrismML result.

| Run | Prefill, tok/s | Decode, tok/s | Decode, s | Client, s | VRAM after request, MiB | VmHWM, KiB |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| A · Rebuilt legacy sampler | 1100.18 | 29.78 | 17.157 | 47.003 | 8932 | 7458604 |
| B · Rebuilt device choice | 1100.61 | 38.97 | 13.113 | 42.949 | 8934 | 7457600 |
| EXP-2096, EXP-2097 mean | 1094.19 | 37.82 | 13.513 | 43.487 | 8932 | 7457928 |
| PrismML, EXP-2097 mean | 1095.12 | 48.08 | 10.628 | 40.559 | 8288 | 7654122 |

B saves 4.045 s in decode versus A in the single pair. That delta
includes recovery of a rebuilt-control deficit. It is not a
4-second gain over EXP-2096. The comparison with the earlier
EXP-2096 mean places B about 0.400 s ahead in decode, with no
contemporary pair against the original executable. Swap is zero
in both arms. B reports +2 MiB post-request process VRAM.

The recompiled CPU greedy loop is not equivalent to the frozen
machine code. Disassembly identifies `gg_function_369` in the
rebuild with ownership scope calls inside the vocabulary scan.
The matching frozen `gg_function_368` scan has two index checks
per comparison and no scope calls. This is a concrete source of
extra CPU work. This experiment does not isolate its entire
performance effect or verify fallback throughput across requests.
Keep the alternative isolated. Recover matching compiler code
generation, or address CPU sampling overhead with equivalence
checks before cross-build promotion.

## Executed GPU path

One separate cold-gated trace of B matches 512 ID values and the
text, with 511 tile-choice and 511 final-choice launches. Both
reduction stages total 3.235 ms of GPU kernel time.

| Decode readback or gap | EXP-2096 trace | EXP-2100 B trace |
| --- | ---: | ---: |
| Full readbacks in decode | 511 | 0 |
| Choice record readbacks | 0 | 511 |
| Decode D2H bytes | 507,566,080 | 6,132 |
| Decode D2H GPU service | 20.12 ms | 0.45 ms |
| Decode GPU gaps | 981.00 ms | 622.53 ms |

A 993,280 B readback still serves the first sample in prefill.
The full trace reports 1101.04 / 38.60 tok/s and 43.064 s client
time. These are diagnostic results and remain separate from the
unprofiled pair. Full phase, kernel, copy and trace hashes are linked
below. No fresh Apex or Inkling runs were done, and the candidate did
not replace either packaged architecture.

## Reproduce

Use the recovered EXP-1728 `main.goose`, pinned by the compose
script. Set `FRANKENMOE_CHECKPOINTS` to the artifact tree and use
the released KeelWing compiler. Use EXP-2096's exact build and
EXP-2099's combined device image.

```bash
exp=experiments/EXP-2026-11-26-2100-bonsai-http-device-choice
out=build/bonsai-http-argmax-integration-2100/frozen-source
bash "$exp/source/compose.sh" "$out" RECOVERED_EXP1728_MAIN_GOOSE
bash "$exp/source/build-http.sh" "$out" build/bonsai-ffn-down-fused-q8-2096 build/bonsai-http-argmax-probe-2099/pipeline.cubin
bash "$exp/source/test-fixture.sh" build/bonsai-http-argmax-integration-2100/test-fixture
```

Use the staged EXP-2096 argv and environment with the experimental
binary and CUBIN, plus `BONSAI_HTTP_DEVICE_CHOICE=0` or `=1`.
Send the unchanged [EXP-1977 raw request](../EXP-2026-11-25-1977-bonsai-prism-32k-reference/request.json)
and use the cold-prefill validator before and after each arm.
The raw trace is under `build/bonsai-http-argmax-integration-2100/frozen-source/trace`.
Use EXP-2091's `source/compare.py` with `--device-choice` to verify
the final candidate readback chain.

[Build provenance](data/provenance.json), [full pair and fixture results](data/results.json),
and [executed GPU path](data/attribution.json).
