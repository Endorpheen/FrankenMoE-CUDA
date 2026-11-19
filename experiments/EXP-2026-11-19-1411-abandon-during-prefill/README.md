# EXP-2026-11-19-1411: a request abandoned during prefill

## The case EXP-1410 did not cover

That card closed the socket before the server had read anything. This one sends a 120 000-character prompt - about 32 000
tokens, several seconds of prefill - and closes **0.8 seconds later**, while the prefill is running:

```
client closed 0.8 s after sending a 32K-token prompt
after prefill abandon: http=200 content='**ok**'
server alive
```

The server finishes or drops the work and keeps serving: the next request is answered 200 and the process survives. A
prefill interrupted mid-way does not leave the slot wedged or the process damaged.

## What is left of the three gaps

| gap | state |
|---|---|
| client never reads the body | measured, EXP-1410 |
| cancel during prefill | measured, here |
| concurrent slots | **not measured** - needs `-np 2`, which is outside the accepted profile and therefore an exploration |

## Reproduce

```
bash experiments/EXP-2026-11-19-1411-abandon-during-prefill/run.sh
```
