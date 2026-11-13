# EXP242: `out_ids` selection and `seq_idx` mapping in Goose — ACCEPTED

Two decisions at the tail of `ubatch_add` (`llama-batch.cpp:937-957`). Both are pure decisions over arrays
that already exist; nothing is copied.

## What it preserves

    for i in idxs: if (udata->output[i]) out_ids.push_back(idxs[i]);
    for s in 0..n_seq_max: if (seq_set_unq.test(s)) { seq_idx[s] = seq_id_unq.size(); seq_id_unq.push_back(s); }

Two traps:

- **`out_ids` order defines the logit correspondence.** It is the list of batch indices whose logits the
  caller reads. A wrong order still returns the right number of rows with plausible values.
- **`seq_idx` is assigned from `seq_id_unq.size()` BEFORE the push**, so the first unique sequence gets 0.
  Assigning from size() *after* the push shifts every value by one, and every sequence then reads the wrong
  slice of the sequence dimension.

## Gate

    1680413 checks, 0 failures

10 named cases plus 10000 random for each of the two functions. Named cases pin: order follows `i` and not
the index value, all/none output flags, and — for `seq_idx` — every word boundary (63, 64, 255) plus the
four-word spread `{0, 64, 128, 192}`.

Mutations:

| mutation | failures |
|---|---:|
| `seq_idx` assigned after the increment (the off-by-one) | 15004 |
| `out_ids` stores the position `i` instead of `idxs[i]` | 21112 |

## A test precondition I got wrong first

The first gate run failed with 1 587 876 errors: the fixture expected `-1` for absent `seq_idx` entries but
the module only *writes* the present ones. Native fills the array with `-1` at
`udata->seq_idx.resize(LLAMA_MAX_SEQ, -1)` before the loop, and the module replaces **the loop, not the
initialisation**. The correct fix was to pre-fill the fixture with `-1` so the pre-state matches native.
Changing the module to write `-1` for absent entries would have made the port do work the original does not.

## A bug I introduced and caught before committing

The first integration extracted the four union words with `seq_set_unq.to_ullong()` and shifts of its
result. `to_ullong()` **throws** `std::overflow_error` once any bit above 63 is set, and shifting its 64-bit
result cannot reach those bits in any case. Replaced with a bounded bit walk —

    for s in 0..min(n_seq_max, LLAMA_MAX_SEQ): if (seq_set_unq.test(s)) u[s/64] |= 1LL << (s%64);

— which cannot throw. The gate would not have caught this, because it tests the module and not the call
site; it was caught by reading my own integration code again.

## Live

    FG242O_UBATCH_OUTIDS calls=292        (146 pick_outids + 146 map_seqs)
    FG241U_UBATCH_CLASSIFY calls=146
    prefill 2489.9  decode 24.57
    tokens True  probabilities True  sha e68df05b

Witness superset vs `exp241`: no missing, `new FG242O`. Six batch routes verified present in the object
before linking.
