# EXP924: bounded readonly expert mappings

**Historical reference.** This note records an Edge0 experiment from the Flint lineage. See [the model registry](../../docs/goose-migration/MODEL-ARCHITECTURES.md) for current qualification.

Reference EXP92344276ead1667a2c9bacfa13c461132b20be55792; six-ELF checkpoint
edge0-projection-checkpoint-20261117 immutable. New host-only profile75/256:
decode37.835605s, CPU22.557858140s, expert pread5.634450233s, graph0.414696322s,
PLE0.015487280s. DU/dense copies0, cache92385hits/30015misses. Actual matvec
nodes606398 vs baseline673200 across12240 FFN calls:66802 D projections removed.
Profile own/global swap0; timing remains diagnostic rather than performance A/B.

Hypothesis: use bounded MAP_PRIVATE/PROT_READ file regions for resident expert
records, replacing pread into staging plus private-cache memcpy. Same Goose LRU
admission/hit/promotion/victim policy and selected-record protection. mmap does
not load pages by itself; faults occur on actual matvec access and move cost
into CPU compute. Only complete request speed can accept the change. Optimistic
Amdahl bound1/(1-5.63445/37.8356)=1.175; real gain UNKNOWN.

3072MiB remains the total virtual mapped-region ceiling. Each record mapping
starts at its floor-aligned file offset; the worst aligned span is round_up
(1164800+page_size-1,page_size). Slot count uses this bound (slightly fewer than
the old2765 slots), never an unbounded full-file mapping. On eviction unmap the
old region before allocating the replacement. Publish tags only after successful
mapping; a failed mapping stops the candidate. No writable model mappings. Cache0
keeps the qualified uncached pread fallback. No expert payload survives its slot
or an active synchronous graph. Control FRANKEN_EDGE0_EXPERT_MMAP=0 keeps the
old private cache; mapped mode is default only after acceptance. No precision,
route,projection or model output changes. No new RAM/VRAM quota.

First full FFN probes: default/legacy/tiny-cache16MiB/cache0/material-rebind.
Compare fixed output/route tolerance and repeat bits. Then one diagnostic256
and short EOS, followed by serial A-B-B-A75/256, max8min for the paired block.
All256 IDs and answer bytes must equal original CPU control; normal EOS; primary
decode median improvement>=5%, no significant prefill/RAM/VRAM regression,
process swap0/global swap-out delta0. Log own minor/major faults and CPU times.
Do not count moved page faults as removed work or choose the fastest sample.
Stop on a failed correctness/ownership/budget gate. Accepted controls untouched.
