# EXP-2026-11-18-1177: P7-06 tokenizer merge order

P7-06's vocabulary loader already exists in `goose/model/vocab-type.goose`, which covers the
SPM, WPM, UGM and RWKV families and their fixed defaults. What the port still lacks is their
tokenizers, and both families walk a bigram queue - so the first thing to move is the order
in which that queue is drained, because it is the decision and it differs between the two:

- SPM: highest **score** first, ties by smaller left index, and the symbol size is **not**
  part of the key even though the struct carries it;
- BPE: lowest **rank** first, ties by smaller left index.

`src/spm_order.goose` expresses both as "a is popped before b".

```
spm/bpe order: fails=0
```

The gate does not compare against a re-typed comparator. It builds the real
`std::priority_queue` with the native comparators, pops it, and requires the Goose predicate
to agree with every consecutive pair. It also checks antisymmetry over all 64 pairs of a test
set, which catches a comparator that returns "false" for both directions and would silently
turn the queue into a stack.

One of my test rows had two identical keys, and the first run failed on it - correctly, since
identical items are not ordered. Unique keys fixed the harness, not the port.
