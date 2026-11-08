# EXP181: MM IDs helper — not qualified, blocker recorded

Status: NOT QUALIFIED. No correctness or performance claim. The candidate builds and
links, the Goose path executes, and it is byte-inexact against native, so it is not
promoted and the active runtime stays at EXP179.

## What is implemented

`ggml_cuda_launch_mm_ids_helper` for the `n_expert_used == 8` specialisation that the
Apex profile instantiates (the census shows `mm_ids_helper<(int)8>`). Goose owns the
warp/token slot decode, the guarded id read, the used-expert compare, the padded slot
count, the 8-wide `any` reduction, the `shfl_up` scan over token positions, the
running compact counter, the shared-memory round trip through the packed
`(it, iex_used)` word, the write of both the forward and the inverse map, and the
expert-bound publication.

Extern imports: the shared-memory accessor, four shuffle primitives, the global
int32 load/store and the sync. No original kernel, GGML struct or C++ routine is
reached.

Rollback: `GGML_GOOSE_CUDA_MM_IDS=0` selects the private-renamed native launcher in
the same DSO. Base lineage EXP180, so every earlier module is unchanged, and the
export surface differs by exactly the two diagnostic readers.

## Where it stands

| check | Goose | native |
|---|---|---|
| fixture runs without a CUDA error | yes | yes |
| `expert_bounds` monotone | yes | yes |
| `expert_bounds` final entry | 31 | 32 |
| bound entries for experts 0..7 | 0,7,11,17,20,24,26,30 | 0,7,11,17,20,24,26,30 |
| `ids_dst` populated rows | ~1 per expert | one per used slot |
| byte-identical outputs | 0 of 15 files | — |

So the per-expert prefix counts are right, and both the running compact counter and
the shared-memory staging are wrong: only about one row per expert survives instead
of one per used (token, slot) pair.

## Defects in my own work, found and fixed

1. The shared buffer was addressed as if each block had a distinct window slot; it
   is one `extern __shared__` window per block, so the slot argument is inert. This
   is now documented in the bridge but was not the cause of the mismatch.
2. The closing bound was written by block 0 instead of the last block. Native
   writes `expert_bounds[gridDim.x]` from the last expert only. Fixed; this moved
   the final entry from 1 to 31.
3. `nex_prev` originally skipped the out-of-range lanes entirely; the native code
   still runs the compare for them, where `INT_MAX` contributes nothing. Documented
   and kept, since the resulting bounds for experts 0..7 now match native exactly.

## The blocker

`it_compact` does not accumulate the number of used slots per pass. The native
sequence is

```
const int it_compact_add_self = warp_reduce_any<neu_padded>(iex_used != -1);
int it_compact_add_lower = 0;
for (offset = neu_padded; offset < warp_size; offset += neu_padded) {
    const int tmp = __shfl_up_sync(0xFFFFFFFF, it_compact_add_self, offset, warp_size);
    if (threadIdx.x >= offset) it_compact_add_lower += tmp;
}
if (iex_used != -1) store[it_compact + it_compact_add_lower] = ...;
it_compact += __shfl_sync(0xFFFFFFFF, it_compact_add_lower + it_compact_add_self, warp_size-1, warp_size);
```

and the Goose body reproduces it line for line, with `red_any8` as the 8-wide xor
ladder and `shfl`/`shfl_up` passed through unchanged. Yet only about one row per
expert is staged, so either the 8-wide `any` returns the wrong lane value or the
`shfl_up` broadcast inside the emitted code does not behave as the source suggests.

Those two primitives were then isolated in `tests/probe.cu` and both behave
exactly as CUDA documents: the 8-wide xor ladder flags lanes 3 and 12 for an input
pattern with those two set, and `shfl_up` at offsets 8/16/24 shifts by the expected
lanes. So the primitives are correct and the defect is in how the emitted body uses
them.

Measured with the fixture's `e8-t4-fwd` case (4 tokens, 8 experts):

| | native | Goose |
|---|---|---|
| `expert_bounds` | 0,7,11,17,20,24,26,30,32 | 0,7,11,17,20,24,26,30,31 |
| rows written to `ids_dst` | 23 | 4 |
| rows per expert | 4,4,4,3,4,3,3,3 approximately | 1 |

Native writes one row per matching (token, slot) pair, so expert 0 gets the four
pairs (0,2),(1,0),(2,1),(3,2). Goose writes one row per expert, which means the
running compact counter `it_compact` reached 1 instead of 4 by the time the write
loop ran, even though the closing bound shows 31 versus 32 -- one short, consistent
with the same counter being one instead of the full count in the last block.

So the remaining question is narrow and specific: why does the emitted body's
`it_compact` advance by one per pass instead of by the number of matching slots.
The probe rules out the shuffle semantics, and `add_lower`'s guard is present in the
emitted text, so the next thing to inspect is the generated variable the loop
condition reads -- the write loop looks for a scalar the emitter may have introduced
per pass rather than the outer one.

## State

Local gate: `FG181M_GPU_FIXTURE` runs in both arms, 20 checks, 5 failures in the
Goose arm. Compute-sanitizer memcheck reports 0 errors, so this is a logic defect,
not a memory one. No Apex measurement was taken, nothing was promoted, the active
runtime is unchanged and production and the compiler are untouched.
