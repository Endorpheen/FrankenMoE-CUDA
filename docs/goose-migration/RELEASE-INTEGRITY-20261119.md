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
