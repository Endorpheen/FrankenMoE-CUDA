# EXP379: a dropped streaming client does not disturb the cache

## Question

P5-09 asks what happens to the memory when a request is cancelled. On the reconstructed tree, does an
aborted stream leave the cache in a state that changes the next answer?

## Method

`run_cancel.py` starts the server on the fixed profile, posts a **streaming** copy of the reference
request, reads eight stream events, then drops the socket. After a three-second pause the reference
request is posted again and its completion hash is compared with the reference.

## Result

| check | result |
|---|---|
| stream reads before the drop | 8 |
| server saw | `cancel task` |
| reply to the next request | arrived, 32 768 prompt tokens |
| completion | `e68df05b…` — **identical to the reference** |
| live witnesses in the same run | 37 |
| exit | 0 |

So cancelling a request halfway does not corrupt the KV state or the sequence bookkeeping on this
tree. The next request is served normally and answers exactly as the reference does.

## Scope

This closes the "cancel leaves the cache usable" half of P5-09. It does not cover **model unload**
during an active request, which needs a second scenario, and it says nothing about a cancel that
arrives while a batch is being applied rather than while it is being streamed.
