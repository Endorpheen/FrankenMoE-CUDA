# EXP-2026-11-24-1876 — Fuse the accepted MoE epilogue with the next RMSNorm

**REJECTED for the full Apex profile.** Control is EXP-1875, which
already fuses the MoE epilogue. The candidate also computes the
adjacent weighted RMSNorm in each 512-thread CTA. It writes both
the layer result and its normalization, preserving the original
FP32 operations and shuffle reduction. The new SM89 entry uses
23 registers, 64 bytes of shared memory and zero spill.

The isolated probe uses the archived reference RMSNorm and the
EXP-1875 epilogue cubin. Both outputs matched bit-for-bit across
five weighted and unweighted fixtures, including exact aliasing
of the two destinations with the routed and shared expert vectors.
On 4096 weighted rows, the median fell from 0.506 to 0.372 ms.
[Raw probe data](probe.log).

The read-only [graph audit](audit-rms.c) found 351 adjacent chains
of this shape on Apex. Each RMSNorm was weighted. The shared
vector's last use is in the removed epilogue. The graph planner
aliases the norm's host span with an earlier input in these cases,
so the dispatcher admits only records dead by the fused boundary.
The driver relabels the old routed and shared GPU slots and
retains the original path when a guard fails.

## Full Apex 32K/512 on RTX 4070

Context 262144, cache 6290 MiB, batch and ubatch 4096, Q4_0 KV.
B then A on the same executable, one warm-up and one scored
request per fresh server. A sets `GOOSE_MOE_EPILOGUE_RMS=0`;
B leaves it unset. The EXP-1875 epilogue stays active in both.

| Scored result | A control | B candidate |
| --- | ---: | ---: |
| Prefill tok/s | 3448.61 | 2965.43 |
| Decode tok/s | 81.24 | 83.56 |
| Prefill ms | 9501.80 | 11050.00 |
| Peak sampled VRAM MiB | 8774 | 8774 |
| Peak RSS KiB | 21491908 | 21492464 |

The prefill loss of about 14.01% exceeds the decode gain of about
2.86%. The same prefill direction appears in the unscored warm-up.
No reverse pair is required to reject this variant. All 512 IDs,
text and post-request SLRU and model properties matched exactly.
System and process swap-out were zero. B recorded 702 fused
invocations across the two requests. The cause of slower full
prefill was not isolated. A mechanism worth measuring is GPU slot
placement after relabeling the shared record, which changes the
allocator's downstream choice relative to a separate RMSNorm.
The control binary and ordinary activation policy remain at EXP-1875.

## Reproduction

Include [epilogue-rms.cl](epilogue-rms.cl) at the end of the frozen
EXP-1786 CUDA device translation unit. Use the Clang 18 and CUDA
12.6 flags from EXP-1859 and extract only `goose_moe_epilogue_rms`
for SM89. Name the result `fused.cubin`. Copy EXP-1875's exact
`epilogue.cubin` beside it. Verify [SHA256SUMS](SHA256SUMS) and run:

```sh
python3 probe.py "$exp1786/device/goose-cuda-mmq.ptx"
```

For runtime A/B, apply both [source patches](source) to EXP-1875.
Generate `moe-epilogue-rms-image.h` as a
`static const unsigned char moe_epilogue_rms_image[]` initializer
of the cubin bytes. Rebuild only the CPU unit and CUDA adapter
and relink the original archive as in EXP-1875.
Use the same env and argv, with the toggle only in A.
[Structured results](results.json) include exact run IDs and measurements.
