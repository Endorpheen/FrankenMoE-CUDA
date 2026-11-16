# No surviving artifact can explain the 42 tok/s point

Searched for a preserved library from the guarded2_512 measurement (42.01 decode / 2436.63 prefill, 13:13-13:14).
There is none, and the two artifacts that do survive are strictly older subsets of the working library.

## What exists versus what is needed

| artifact | date | bytes | symbols | fg/slru only here |
|---|---|---|---|---|
| working `libllama.so` | 16.11 15:33 | 5 661 672 | 6 496 | - |
| `/tmp/libllama-ours.so` | 15.11 22:05 | 5 575 776 | 6 372 | 0 (31 only in working) |
| checkpoint `CONTROL-OPT-20261112` | 12.11 04:37 | 5 533 896 | 6 332 | 0 (51 only in working) |
| the 42-era build | 16.11 ~13:13 | 5 652 072 | - | does not exist |

Checked two ways: a filesystem search by exact size (5 652 072 bytes, no hits) and the inventory of 821 candidates
(`/tmp/franken-libllama-candidate-inventory-20261116.json`, no entry that size, and no entry with a 16.11 12-13h
mtime). The inventory was captured before today's later builds, so it could not have held it; it corroborates the
absence rather than causing it.

## What the comparison settles

Both surviving artifacts are strict subsets of the working library: they contain no symbol the working one lacks,
and the working one has 31 and 51 extra Goose/SLRU symbols respectively - including `fg756s_*` (the SLRU expert-cache
policy), `fg_gr_*` (graph reuse) and `fg_arch_registry_goose_init`.

So neither is a candidate for restoring a faster state. `abfe9400` predates SLRU, which is the change that raised the
expert-cache hit rate from 69.3% to 83.7%; it is earlier and less capable, not faster. The checkpoint is older still.

## Consequence

Restoring the 42 point cannot be done by recovering a file. It would require rebuilding that exact object set, and
those objects were overwritten - which is precisely the gap the runtime-store now closes for the future, but cannot
close retroactively.

Recorded so this search is not repeated: the answer is "no artifact", established by size search, by inventory
query, and by symbol-set comparison.

Co-Authored-By: CommandCodeBot <noreply@commandcode.design>
