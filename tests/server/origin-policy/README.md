# origin-policy

**Requirement:** P8-12 (server bodies removed only after the replacement is proven equal).
**Tier:** fast - no model, no GPU, seconds.
**Oracle:** the native `origin_is_localhost` from `vendor/flint/tools/server/server-http.cpp`,
transcribed three lines long in the fixture over the *same* parser production uses
(`common_http_parse_url`).
**Port:** `web_origin_is_localhost` of `goose/server/prerouting.goose`.

## What it caught

Written on 2026-11-19, and its first two results were both failures that had nothing to do with the
port being right:

1. The fixture declared `fg446_ld_u8` with one argument while the module's is
   `fn ld_u8(p: u64, o: u64) -> u32` (`prerouting.goose:18`). The offset was ignored, every comparison
   read byte 0, and the gate reported eight differences that were all its own.
2. With the signatures right, four differences remained - `localhost:8080`, `127.0.0.1:9101`,
   `ftp://localhost`, `://localhost` - where `common_http_parse_url` throws and the native function
   answers false, while the port answered true. The port now requires an http or https scheme, which
   is what the parser requires.
3. A fifth appeared with the harder cases: `http://[127.0.0.1]`, where the native parser yields
   `127.0.0.1` and answers true. The port now answers true there as well, rather than being quietly
   stricter than the code it replaces.

Result: **29 cases, 29 agree, 0 differ.**

## Why the oracle is the parser and not a copy of it

A transcription of the URL parser would only prove that two hand-written parsers agree with each
other. Including `common/http.h` means the gate compares the port against the exact function
production calls.

## Run

```
bash tests/server/origin-policy/run.sh
```
