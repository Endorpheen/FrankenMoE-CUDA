# EXP-2026-11-24-1868 — Reuse the prefill GDN gate exponentials

**Stand-alone improvement confirmed. Full-profile adoption is
unverified.** The full-window prefill trace records 589.08 ms across
240 launches of `goose_gdn_mma_v32`. Reuse `exp(gate_prefix[t])`
and `exp(gate_prefix[last] - gate_prefix[t])` from a pair of
16-element shared tables. Warp zero computes them using the original
arguments and its existing scan. The current synchronization
orders the table stores and consumers. This adds 128 shared bytes
per CTA without a new buffer or CTA barrier.

The full-block variant also fixes the group length at 16 and uses
aligned Q/K vector loads, including the original prefetch of the
next Q/K half. It is only valid for complete 16-token groups with
16-byte-aligned Q and K at every accessed row address. The local-load
variant computes each Q/K half in the iteration that uses it.
All use the original tensor operations, prefix-scan order and state
update order.

## RTX 4070 isolated probe

Each median uses six alternating blocks of 20 launches for nt=4096,
32 value heads, 16 key heads and 128 hidden elements. Relative times
use the control from the same diagnostic block.

| Variant | Reg/thread | Entry stack | Static spill loads/stores | CTA/SM | Median ms | Time change |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Reuse only | 128 | 80 B | 0/0 B | — | 2.3850 | −1.74% |
| Full block | 129 | 80 B | 0/0 B | 1 | 2.8754 | +18.16% |
| Full block, 128 cap | 128 | 96 B | 12/12 B | 2 | 2.3238 | −4.50% |
| Full block, load in use iteration | 128 | 80 B | 0/0 B | 2 | 2.3552 | −3.02% |

The corresponding control medians were 2.4272, 2.4334 and 2.4285 ms.
Reuse-only validated five shapes including one- and fifteen-token
tails. The full-block variants validated three aligned shapes.
All score and final-state FP32 words matched. Peak model VRAM, RAM
and full Apex speed were not measured. A direct extrapolation of
the full-block gains to the trace would save about 18–27 ms,
subject to profile coverage and overlap. The accepted checkpoint
remains EXP-1863.

## Reproduction

The control Goose fragment rebuilds both frozen GDN headers
byte-for-byte using KeelWing. Each [source patch](source) is against
that original `gdn-mma.goose`. Recompile the device fragment and the
frozen EXP-1786 device tree with the KeelWing and Clang 18 commands
from [EXP-1859](../EXP-2026-11-24-1859-flash-interior/README.md).
Extract only `goose_gdn_mma_v32` using CUDA 12.6 ptxas on SM89.
The control comes directly from archived MMQ PTX. For full128,
compile the full-block PTX again with `--maxrregcount=128`.
Verify cubins via [SHA256SUMS](SHA256SUMS). Each Python probe takes
the named cubins from the same directory. The raw timings and
residency are in the logs and [results.json](results.json).

Any runtime integration needs a guarded full-block dispatch and
the original entry for ragged or unaligned inputs. It must keep the
accepted context, cache, output and state checks.
