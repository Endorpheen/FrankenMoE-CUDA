# P4-03: base-library reproducibility — findings and route

Owner decision: do not accept a slower rebuilt baseline. Find the cause of the
non-reproducibility, restore the original artifact, and make the build
reproducible while keeping the verified performance.

## Finding 1: the link is deterministic

The recorded recipe for the November variant exists in
`experiments/EXP-2026-11-07-147-goose-buffer-audit/parent-exp146/build-receipts/base-link-command.json`.
Re-running it with its exact object list:

    g++-13 -fPIC -O3 -DNDEBUG -shared -Wl,-Bsymbolic -Wl,-soname,libggml-base.so.0 \
      -o out.so base-ggml.o base-ggmlcpp.o base-ggmlalloc.o backend-base.o \
      base-backendmeta.o base-opt.o base-threading.o base-quants.o base-gguf.o \
      backend-base-goose.o -lm .../libgomp.so .../libpthread.a

produces `115d748ba33636de2a41258ec76441401172536e7672fd15eceb2d4b8df27bc7`, which
is exactly that variant's recorded base hash. Same objects plus same argv give
the same bytes on this host.

## Finding 2: the divergence is at object level

The accepted baseline loads
`runtime/reconstructed/build/stage-run/libggml-base.so.0`,
sha256 `e213fa19…`. That file is a later integration than the November variant, so
its object set differs. Because the link is deterministic, every difference in the
deployed bytes comes from the objects that were linked — not from the linker, not
from a timestamp, not from the output path.

The blocker record already states the consequence: five Goose objects
(`shared-core.o`, `shared-meta.o`, `allocator.o`, `buffers-goose.o`,
`stream-goose.o`) were missing from disk and were rebuilt from their recorded
steps, while the scheduler pair still matched its deployment name hash. That is
the root cause of "a faithful rebuild changes the numbers": part of the deployed
object set no longer exists.

## Finding 3: the artifact is safe

A byte copy of the deployed base is preserved at
`runtime/baseline-artifacts/libggml-base.so.0.e213fa19` and must not be replaced.
Restoring the baseline therefore means keeping and loading this file, not
re-deriving it.

## Route to a reproducible base

1. **Archive objects, not only sources.** For every accepted base, store the
   linked object list with per-object sha256 and the exact argv. The link is
   reproducible, so this is sufficient to rebuild byte-identically.
2. **Rebuild the missing five objects from their recorded steps** and relink;
   then compare the result against `e213fa19` symbol by symbol. Any difference
   that is not in those five objects proves a further lost input.
3. **Gate the candidate on the Apex acceptance**: same content hash
   (`e68df05b`), exact token ids and probabilities, then the 32K/128 and 512
   decode numbers against 41.62 / 43.04 tok/s, with VRAM and RAM recorded.
4. **Only then** apply the prepared constructor patch
   (`fix-n0-block-alignment.patch`) and the native assert text, relink, and repeat
   the acceptance.

Steps 2 and 3 need server runs; they are benchmark runs and will be proposed to
the owner with duration and run count before starting.
