# EXP241: `ubatch_add` classification in Goose — ACCEPTED

The block that decides what kind of ubatch to build and how large its three arrays are
(`llama-batch.cpp:871-887`), inside `ubatch_add`.

## What it preserves

    mixed_batch = !is_embd_vec.empty()
    n_embd_rows = sum of is_embd_vec[idx] over idxs          (0 when !mixed_batch)
    mixed     = mixed_batch && n_embd_rows > 0 && n_embd_rows < n_tokens
    use_token = batch.token && !(mixed_batch && n_embd_rows == n_tokens)
    use_embd  = batch.embd  && !(mixed_batch && n_embd_rows == 0)
    n_embd_all  = use_embd  ? n_tokens*n_embd       : 0
    n_state_all = has_state ? n_tokens*n_embd_state : 0
    n_pos_all   =             n_tokens*n_pos_per_embd

Two asymmetries that are easy to invert and hard to notice:

- `use_token` is suppressed only when **all** rows are embeddings; `use_embd` only when **none** are.
  Swapping the two equality tests yields a ubatch of entirely the wrong kind for a batch that is entirely
  one kind — and the token count still matches, so nothing downstream complains.
- `n_pos_all` is **unconditional**. A ubatch of embeddings still carries `n_pos_per_embd` per row; zeroing
  it makes the positions array short.

`n_embd_all` is multiplied by `n_tokens` while `n_pos_all` is not, because `n_pos_per_embd` is already
per-token. That asymmetry is in the native code and is preserved rather than "fixed".

## Gate

    160048 checks, 0 failures

6 named cases — token-only, embd-only, all-embd rows (the `use_token` suppression), none-embd rows (the
`use_embd` suppression), genuinely mixed, and a single-token case for the unconditional `n_pos_all` — plus
20000 random ones with `n_embd`, `n_embd_state` and `n_pos_per_embd` varied independently.

Mutations:

| mutation | failures |
|---|---:|
| `use_token` row test `== n_tokens` → `== 0` (the two asymmetries swapped) | 2488 |
| `n_pos_all` drops the `n_pos_per_embd` factor | 15013 |

## Safe integration

The cross-check is deliberate: the module returns `n_embd_rows`, and the C++ side only adopts the
classification when that matches what the native loop computed. A disagreement falls through to the native
values, so a wrong port degrades to the previous behaviour instead of building a malformed ubatch.

## Live

    FG241U_UBATCH_CLASSIFY calls=146
    FG240Q_BATCH_SEQSET   calls=146
    prefill 2490.3  decode 25.23
    tokens True  probabilities True  sha e68df05b

Witness superset vs `exp240`: no missing, `new FG241U`. All five batch routes verified present in the object
before linking (`fg237s fg238q fg239b fg240q fg241u`).

## Two process notes

- The first run of this stage wrote its results into `runtime/exp240/results4` because I derived the runner
  from the previous stage's and only replaced some of its paths. The run itself was fine (2491.6 / 24.92,
  tokens exact); only the destination was wrong. Regenerated the runner with explicit `exp241` paths.
- That is the same class as the `run2/run3/run4` mistake in EXP240: I keep assuming a derived artefact has
  been fully retargeted. Grepping the generated script for the new stage name before launching costs one
  command.
