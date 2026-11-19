# EXP-2026-11-18-989: Host validation and the URI limit

Status: two RFC 9112 rules from cpp-httplib implemented, 38 230 checks, 0 failures.

## The rules, read out of the library

`httplib.cpp:10246-10262`, not guessed:

    host_count > 1 || (host_count == 0 && version == "HTTP/1.1") ||
    !fields::is_host(get_header_value("Host"))
        -> 400, connection closed

    target.size() > CPPHTTPLIB_REQUEST_URI_MAX_LENGTH     (8192)
        -> 414, connection closed

`is_host` (httplib.cpp:5750-5757) allows alnum plus `- . _ ~ ! $ & ' ( ) * + , ; = % : [ ]`.

## What was done

`goose/server/http-request.goose` gains `rq_is_host_char`, `rq_host_violation` and
`rq_uri_max`. `goose/server/http-listen.goose` counts Host field lines during the
existing header scan (first value kept, as `get_header_value` does), decides the fault
before routing, and answers 400 or 414 with `Connection: close`.

## Three things the compiler and the tests taught

1. **The version code is 2, not 1.** `rq_parse_line` writes 2 for HTTP/1.1 and 1 for
   HTTP/1.0. Comparing against 1 made 1.1 requests exempt and 1.0 requests refused -
   exactly inverted, and the gate showed both at once.
2. **A slice-typed export resolves only if the module itself calls it with a concrete
   argument.** `rq_is_host` was resolved fine as a plain private helper but not as an
   export whose only caller lives in another module, and a wrapper taking the slice
   failed the same way. The decision is now scalar
   (`count, version, host_ok, have_host`) and the character rule is exported one byte
   at a time; the caller loops. The rule stays owned by one module.
3. **An untyped literal in an argument position** (`if cond { 1 } else { 0 }`) leaves
   an export without one concrete signature. Typed into an i64 first.

## The native quirk, reproduced

`is_host` is `all_of` over the bytes, and `all_of` on an empty range is true, so an
**empty Host value is accepted**. That is what the library does; the port does the
same and the gate says so, rather than quietly "fixing" behaviour that clients may
depend on.

## The tests were unrealistic, and the change exposed it

Every existing HTTP/1.1 request in the gate lacked a `Host` header - 24 request
strings. Under the new rule they all become 400, which is correct: a real 1.1 client
always sends one. They now carry `Host: x`, and the suite passes unchanged otherwise.

The 414 case needed a bigger request buffer: with 8192 bytes a target over 8192 cannot
arrive, so the limit was unreachable and the check would have been theatre. The test
helpers now use 16384, which makes the check real.

## Result

| gate | checks | result |
| --- | --- | --- |
| http-request (12 new direct decision checks) | 578 | PASS |
| http-listen (6 new cases) | 147 | PASS |
| whole suite | **38 230** | **0 failures** |

Cases: 1.1 without Host - 400 and closed; two Host fields - 400, not last-wins;
`Host: a b` - 400 while `a-b.c:8080` - 200; 1.0 without Host - 200; empty Host - 200
(the quirk); an 8200-byte target - 414.

## Not claimed

Transfer-Encoding handling (RFC 9112 §6.1) is still absent - the library closes the
connection after such a request and this listener neither reads nor drains a body, so
the rule is not meaningful yet. No TLS. No model was started.

## Reproduction

    bash tools/verify-unit-gates.sh
    sed -n '/^gate/,/^result/p' <log>
