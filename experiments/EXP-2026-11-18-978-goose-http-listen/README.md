# EXP-2026-11-18-978: an HTTP listener in Goose, over a real socket

Status: the standalone serves a request. This closes the "no HTTP transport" line.

## Hypothesis fixed before changes

Compiler blocker 6 says Goose cannot call `open`, `mmap`, `socket` because the
generated unit includes libc headers that declare those names with a different
prototype. That is true for the shipping DSO, where carrier sources include the
headers. The standalone unit is compiled **alone**, and the probe in EXP958 showed
`extern "socket"` compiles there. So the blocker's scope was narrower than the note
implied, and the transport was not actually blocked.

Measured per symbol, with and without a real call:

    socket bind listen accept getsockname shutdown recv send   -> compile clean
    read write close                                           -> conflicting types

`read`, `write` and `close` collide with `<unistd.h>`, which the generated unit does
include. Those three therefore go through substrate primitives.

## What was done

- three primitives in `integration/m1/host/goose_host.c`: `qz_rd`, `qz_wr`, `qz_cl`.
  They move bytes and decide nothing, the same category as `qz_st_u8` and
  `qz_ld_u8` - not a bridge between Goose and a policy.
- `goose/server/http-listen.goose`: `hl_open` builds a `sockaddr_in` in the host
  scratch (`qz_tmp`), binds port 0, listens, and reports the ephemeral port through
  `getsockname`; `hl_serve` accepts one connection, reads it, parses the request line
  with `rq_parse_line`, answers with `rs_status_line`/`rs_header`/`rs_end_headers`,
  and closes both sockets.
- the canonical unit gains the module: 142 modules, 4281 specializations.

## Result

| check | result |
| --- | --- |
| gate | **15 checks, 0 failures** |
| well-formed request | `HTTP/1.1 200 OK`, `Content-Length: 2`, body `ok` |
| malformed request line | `HTTP/1.1 400 Bad Request`, `Content-Length: 0` |
| wire vs buffer | the bytes the client received equal the Goose response buffer |
| port | **ephemeral**, chosen by the kernel and reported by `getsockname` |
| artifact | `hl_open`, `hl_serve`, `hl_close`, `rq_parse_line` in `frankenmoe-v4` (5 614 088 B) |

The client runs in a forked child so the parent can block in `accept()`, and it
echoes what it receives into a pipe - so the comparison is against the socket, not
against the buffer alone.

Ports are deliberately ephemeral: rule 21 reserves 9101 for runtime benchmarks, and
a test that hard-codes a port races whatever else is listening.

## A build trap found here, and fixed

The first rebuilt artifact contained `rs_status_line` but **not** `hl_open`.
`build-standalone-runtime.sh` rebuilt `build/goose-main/main.c` only when the file
was *missing*, so after adding a module it reused the previous unit and linked a
stale binary. A full build was lost to it before `nm` showed the absence.

The guard now rebuilds when `build-main-runtime.sh` or any `goose/**/*.goose` is
newer than `main.c`, and prints `unit: stale or missing, rebuilding`. The rebuilt
artifact then contained all four symbols.

This is the third instance of one pattern in this series - EXP974's silent `set -e`
exit, EXP970's ABI mismatch, and now a cached composition step. Each time, the build
succeeded and the artifact was wrong.

## Not claimed

One request per connection, no TLS, no keep-alive, no body, no routing: an unknown
path still gets 200 with `ok`. The listener proves the transport seam, not a server.
CUDA and full 41-layer execution remain absent.

The shipping `llama-server` still uses httplib; nothing in the DSO calls these
exports. Blocker 6 still applies there, which is why the carrier has a reader
adapter.

No model was started.

## Reproduction

    FG_SUBSTRATE=/tmp/opencode/sub-668/flint-cpu/build-off/ggml/src \
        bash tools/build-standalone-runtime.sh
    nm builds/goose-standalone/frankenmoe-v4 | grep -E ' T hl_'
    bash tools/migration-status.sh | grep -A2 'HTTP server (transport)'
    # the gate: build tests/gate.cpp against the composed unit and run it
