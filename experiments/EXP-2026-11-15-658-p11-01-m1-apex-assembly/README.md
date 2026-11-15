# EXP-2026-11-15-658 — M1: Apex runtime assembled and linked, not run

Base: `050354e9`. Isolated tree; the repository was not modified. No model,
server or GPU run happened.

## Result

| check | OFF | ON-all |
|---|---|---|
| tree build | ok, 2 upstream warnings | ok, 2 upstream warnings |
| `llama-server` linked | 17 794 640 B | 20 316 528 B |
| accepted gates | 10/10, `goose_calls=0` | 10/10, `goose_calls` match the cards (30 / 394 / 31 / 53 / 182 / 74 / 40 072 / 2 572 / 84 / 376) |
| `goose_init` in the 12 binaries | 0 | exactly 1 |
| 55 switch exports | 0 | max 1 definition each |
| `op_sqrt` | absent | absent |
| init under threads | — | 2 threads × 256 calls → `goose_init_runs=1` |

The Apex reference binary is untouched (`badbfc5cf63895a9…19f10d26`, mtime
12.11).

## Composition

The unit is deterministic: from the base module list it reproduces
byte-identically (`7f29ca5e…`). On the current HEAD the extern count rises from
99 to 136, but all 37 new symbols are libc/POSIX: no new Goose helper appears, so
the adapter is reused unchanged.

## Seam

The unit and the host adapter are compiled **into `ggml-base`**, not into a
separate library. The unit reads the native IQ tables through 14 entry points,
and those tables are file-static in `ggml/src/ggml-quants.c`; a separate target
would create a static-library cycle. `ggml-base` is already on the link line of
`llama`, `llama-common` and every gate, so there is exactly one `goose_init` and
no cycle. Handing the tables in at init instead of linking them is the next step.

Side changes: three duplicate host hooks are removed from the bridges
(`gs_json_strtod`, `mt_fmt_g6`, the 54 quant primitives) - the adapter now owns
them; the IQ accessor guard in `ggml-quants.c` is widened to `FG_GOOSE_RUNTIME`;
`LLAMA_BUILD_TOOLS=ON` is required in this revision for the `llama-server`
target to exist.

## Files

- `REPORT.md` — full English report (commands, OFF/ON, gates, nm, hashes, risks).
- `gen_host.py` — emits the one host adapter from the unit's extern prototypes
  and the undefined-symbol list; the 14 IQ table views are emitted as a fragment
  for `ggml-quants.c`.
- `host/goose_host.c` — the generated adapter (`0af6a592…`).
- `run-off/`, `run-on/` — gate outputs and CSVs.
- `evidence/` — build/configure logs and the base module list.

## Reproduction sketch

1. Copy the CPU-only tree (the staging tree from EXP626) into a scratch dir.
2. Compose the unit from `modules-base.txt` with `tools/compose-goose.py`.
3. Run `gen_host.py` against the unit to produce `goose_host.c` and the IQ
   fragment.
4. Add the unit, adapter and fragment to `ggml-base` behind ten OFF-by-default
   flags; build OFF and ON-all; link `llama-server` in both.
5. Run the ten accepted gates and the nm checks; do not start the server.

## Boundaries

No model was run, so no performance or correctness claim is made here. The next
step is the A/B protocol in `docs/goose-migration/APEX-TEST-PLAN.md`, which needs
Igor's approval.
