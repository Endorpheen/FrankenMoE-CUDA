# EXP237: `split_equal` expansion step in Goose — ACCEPTED on correctness

The loop that decides how many tokens each sequence set contributes to a ubatch
(`llama-batch.cpp:683-713`). It is the first P6-07 work: batching and ubatching, which had no Goose
coverage at all before this.

## What the port preserves

    while (true) {
        can_expand = all s: cur_idx[s] < seq_map[set[s]].size()
        if (!can_expand) break;
        for s: push idx; used[idx]=true; ++n_used; ++cur_idx[s];
        if ((idxs_per_seq[0].size() + 1)*n_seqs > n_ubatch) break;
    }

Two properties the gate holds:

- the loop adds **exactly one token per set per iteration, all-or-nothing** — it never advances one set and
  stops early on another;
- the size test runs **after** the push and uses `size()+1`, so a ubatch may exceed `n_ubatch` by up to
  `n_seqs-1` tokens before the break.

The second is the one that matters: an off-by-one there changes how batches are formed, which token-level
correctness cannot see. It changes order and performance, silently.

## Division of work

Goose decides **how many iterations** and where each cursor ends. The C++ side still performs the mutations
(`used[]`, `idxs_per_seq`, `n_used`), so the module owns only the control decision and no container crosses
the boundary. Four explicit cursors instead of an array — the logic is per-set and identical, and four
covers every branch.

## Gate

    89920 checks, 0 failures

7 named edge cases plus 20000 random ones. The named cases pin: `n_ubatch` exactly filled, `n_ubatch` of 0,
`(iters+1)*n_seqs > n_ubatch` on the first iteration, four sets, sets of unequal length where the shortest
ends the loop, and a single empty set producing zero iterations.

Mutations:

| mutation | failures |
|---|---:|
| `(iters+1)*n_seqs` → `iters*n_seqs` | 721 |
| advance one set before breaking on `!can_expand` | 19206 |

## Live

    FG237S_BATCH_SPLIT calls=146
    prefill 2453.2  decode 24.77
    tokens True  probabilities True  sha e68df05b

146 calls, one per ubatch. Witness superset vs `exp233`: no missing, `new FG237S`. Route prefix verified in
`llama-batch.o` with `nm -u` before linking.

## Performance

Not measured as a pair. **Прогон выполнен при параллельной нагрузке. Показатели производительности могут
быть искажены.** One run at 2453.2 / 24.77, in the same band as everything else today. The module replaces
a loop whose body runs once per token per set, so a per-call boundary is amortised — but that is reasoning,
not a measurement.

## Integration note

I used a `goto` to skip the native loop, which is the same construct that failed in EXP228 — there it jumped
over an initialisation and the compiler rejected it. Here the label sits outside the loop's scope with no
declaration between, so it compiles; I checked that specifically rather than assuming the earlier failure
was about `goto` itself.
