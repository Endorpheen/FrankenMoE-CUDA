# EXP-2026-11-19-1395: the encoder proven at acceptance scale, on a text prompt

## Why a second request shape

The acceptance request carries **token ids** (`{"prompt": [5439, 425, ...]}`), so the tokenizer never runs on that path
and its hash is not evidence about one. `make_request.py` builds the same size of request with **text**: about
32 000 tokens (128 000 characters), 512 generated, temperature 0, fixed seed.

## The two runs

`tools/apex-test.sh` with `APEX_REQUEST` pointing at that file, once with the native encoder and once with the ported one
serving:

| run | decode tok/s (3 requests) | prefill | answer sha256 |
|---|---|---|---|
| native encoder | 63.68, 64.15, 64.20 | 2673 / 49.8 / 52.1 | `2d38a6fd6a03` |
| ported encoder serving | 63.77, 64.42, 64.32 | 2681 / 52.6 / 54.2 | `2d38a6fd6a03` |

The answer hash is **identical**, on a prompt the tokenizer actually processes, at acceptance scale. Decode is unchanged
within the spread, and prefill's low second and third figures are the cached prefix of the previous request, not a
regression - both runs show the same shape.

## What this closes

The gap this card's predecessor exposed: there was no acceptance-level evidence for the encoder because the acceptance
request cannot produce any. There is now, and it uses the same `tools/apex-test.sh` with one variable changed, so the
profile, the pre-flight, the process isolation and the SHA recording all still apply.

## What it adds to the record

A second reference for future tokenizer work: text prompt, 32 000 tokens, temperature 0, seed 11, answer hash
`2d38a6fd6a03`, decode 63.7-64.4. Any change to the encode path must reproduce it. It is not a new baseline for the
runtime - the runtime baseline stays EXP1298 at 62.73 with its own profile - it is the reference that makes tokenizer
changes falsifiable.

## Reproduce

```
python3 experiments/EXP-2026-11-19-1395-text-prompt-acceptance/make_request.py /tmp/req_text.json
APEX_REQUEST=/tmp/req_text.json FG_TK_BPE_DIAG=1 FG_TK_BPE_SERVE=1 \
  APEX_SERVER_DIR=$PWD/builds/flint-acc/bin bash tools/apex-test.sh text-served 512 4096 4096 4992
```
