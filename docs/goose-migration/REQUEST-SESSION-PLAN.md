# APEX.REQUEST / APEX.SESSION: scope and first target

`tools/server` is 26,529 lines across `server-common`, `server-chat`, `server-context`, `server-slot`,
`server-task` and the HTTP layer. Porting that as one piece is not meaningful; the item is addressed the way every
other item here was: find what the agreed Apex request actually executes, port that, gate it, prove it on a run.

## What the agreement requests

One `/completion` call with `n_predict`, sampling parameters and slot save/restore - the same request the accepted
profile already uses. So the scope is the path that request takes: parse, validate, schedule onto the single slot,
assemble the response, save/restore the slot state.

## First target: request parsing and validation

Pure host-side data handling with no sockets, so it fits the bridge: the fields read from the request JSON, the
range checks, and the derived values that end up in `common_params`. This is where a wrong port changes behaviour
silently (temperature bounds, `n_predict` clamping, `cache_prompt` handling), so it deserves the same gate as a
kernel.

## Method (same as everywhere else in this plan)

1. Harness: feed identical JSON to the native parser and to the Goose module, compare the resulting parameter
   structs field by field, including rejected inputs and their error codes.
2. Mutations: each validation bound and each derived field must be caught by the harness.
3. Defect test: one arithmetic change in the Goose module must change what the server does with a live request.
4. Run: `/completion` on the accepted profile must still yield `e68df05b` with equal probabilities.

The slot save/restore half of the item is already covered by APEX.KV evidence (exact restore), so what remains here
is the request path itself.
