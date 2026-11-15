# P10-11 — licensing, attribution and source reproducibility

Status: **DONE** (audit, 2026-11-15).

## What is preserved

| item | evidence |
|---|---|
| repository licence | `LICENSE` is Apache-2.0 |
| inherited runtime licence | `runtime/LICENSE.BigMoeOnEdge` is Apache-2.0 |
| upstream llama.cpp licences | `upstream/llama.cpp/LICENSE`, `upstream/llama.cpp/licenses/` (jsonhpp, vendored sha256/xxhash/rotate-bits, cpp-httplib, UI vendors) |
| upstream BigMoeOnEdge licences | `upstream/BigMoeOnEdge/LICENSE` and its `third_party/llama.cpp` licence tree |
| attribution statement | `README.md`, section "Attribution and license" |
| pinned upstream revisions | `UPSTREAMS.md` (llama.cpp, BigMoeOnEdge, expert-tier, submodule, related PRs) |
| per-file provenance | `goose/MANIFEST.json` maps every canonical file to its experiment, native path, gate and SHA-256; each `.goose` header cites the native source |
| experiment provenance | every `experiments/EXP-*` card carries the native path, gate numbers, mutations and input SHA-256 |

## Reproducibility

- `tools/sync-goose-tree.py` rebuilds `goose/` from the accepted experiments and
  rewrites the manifest.
- `tools/check-goose-tree.py` verifies all 4238 entries against their expected
  hashes; it is green.
- `tools/build-main-runtime.sh --repro` builds the composed unit twice and fails
  on any difference; it reports `repro: ok`.
- `tools/package-main-runtime.sh` writes `dist/` with `SHA256SUMS` and
  `manifest.json`, so a released unit is verifiable without the build tree.
- Experiments record input hashes (native source, generated C, binaries) so a
  result can be re-derived.

## Not changed

No licence text, upstream notice or attribution was modified, moved or removed
during the migration. The Goose sources are new files; they do not relicense the
upstream code they describe.

## Optional improvement, not a blocker

A root `NOTICE`/`THIRD_PARTY` summary generated from `UPSTREAMS.md` would make the
third-party inventory visible without walking `upstream/`. It is documentation,
not a licensing gap: the licence texts themselves are already present and
unmodified.
