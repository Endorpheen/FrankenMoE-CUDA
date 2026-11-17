# EXP-2026-11-17-938: Goose can open a listening socket

Status: probe PASSED. This closes the transport question that has stood since the
MODEL ownership blocker was recorded in `docs/goose-migration/CODEX-HANDOFF.md`:
the standalone runtime has server decisions but no listener.

## Hypothesis

The blocker recorded in the handoff is that Goose cannot call libc: the generated
translation unit includes system headers that already declare `open`, `close`
with different prototypes, so an `extern` with the Goose signature fails to
compile. If the blocker is about **specific names** rather than about calling
into libc at all, then socket calls — which no included header declares — should
bind directly, and only `read`/`write`/`close` need a rename.

## Probe

`socket`, `bind`, `listen` are declared in Goose with their own C names and
compile with no conflict. `close`, `read`, `write` conflict exactly as recorded,
so they are bound through three one-line libc forwards with distinct names
(`gx_close`, `gx_read`, `gx_write`); the forwards contain no decision, only the
prototype.

Result on 127.0.0.1, kernel-chosen port:

    LISTEN_rc=0

`socket -> bind -> listen -> close` executes from Goose.

## What this changes

The "no HTTP transport" remainder of P11-01 is not a compiler wall. The
listener can be written in Goose now, with the three tiny forwards as recorded
adapter debt (translation only, no policy). The `open`/`mmap` ownership blocker
is narrower than stated: it is about names `unistd.h`/`fcntl.h` already declare,
not about reaching the kernel.

## Reuse

    goose --standalone -o r.c --header r.h source/listen.goose
    gcc-13 -O1 -o listen tests/main.c r.c source/libc-wrappers.c -lm
    ./listen          # prints LISTEN_rc=0

## The listener itself

`goose/server/http-listener.goose` now carries the transport decisions:

- `ht_open` — socket, SO_REUSEADDR, bind, listen, with a distinct return code
  per failing step so a gate can name the step;
- `ht_accept` — one connection, with the peer address buffers owned by the host
  (a Goose slice is passed by value, so a local array cannot stand in for the
  C pointer);
- `ht_read_request` — read until CRLFCRLF, bounded by the header limit, then the
  method code, the path span (query excluded) and the header end;
- `ht_keep_alive`, `ht_contains_ci` — case-insensitive header search and the
  keep-alive rule;
- `ht_response_head`, `ht_write_all` — status line, Content-Length, content
  type, connection header, and a short-write loop.

Real request through it:

    HTTP/1.1 200 OK
    Content-Type: application/json
    Content-Length: 11
    Connection: keep-alive

    {"ok":true}

Server-side log of the same request: `ACCEPT conn=4`, `REQ n=60 method=1
hend=60 path_off=4 path_len=6`.

Gate: `bash tests/run.sh [outdir]` prints `HTTP_LISTENER OK`.

### Two defects the probe exposed

- reading before `accept` gives `ENOTCONN` (107) on the listening socket; the
  first version did exactly that and looked like a broken read;
- `accept` cannot take a Goose slice for `sockaddr`/`socklen_t`: slices are
  passed by value, so the host supplies those buffers and the forward passes
  their pointers.

## Router behind the listener

`integration/goose-standalone/tests/http_server.c` closes the loop: accept, read,
take the path span, ask `route_id`, answer with the status the route implies and
a body per route category, honouring keep-alive.

    /health  -> HTTP/1.1 200 OK      route=1
    /props   -> HTTP/1.1 200 OK      route=3
    /metrics -> HTTP/1.1 200 OK      route=2
    /nope    -> HTTP/1.1 404 Not Found  route=0

Gate: `bash tests/server-route.sh [outdir]` prints `HTTP_ROUTES OK`.

`route_id` signals "no route" with **0**, not -1; the first version treated 0 as
a hit and answered 200 to an unknown path. The host now maps `route > 0` to 200.
