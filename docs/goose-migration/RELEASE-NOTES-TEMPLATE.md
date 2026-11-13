# <version> — <short title>

Released: <date>. Commit: <full hash>.

## What changed

<one paragraph: the accepted work in this release>

## Closed requirements and slices

| requirement | slices | evidence |
|---|---|---:|
| <id> | <n/m> | <gate numbers, commit> |

## Measured performance

Configuration: <model, build, flags, hardware>. Every number carries its source
file and content hash.

| metric | value | source |
|---|---:|---|
| prefill | <tok/s> | <file> |
| decode | <tok/s> | <file> |

## Still open

<the honest remainder: partial requirements, unimplemented model paths>

## Verification

Commands run before publishing:

```
python3 tools/check-goose-tree.py
<gate commands and their results>
```

## Assets

| asset | SHA256 |
|---|---|
| `goose-runtime-<version>-linux-x86_64.tar.zst` | <hash> |
