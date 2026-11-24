# EXP-2026-11-24-1854 — Flash causal inlining: 264-byte frame removed, prefill +2.68 % certified

Status: **ACCEPTED as the new baseline of the kernel lineage.** The accepted
`37ac0f1d` binary is untouched and remains the accepted release baseline; this gain
belongs to the lineage whose image is built from the current tree.

## The change (registry EXP-1609)

`goose_flash_causal_mma` is the largest single kernel of the prefill (479 ms of 3.7 s)
and carries a 264-byte stack frame with 158 `LDL` and 172 `STL` at 255 registers. The
registry's EXP-1609 result for the same entry is that the causal path passes constants
through a large shared helper and the CUDA pipeline leaves an out-of-line call there:
inlining the body **and** the wrapper specialises the call, at the cost of growing the
Stream-K entry — which is off by default (`GOOSE_FLASH_STREAM_K=0`).

Implemented as a build-script step gated by `GOOSE_FLASH_INLINE_CAUSAL=1`, which marks
`process_tile_apex_nc8_np1_g` and `flash_causal_part_g` as `always_inline` in the
`flash-mma` fragment. Only the image changes; archive, driver, flags and environment are
those of the lineage.

## Kernel-level verification (four levels)

| property | lineage image | inlined image |
| --- | ---: | ---: |
| `goose_flash_causal_mma` stack frame (ptxas) | **264 B** | **0 B** |
| registers | 255 | 254 |
| spill stores / loads | 0 / 0 | 0 / 0 |
| `call.uni` in the entry (PTX) | 2 | 1 |
| `.local` in the entry (PTX) | none | none |

## Runtime A/B, three alternating pairs

Same lineage binary family, only the image differs; each arm is a fresh server with a
warm-up and one measured 32768/512 request (`runs/inl-100945`, `runs/inlpair-101218`):

| pair | lineage prefill | inlined prefill | decode (lineage / inlined) |
| --- | ---: | ---: | --- |
| 1 | 2168.0 | 2230.6 | 72.881 / 72.591 |
| 2 | 2181.0 | 2221.9 | 72.533 / 72.888 |
| 3 | 2172.4 | 2235.2 | 72.988 / 73.142 |
| **median** | **2172.4** | **2230.6** | **72.881 / 72.888** |

**Prefill +2.68 %** (median), decode parity (+0.01 %), identical token ids
`71801bbe251e` and identical text `db22d3e02e5e` in all six runs, VRAM 9 906 MiB in both
arms, VmHWM 21 495 MB, swap 0.

## Verdict

Accepted. The kernel lineage's baseline is now the inlined image; the card, the binary,
the driver source and the image are preserved under
`checkpoints/exp1854-flash-causal-inline/` (`84e7d28d…` binary, `743dcf41…` image).

The change is a **build-time** one and therefore transfers to any future rebuild of the
accepted image; it cannot be applied to the accepted binary in place, which is why it is
recorded in the kernel lineage rather than as a change to `37ac0f1d`.

## Cumulative state

| lineage | prefill (median) | decode | ids / text |
| --- | ---: | ---: | --- |
| accepted release `37ac0f1d` | 3105.9 | 77.574 | `38abffce4131` / `f8664331bd30` |
| kernel lineage, before | 2172.4 | 72.881 | `71801bbe251e` / `db22d3e02e5e` |
| **kernel lineage, after** | **2230.6** | 72.888 | unchanged |

## Next kernel experiments (same lineage, same gate)

1. the remaining frames of the same fragment — `process_tile_apex_nc4_np2` and the
   value-prefill helpers, wherever `nvdisasm` still reports `LDL`/`STL`;
2. the registry's scalarization item on `goose_gdn_mma_v32` (80 B frame, 20/22 local
   accesses, 317 ms) and `goose_gdn_mma` (288 B frame, 68/86, unused in this profile);
3. fusion candidates from the registry: post-FFN residual (EXP-1717) and the vector
   plan's parameter-space lowering (EXP-1603), both already accepted upstream and worth
   re-verifying inside this lineage.

## Artefacts

- `checkpoints/exp1854-flash-causal-inline/` — binary, driver source, image, SHA256SUMS.
- `runs/inl-100945`, `runs/inlpair-101218` — six measured requests with ids and text.
- `tools/build-goose-cuda-kernels.sh` — the `GOOSE_FLASH_INLINE_CAUSAL` step.
