# EXP300 (record): P5-03 closure evidence -- ISWA / DSA / MSA / DSV4 mapping

Criterion: "All ISWA/DSA/MSA/DSV4 combinations and attention masks/position updates mapped; native-supported modes not
silently dropped."

## Every family, with its gate

| family | what is mapped | gate |
|---|---|---|
| ISWA | SWA cache size (min(base, n_swa*(unified?n_seq_max:1)+n_ubatch), padded to 256, swa_full override), base/SWA layer filters (external AND the layer SWA bit), delegation (seq_rm is an AND, others go to both), observers read the SWA cache, breakdown merge in key order | EXP298: 10 412 checks, 8/8 |
| MSA | derived indexer hyperparameters, n_stream = unified ? 1 : n_seq_max, two independent filters, observers read the main cache | EXP299: 14 099 checks, 7/7 |
| DSA | indexer MQA with one key head, indexer head width, rope forced to NeoX, two filters, observers on the MLA cache, can_shift with size equality, state_read as a transaction clearing the MLA part on a throwing indexer read | EXP300: 16 495 checks, 8/8 |
| DSV4 | compressed-state helpers, non-overlap plan, overlap index layout, restore/snapshot planes, clear/seq_cp/apply_copies, stream ids, tensor I/O plan, compressed-state restore, construction policy | EXP301-308: 9 000 / 87 911 / 124 118 / 162 768 / 17 780 / 9 924 / 5 000 / 31 432 checks, 8/8 each |

## Attention masks and position updates

- Masks: EXP316 (float path, empty/sequence/causal/SWA drops for all three SWA types, row-reuse), 132 213 checks 8/8;
  EXP317 (ALiBi, M-RoPE causal comparison, non-causal SWA span), 115 277 checks 8/8; EXP318 (K/V view shapes, strides,
  stream offset, n_kv padding), 29 000 checks 8/8.
- Position updates: the per-family observers above, on top of the cell-level position operations closed in P5-01 (EXP199,
  EXP289) and the sequence extremes repaired in EXP293.

## The one item that was still open, resolved

`init_batch` split policy is not missing -- it is gated elsewhere, in the batch work: EXP330 ports the transactional
`init_batch` (always-sequential equal split, rollback window, drain) and `prepare` (snapshot, ask each ubatch, restore on
failure), and EXP336 adds the hybrid-indexed split rules. So the split rules are covered, and they belong with the batch
allocator as the evidence said, not with the cache families.

## Native-supported modes not silently dropped: the dormancy statement

Apex instantiates none of these four families: its model has no sliding-window and no indexer layers, so a run prints no SWA
or indexer counters and never takes these paths. The coverage for ISWA/MSA/DSA/DSV4 is therefore fixture-based, and this is
stated rather than implied. What the gates establish is that each native-supported mode has a named, checked policy with
mutations caught; what they do not establish is live traffic, because the accepted profile cannot produce it.
