# The build that was missing the unit: 1253 of 1256 doors, from tracked sources

Found and fixed on 2026-11-20, and it is the largest single result of the migration so far.

## The defect

`tools/build-from-tree.sh` never linked the composed Goose unit. The build succeeded, the artefact loaded
and answered, and it carried **11 unit doors** where the accepted artefact carries 200. That looked like
"path B produces a smaller runtime" - it was a missing file in a link line.

Evidence: the unit object defines **5045** door-like symbols, and none of them appeared in path B's library.
Every comparison for two days was made against an artefact that had no unit in it.

## The fixes, in the order they were needed

1. **link the unit** - and refuse to build without it:
   `no composed unit object ... a runtime without the unit is not the runtime`;
2. **compile a PIC copy** from the same generated C (`-O1 -fPIC -std=c11`): the staged tree builds the unit
   for an executable, and `ld` refused `R_X86_64_PC32` against `stderr` for a shared object;
3. **skip extra objects the unit supersedes** - q35_shape, q35_rope_multi, qg_split and goose_init were
   defined twice. The filter now reports each skipped object by name, so the list can shrink deliberately;
4. three `set -e`/`pipefail` traps on the way (`ls` with a missing glob, `comm` piped into `grep -q`, and the
   filter's own SIGPIPE), each of which ended the script with no message.

## The result

| | baseline `run-candidate` | rebuilt path B `out4` |
|---|---|---|
| `libllama.so.0` | 5 788 128 bytes | **10 490 880** |
| defined symbols | 6090 | **7701** |
| unit doors carried | 200 | **1253 of 1256 (99%)** |
| doors the baseline has that this lacks | - | **0** |
| duplicate globals | 0 | **0** |

Everything the baseline carries is in this artefact, and 1053 doors more. It is built by one script from
tracked sources, which is what `REPRODUCTION-VERDICT-20261119.md` said did not exist.

## What it still misses, named

Five accepted components are absent, and each is a specific reason rather than a mystery:

- `goose-shim-marker` - the shim's diagnostics, which come from the carrier set;
- `goose-dry-apply` - DRY steps 2-4 through the shipped door;
- `goose-bulk-bridge` - the bulk bridge the ggml base library links against;
- `gguf-reader-call-path` - `FG_GGUF_DIAG`, emitted by the loader this build does not compile;
- `goose-spec-synth` - added to a `server-context.cpp` this build does not compile.

So the remaining work is not "find the modules" any more: it is the carrier and bridge set, which is a
shorter and much more concrete list than what this morning had.

## What is not claimed

That it runs, or that it is as fast. The artefact is a library, not a runnable set - it needs its libraries
and a matching server - and neither has been assembled or measured. The next step is that, and the four axes
decide whether this becomes the baseline.
