# EXP-2026-11-19-1410: a client that sends a request and reads nothing - the third open edge of P7-07

## The case

EXP-1399 left three gaps: concurrent slots, cancellation during prefill, and a client that disconnects without reading
the body. The third is testable without touching the profile: connect, send a streaming request, close the socket.

```
client closed without reading
after abandon: http=200 content='**ok**'
server alive
```

The server does not wedge: the next request is served normally and the process survives. Whatever the abandoned stream
held - its slot, the detokeniser's partial state - is released.

## What remains open

- **concurrent slots**: the accepted profile runs `-np 1`, so testing `-np 2` is an exploration, not a profile run, and
  the card does not present it as one;
- **cancellation during prefill**: needs a prompt long enough that the cancel lands before decoding starts. That is
  testable in the profile and is the next of the three.

## Reproduce

```
bash experiments/EXP-2026-11-19-1410-client-abandons/run.sh
```
