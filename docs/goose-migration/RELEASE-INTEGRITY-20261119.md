# The published release is the file this repository records

Checked 2026-11-19 against GitHub, because "the release is published" and "the published bytes are the
recorded bytes" are different claims and only the second one protects the accepted baseline.

| check | result |
|---|---|
| `gh release view v0.10.0-exp1476` | draft **false**, prerelease **false**, published 18:30 |
| asset size | `frankenmoe-apex-exp1476-linux-x86_64.tar.gz` = **121 544 166** bytes, equal to the local archive |
| published checksum file | downloaded from the release, sha256 `23857b88…` |
| local checksum file | sha256 `23857b88…` |
| `diff` of the two checksum files | **identical** |

So the asset a user downloads hashes to the value recorded here, and the local archive hashes to the same
value - the three-way agreement (published checksum file, local checksum file, local archive) is what makes
the number meaningful.

## What this does not establish

It does not establish that the archive can be **rebuilt**: that is the separate and currently negative
finding - the release came from neither build path in the tree
(`REPRODUCTION-VERDICT-20261119.md`). A release can be byte-identical to its record and still
irreproducible; today it is both.

## Command

```
gh release view v0.10.0-exp1476
gh release download v0.10.0-exp1476 -p "*.sha256" -D /tmp/expchk
diff /tmp/expchk/*.sha256 /home/end0/AI/LlamaServer/frankenmoe-apex-exp1476-linux-x86_64.tar.gz.sha256
```

## The shipped binary runs, in a directory that is not its build tree

Copied `llama-server` and every `.so` from the release into `/tmp/reltest` - a directory with no relation
to any build tree - and started it with the instruction the manifest gives:

```
LD_LIBRARY_PATH=/tmp/reltest ./llama-server --version
version: 0.7.0-dev (build 2609, commit b4a43343)
built with GNU 15.2.0 for Linux x86_64
FG270Q_SERVER_QUEUE pick=0 cleanup=0
FG276T_STOP_STRINGS full=0 hits=0
FG274B_BATCH_COMPAT calls=0 yes=0
FG272S_SLOT_SELECT similar=0 lru=0
exit 0
```

Three facts from that output, each of which had only been assumed before:

1. the binary loads every library it needs **from its own directory** when the documented
   `LD_LIBRARY_PATH` is set - the RUNPATH into build trees never has to be used;
2. it reports `GNU 15.2.0`, which matches the compiler stamp found in the libraries and confirms the
   release was built with the system compiler rather than the pinned gcc-13;
3. the Goose components print their **witness lines on start** - `FG270Q_SERVER_QUEUE`,
   `FG276T_STOP_STRINGS`, `FG274B_BATCH_COMPAT`, `FG272S_SLOT_SELECT` - so those modules are not only
   linked but initialised in the shipped binary.

## What those counters do and do not prove

They are all zero, because no request was served. They prove initialisation, not use: a count that stays
at zero during a real request would mean the decision never ran. Using them as evidence of behaviour is
exactly the mistake rule 33 names, so this document claims only what the output shows.
