# Releases

Releases are the tested snapshots of this repository. A release is created only
from `main` after an accepted milestone, never from a working branch, and it is
announced with an annotated tag and GitHub Release notes.

## Version scheme

`vMAJOR.MINOR.PATCH`, starting at `v0.1.0`:

- **MAJOR** changes when the runtime contract changes incompatibly (ABI, model
  format, server API).
- **MINOR** changes when an accepted batch of Goose slices lands or a new model
  path becomes usable (for example a P7-08 closure or Edge0 support).
- **PATCH** changes for documentation, tooling and fixes that do not alter the
  runtime contract.

The version describes the repository state, not a promise of production
readiness. Pre-releases use `-rc.N` and are marked as pre-release on GitHub.

## Release criteria

All of these must hold before a tag is created:

1. `git status` is clean and `HEAD` equals `origin/main`.
2. `python3 tools/check-goose-tree.py` is green.
3. The ledger and `docs/goose-migration/STATUS.md` describe the same state.
4. The build named in the release notes was actually measured; numbers carry
   their configuration and hashes.
5. No file above 100 MB is in the tag tree (GitHub rejects such pushes).

Optional, and required for a runtime release:

6. The runtime bundle starts on the reference host, and the notes record the
   binary hash, the library manifest and the Apex measurement.

## Assets

GitHub allows 2 GiB per asset. Two kinds are published:

| asset | when | content |
|---|---|---|
| source archives | always (GitHub generates them) | the tagged tree |
| `goose-runtime-<version>-linux-x86_64.tar.zst` | runtime release | the run set: `llama-server`, the libraries it selects, `MANIFEST.sha256`, `LAUNCH.txt` |

The runtime bundle is a compressed copy of the isolated run set that was
verified with `ldd`, `strace` and a real request. It is never the working
`runtime/` directory. Stage it under `releases/` (ignored by Git), record
SHA256 next to it, and pass it with `--asset`.

## Creating a release

```
# dry run first: checks, tag name, notes, assets; changes nothing
scripts/make-release.sh --version v0.1.0 --notes-file /tmp/notes.md

# real run after Igor approves
scripts/make-release.sh --version v0.1.0 --notes-file /tmp/notes.md --yes
```

The script refuses to run when the tree is dirty, `HEAD` is not `origin/main`,
the ledger check fails, or the tag already exists. It creates an annotated tag,
pushes it, publishes the GitHub Release with `gh`, uploads the assets and prints
their SHA256.

## Notes format

`docs/goose-migration/RELEASE-NOTES-TEMPLATE.md` is the skeleton: what changed,
which slices closed, the measured numbers with their configuration, what is
still open, and the verification commands that were run.
