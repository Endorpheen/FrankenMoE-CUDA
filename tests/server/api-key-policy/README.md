# api-key-policy

**Requirement:** P8-09 - the API-key middleware served by Goose.
**Produced by:** the P8-09 web state note of 2026-11-19.
**Tier:** fast - one C file, no model, no GPU. **Skips** when the library does not export the door.

## What it asks

Seven questions that reproduce `middleware_validate_api_key` in `server-http.cpp`:

| case | expected |
|---|---|
| `Bearer <configured key>` | authorised |
| the raw configured key | authorised |
| a wrong key | refused |
| no credential at all | refused |
| the key in `X-Api-Key` instead of `Authorization` | authorised |
| a public path | authorised |
| `Bearer ` with nothing after it | refused |

## Why it skips on the accepted set

`nm -D builds/flint-acc/bin/libllama.so.0` has no `key_*` export at all: the api-key module is in the
current unit list but was not in the unit when that library was built. So the accepted release's
API-key check is entirely native, and the wiring added to `server-http.cpp` falls back to the native
branch on that library - which is the behaviour it was designed for.

This is the same source-ahead-of-binary situation recorded for the model-manager doors in P8-06,
found a second time. The test will start answering as soon as a library built from the current unit
is passed to it.

## The wiring

`server-http.cpp` consults the door under `FG_API_KEY_GOOSE=1`, falling back to the native check when
the port declines or the door is absent. Opt-in while unproven, and the fallback is the safe
direction: a declined port can never open a door the native check would close.

## Run

```
bash tests/server/api-key-policy/run.sh            # builds/flint-acc/bin -> skip today
bash tests/server/api-key-policy/run.sh <lib-dir>  # a library that exports key_authorize
```
