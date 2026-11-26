# EXP-2026-11-26-2109 — Close the Bonsai CUDA optimization stage

This is an offline comparison of saved data. No model run, new
CUDA build or change to a working checkpoint was needed.

## What transferred from ordinary decode to MTP

RTX 4070, Bonsai PQ2_0 MTP GGUF, 32K/512, four-row verifier and
greedy with EOS masked. Both saved EXP-2047 CLI answers match all 512
EXP-2106 output IDs. EXP-2106 also matches the PrismML text.

| Saved result | Prefill, tok/s | Effective MTP decode, tok/s | Blocks | Accepted drafts |
| --- | ---: | ---: | ---: | ---: |
| EXP-2047 CLI, two-run mean | 618.10 | 72.12 | 145 | 365/435 (83.91%) |
| EXP-2106 HTTP, one unprofiled request | 1003.09 | 72.40 | 141 | 369/423 (87.23%) |
| EXP-2107 HTTP, candidate mean of two pairs | 1016.79 | 72.33 | 141 | 369/423 (87.23%) |

The CLI-to-HTTP difference of 0.28 tok/s is 0.39%. These are different
checkpoints and timing wrappers. It is not a scored performance delta.
The EXP-2106 work transferred primarily to target prefill. The saved
CLI mean prefill is 53.014 s compared with 32.667 s in the single HTTP
request, including draft cache initialization on both paths.

1. **The verifier does different work.** Ordinary EXP-2105 decode uses
   single-token matvec and ten-node FFN replay. EXP-2106 batched
   verification still uses EXP-2047's four-row PQ2 kernels
   `bonsai_mvq_4_permutation_{2,4}` and the prefix transaction.
   Nsight observes zero graph nodes in all 141 verification blocks
   and 640 in one ordinary tail step. The four-row projections
   account for 2.512 s of GPU time.
2. **The single-query Attention change did transfer.** With
   `BONSAI_DECODE_DIRECT_MMA=1` the EXP-2106 exact four-row dispatch
   deliberately bypasses the EXP-2047 shared-KV Attention branch.
   Nsight records 9024 calls to
   `bonsai_decode_direct_q4_mma_half2` inside verification, exactly
   141 blocks × 16 Attention layers × four queries. The calls use
   2.330 s. The ordered merge adds 0.112 s. The EXP-2047 source instead
   shares K/V reads across verification rows. The new path uses the
   cumulative ordinary arithmetic, with a separate K/V pass per query.
3. **Choice was already on device.** EXP-2047 selected both draft and
   target rows on GPU. EXP-2106's trace shows three 12-byte choices
   and one 48-byte readback per verification block. AVX2 still serves
   eligible host sampling, but it does not replace these greedy steps.
   The initial full vocabulary copy and the single ordinary tail
   result are outside the four-row verification loop.

The saved EXP-2047 logs report 5.886 s of host target timing on average,
or 40.59 ms/block. The EXP-2106 diagnostic trace spans 6.065 s from
third-draft readback to target readback across the 141 blocks, or
43.01 ms/block, with 5.845 s of kernel activity inside those windows.
The readback windows and host timers have different boundaries.
No raw EXP-2047 CUDA trace was found in the checkpoint store. The older
EXP-2035 trace validates the general MTP phase structure, but it used
three rows and cannot supply EXP-2047 kernel durations. The evidence
supports a dispatch difference and identifies current costs. It cannot
isolate a causal per-kernel old-to-new speed delta.

## Frozen checkpoints and open work

The [checkpoint manifest](../bonsai-checkpoints.json) identifies the stable
release EXP-2049, ordinary experimental EXP-2105 and MTP experimental
EXP-2107. Their local executable and CUBIN bytes were checked against
every recorded SHA-256. Source, testing scope, inherited quality risks
and known regressions remain with the linked experiment cards. The
[EXP-2106 receipt](../EXP-2026-11-26-2106-bonsai-http-mtp/data/results.json)
also pins its executable, image and source inputs. Preserve these files
with their build trees. No promotion or new release was made here.

- If CUDA work resumes, share Q4 KV loads between the four verifier
  queries while preserving current direct MMA and ordered reduction
  per query. Check bitwise logits, committed prefix state and rejects
  before a single full HTTP A/B. The 2.442 s attributed to verification
  Attention and merge is a budget, not the expected saving.
- The four-row projection has a 2.512 s measured budget. Treat Q4 draft
  prefill and ordinary decode matvec as separate scopes. The draft
  prefill speedup is already retained in EXP-2107. Gate/up CTA ordering
  was rejected in EXP-2108. Width-5120 FP16 PQ2 is still an open
  prefill opportunity at 4.982 s. EXP-2080 remains a separate
  numerical experiment with open quality questions.
- Compare against PrismML using matched modes before claiming a
  throughput gain. The 70% target and any release-wide cross-
  architecture performance requalification remain open.

[Derived trace table and archived CLI receipts](data/comparison.json).
Regenerate with `source/compare.py EXP2047_BUILD EXP2106_TRACE` using
the preserved logged outputs and exported SQLite trace.
