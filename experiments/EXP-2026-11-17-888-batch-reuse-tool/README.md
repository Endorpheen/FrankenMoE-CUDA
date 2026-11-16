# EXP-2026-11-17-888 — a guarded batch tool for the graph-reuse family, and why it refused to write

Igor's standing order: port C++ to Goose in the largest safe batches, whole subsystems at a time, with cheap
local checks during the batch and full model checks once the batch is finished. This is the tool for the batch
that remains - 14 `can_reuse` conjunctions - and an honest record of its current state.

## Why a tool and not an edit

The first attempt at this batch located function bodies by counting braces. It produced a file that **compiled
cleanly** while the per-function row counts no longer matched the original scan, which is the signature of a body
being spliced into the wrong function. Nothing would have caught that on the path Apex exercises; the other
thirteen architectures would have been silently wrong.

`tools/rewrite-graph-reuse.py` replaces that with assertions that fail closed:

| guard | what it prevents |
|---|---|
| body found by exact signature + the next `\n}\n` at column 0 | nesting inferred from braces, which is what went wrong |
| `res &=` count must equal the scan's count | a body that was not the one intended |
| after rewriting, count unchanged and exactly one `fg_gr_check` | a half-applied splice |
| total `res &=` count unchanged across the batch | a lost or duplicated row |

The write happens only after all of them pass. A run that fails an assertion leaves the file untouched.

## It refused, and that is the useful part

`--check` produces the correct plan: 14 functions, 85 `res &=` lines, row counts matching the independent scan
(`rs` 4, `attn_kv` 3, `kv_msa` 3, `k_dsa` 4, `k_dsa_iswa` 2, `kv_iswa` 6, `k_iswa` 4, `dsv4` 5, `mem_hybrid` 7,
`mem_hybrid_k` 6, `mem_hybrid_iswa` 10, plus the two single-row cases).

Applying it aborts on the last function. The first abort was real and mine: the inserted comment contained the
literal `res &= `, which the post-rewrite count then saw. That was fixed. A second abort remains and is **not yet
diagnosed** - the counts checked by hand for `mem_hybrid_iswa` agree (10 and 10), so the mismatch is in how the
plan's offsets are reused across the loop rather than in that function's rows.

Recorded rather than worked around: `git status` confirms `llama-graph.cpp` is untouched, and the working
runtime is the committed state with the verified `c2ff42cc1219`.

## Next

Find the offset-reuse fault, get a clean apply, then one full Apex run for the whole batch - not one per
function. That is the batch Igor asked for, and the tool is what makes it provable instead of hopeful.
