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
