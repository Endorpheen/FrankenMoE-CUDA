# 29 server doors exist and are in no shipped library

Measured 2026-11-19 against `libllama.so.0` of the release `v0.10.0-exp1476`, at symbol level rather
than by text search.

## The fact

| module | doors | in the shipped library |
|---|---|---|
| `bind-params` | 4 | 0 |
| `multipart` | 3 | 0 |
| `request-errors` | 3 | 0 |
| `required-fields` | 1 | 0 |
| `response` | 5 | 0 |
| `sse-framing` | 2 | 0 |
| `startup-policy` | 1 | 0 |
| `stream-manager` | 4 | 0 |
| `stream-policy` | 2 | 0 |
| `stream-provider` | 4 | 0 |
| **total** | **29** | **0** |

30 of the 59 files in `goose/server` are not in the module list. Ten of them export doors; the other
twenty export nothing and are fragments consumed some other way. Not one of the 29 exported doors is a
symbol in the artefact users run.

## Why the check is at symbol level

A text search cannot answer this question here: the door names are ordinary words - `missing`, `frame`,
`decide`, `step`, `duplicate`, `append` - so `grep` finds them in unrelated code (159 hits for `append`,
104 for `missing`) and says nothing about call sites. That is the same trap that produced six wrong
"no callers" records earlier in this migration, and it has one reliable answer: if the symbol is not in
the library, nothing in that library can call it.

## What it means for P8-09 and P8-12

`prerouting` was one of these and is now in the unit with a gate and a call site behind
`FG_ORIGIN_GOOSE`, which is the pattern the rest can follow. Each of the ten modules needs one of two
decisions, and neither is a decision by assertion:

1. **wire it** - add it to the unit, write the differential gate that holds it to the native
   behaviour, then call it with the native branch kept as the fallback; `sse-framing`, `multipart` and
   `request-errors` are the strongest candidates because their contracts are narrow and testable;
2. **drop it** - record in the manifest that the module was superseded or duplicated, and remove it
   from the tree, as `D3-JINJA-SUPERSEDED-PROOF.md` does for the eight Jinja slices.

Until one of those happens, the honest count for these modules is zero, and no report may claim the
server's SSE framing, multipart parsing or request validation is on Goose.

## Re-measure

```
python3 - <<'PY'
import re, subprocess, pathlib
lib = 'builds/checkpoint-20261119-EXP1476/libllama.so.0'
names = {l.split()[-1] for l in subprocess.run(['nm','-D','--defined-only',lib],
         capture_output=True, text=True).stdout.splitlines() if l.strip()}
for p in sorted(pathlib.Path('goose/server').glob('*.goose')):
    d = re.findall(r'(?m)^export fn ([a-z0-9_]+)', p.read_text())
    if d:
        print(f'{p.name:28} {len(d):2} in_lib={sum(x in names for x in d)}')
PY
```
