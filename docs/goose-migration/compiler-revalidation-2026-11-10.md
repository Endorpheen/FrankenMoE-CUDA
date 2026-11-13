# Compiler revalidation, 2026-11-10

Every accepted module was regenerated and re-gated with `subset-d5e9fdc5233e52b8`, the newest
qualified build, to check that nothing already accepted breaks under it. The runtime pin is
**unchanged** (`subset-84f31960f9f3031e`): this is a forward-compatibility check on the modules,
not a chain rebuild, and the pin only moves when a whole chain is accepted on a build.

| module | gate | result |
|---|---|---|
| EXP199 KV cells | `FG199K_FIXTURE` | 74,123 checks, 0 failures |
| EXP202 tokenizer | `FG202T_FIXTURE` | 1,965 checks, 0 failures |
| EXP203 MMQ dispatch | `FG203D_FIXTURE` | 593,237 checks, 0 failures |
| EXP209 incremental seq bounds | `FG209I_FIXTURE` | 40,877 checks, 0 failures |
| EXP210 recurrent seq_keep | `FG210K_FIXTURE` | 16,410 checks, 0 failures |
| EXP213 detokenization piece copy | `FG213P_FIXTURE` | 1,271 checks, 0 failures |
| EXP215 UTF-8 codepoint decode | `FG215U_FIXTURE` | 1,596 checks, 0 failures |
| **total** | | **729,479 checks, 0 failures** |

Build verified before use: sha256 `d5e9fdc5233e52b8…5785782`. Its headline feature was also run,
not read: `fn find(xs:i64[:])->i64{ each_rev(xs){ if it==3 { return it; } } return -1 }` compiles
and returns 3 through a generated C host that calls `gg_init_globals` and passes the returned base.

Note on the run: the first attempt failed on all seven with "cannot read …/source/*.goose", because
the compiler resolves relative paths against its own working directory. Absolute paths fix it — the
same constraint the build packet states for the pinned stdlib.

## What this does not say

It does not say the chain is accepted on this build. The witness chain, the Apex run and the
interleaved performance pairs would all have to be redone on a rebuilt chain, and the live numbers
in `apex-history` belong to the pinned build. This file records module-level compatibility only.
