# EXP456: GCP AIP compatibility and /props in Goose (P8-02 ninth slice)

Two small API-surface layers are still decided in C++: the Vertex AI (AIP)
compatibility switch and the `/props` payload shape. This experiment moves both.

## GCP AIP decisions

`source/gcp.goose`, from `server-http.cpp` (`gcp_params`, `path_to_gcp_format`,
`server_http_context::init`, `register_gcp_compat`):

| decision | rule |
|---|---|
| enabled | the value is exactly `PREDICTION`; anything else, including a longer or differently cased string, disables compat |
| route | empty or missing value falls back to the default; a non-empty value gets a leading slash when it lacks one |
| port | compat enabled replaces the server port with the AIP port and reports whether that changed anything |
| alias | drop a leading `/v1`, drop one more leading slash, stop at `:`, upper-case the character after `/`, `-` or `_` |
| predict body | must be an object with an `instances` array; the array is capped at 128 |
| instance | must be an object with a string `@requestFormat` |

`getenv`, `std::stoi` and the JSON library stay native; a non-numeric
`AIP_HTTP_PORT` still throws in the native constructor and is not re-implemented.

## /props decisions

`source/props.goose`, from `server-context.cpp` (`get_res_props`, `get_props`,
`post_props`):

- the ordered key list of 19 keys, plus `connect_code` when the tunnel has a
  code, for 20;
- the source of `GET /props`: the cached snapshot while sleeping, the live
  structure otherwise;
- `POST /props` is `not_supported` (501) when the endpoint is disabled.

## Gate

`tests/gcp_props_fixture.cpp` transcribes the native rules (string comparisons,
`path_to_gcp_format`, the port override, the predict checks and the key list) and
compares them with the two Goose modules.

| corpus | cases |
|---|---|
| AIP_MODE strings (exact, wrong case, trailing space, prefix, suffix, empty) x 2 arms | 8 |
| route value x default x has-value | 36 |
| port pairs x enabled | 18 |
| alias paths (route-table shapes, `:param`, dashes, underscores, digits, camelCase input) | 19 |
| predict body shape and instance shape matrices | 22 |
| props key order/count, mode and POST decision | 8 |
| total | **111 cases, 170 values, 0 failures** |
| ASan + UBSan (both modules and the fixture) | 0 failures, rc 0 |

Mutations 10/10 caught: AIP_MODE length check becomes a minimum (3 failures),
route leading-slash test inverted (29), port override flag inverted (9), alias
keeps the `/v1` prefix (12), alias stops upper-casing (10), predict limit off by
one (1), instance format check inverted (2), `connect_code` never emitted (2),
props mode inverted (2), POST /props decision inverted (2).

## What stays native

- `getenv`, port parsing and the conflict `exit(1)` when a custom predict route
  collides with an existing handler;
- request execution and response translation for the predict route
  (`parse_gcp_predict_response`), including the per-instance fan-out;
- every value inside the `/props` payload.

P8-02 stays IN_PROGRESS on the httplib transport, handler binding/registration
and the predict fan-out.

## Reproduce

    bash experiments/EXP-2026-11-13-456-p8-02-gcp-props/build_fixture.sh
    python3 experiments/EXP-2026-11-13-456-p8-02-gcp-props/mutate.py
