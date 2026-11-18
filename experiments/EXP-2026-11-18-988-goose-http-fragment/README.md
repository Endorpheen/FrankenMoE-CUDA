# EXP-2026-11-18-988: a fragmented request was answered 400

Status: real bug fixed, demonstrated by running the same gate against the unit before
and after the change.

## The bug

`hl_serve_full` read once and, if the header terminator had not arrived, treated the
bytes it had as a whole request:

    var end = hl_req_end(req, filled);
    if end < 0 { end = filled; }        // <-- a fragment became a request

A client that sends `GET /hea` and then `lth HTTP/1.1` gets `400 Bad Request` for a
perfectly good request. `hl_serve_keep` already looped; the single-shot entry did not,
and nothing tested it.

## Evidence, before and after

The same gate compiled against the unit as it was at `HEAD` and against the fixed
unit:

    pre-fix unit :  FAIL a request in two fragments is not answered with 400
                    BAD checks=111 failures=1
    fixed unit   :  PASS checks=111 failures=0

## What was done

`hl_serve_full` now reads until `hl_req_end` finds `CRLF CRLF`, stopping only on EOF,
a read error or a full buffer, and answers `-1` if the terminator never arrives.
That is the same loop `hl_serve_keep` already used.

## A test that proved nothing, and how it was caught

The first version of the test split the request after the request line -
`"GET /health HTTP/1.1\r\n"` then `"\r\n"`. It **passed against the pre-fix unit**,
because a fragment that already contains the whole request line parses as a complete
request: the bug needs a split *inside* the line. The test now sends `GET /hea` then
`lth HTTP/1.1\r\n\r\n`.

I only noticed because I ran the gate against the old unit to check the claim instead
of asserting it. A test that passes on the broken build is worse than no test, and it
would have been recorded as proof of a fix that was never needed for that path.

## Result

| check | result |
| --- | --- |
| gate | **111 checks, 0 failures** |
| whole suite | **38 182 checks, 0 failures** (8 gates) |
| pre-fix unit | fails the new case, as it should |

## Not claimed

There is still no TLS and no body reading. A request larger than the caller's buffer
is refused rather than continued, which is a bound rather than a behaviour.

No model was started.

## Reproduction

    bash tools/verify-unit-gates.sh
    # pre-fix comparison:
    git show HEAD~1:goose/server/http-listen.goose > old.goose
    # compose with old.goose instead, rebuild, run the same gate
