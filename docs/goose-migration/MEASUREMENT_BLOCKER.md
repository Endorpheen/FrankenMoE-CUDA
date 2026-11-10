# Open: the paired Apex measurement cannot be completed with the current runner

Status recorded 2026-11-11. **EXP239, EXP240, EXP241 and EXP242 were committed without the paired
before/after run that rule 13 requires.** That is a violation, not an oversight to be glossed over, and it is
the reason this note exists rather than a green table.

## What was attempted

The pair was `exp238` (before the batch-port block) against `exp242` (after it), in the order
old → new → new → old, with only `libllama.so.0.7.0` swapped between runs.

## What blocked it

1. **Disk.** The host was already at 100% (`888G/938G`). Copying both stage `run` directories pushed it to
   `131M` free, and `cp` began failing part-way through a run directory, producing partial stages. Cleaned
   `runtime/exp240/run{_old,2,3,4}` (5 × 637M) and my `/tmp` scratch back to 6.0G free. **These stage copies
   must not be recreated** — switching DSOs by symlink needs no second copy.
2. **`setsid` without `--wait` returned immediately**, so the next run started while the previous server still
   held port 8081. Replaced with plain sequential invocation.
3. **The runner writes every run to the same result path.** Four runs with different `FG_SUFFIX` produced four
   distinct `runtime/exp242/measured/<label>/` directories, but a single
   `runtime/exp242/results/apex-oi242-oi/goose/summary.json`, overwritten each time. A fifth run then returned
   figures **identical to two decimals** with the previous one (2489.93 / 24.572), which is far more likely a
   stale file than a real coincidence.

## What is therefore not known

Whether the four batch ports of this block cost anything measurable. Every run in the block landed in the
2489–2496 prefill / 24.3–25.2 decode band, but that band is also what `exp238` shows, and without a valid
interleaved pair those numbers cannot distinguish "no regression" from "same load, same code".

## The fix, before any claim

Give each run its own result root instead of relying on `FG_SUFFIX` for the label only — a distinct
`FG_STAGE` (with its own `run` directory produced by symlink, not copy) makes `runtime/<stage>/results`
unique by construction. Then re-run the four-step pair and only afterwards state a percentage.

## Consequence for the ledger

`tasks.json` P6-07 currently records the ports as live and gated. That is verified and true. It does **not**
record a performance verdict for EXP239–242, and this note is the reason it should not be added until the pair
above exists.
