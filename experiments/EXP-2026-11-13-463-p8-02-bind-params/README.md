# EXP463: handler binding and path parameters in Goose (P8-02 tenth slice)

Registering a route and matching a request to it is a small but load-bearing
decision: the registered path is prefix + route, the first handler wins, and a
parameterised pattern captures segments. This experiment moves those decisions
into Goose.

## What was ported

`source/bind.goose`, from `vendor/cpp-httplib` (`PathParamsMatcher` constructor and
`match`) and `server-http.cpp` (`handlers.emplace`, `path_prefix + path`):

| decision | rule |
|---|---|
| pattern match | each static fragment must match at the current position; after the fragments that precede a parameter, the value runs to the next `/` or the end; the path must be fully consumed |
| empty capture | a parameter may capture an empty string (`/slots/` matches with an empty value) |
| duplicate names | a pattern that repeats a parameter name is a construction error |
| binding | the first registration of a path wins (`emplace` never overwrites) |
| registered path | `path_prefix + route` |

## Gate

`tests/bind_fixture.cpp` transcribes the httplib matcher with `std::string` and
compares match results and every captured value.

| corpus | cases |
|---|---|
| 10 patterns (literal, one/two/three parameters, leading and trailing parameter, duplicate, `/v1/...`) x 22 paths | 220 |
| duplicate-name detection and distinct names | 2 |
| first-wins binding | 2 |
| prefix + route registration | 6 |
| total | **220 cases, 470 values, 0 failures** |
| ASan + UBSan (runtime, module, fixture) | 0 failures, rc 0 |

Mutations 8/8 caught: exact-length fragment rejected (19 failures), fragment bytes
never equal (55), capture stops at a space (50), capture does not skip the
separator (20), fully consumed path not accepted (16), duplicate names accepted
(1), registration overwrites (1), registered path reversed (6).

## Divergence found and fixed in EXP449

The httplib matcher accepts `/slots/` and captures an empty `id_slot`. EXP449's
route table had an extra `p.len > 7` guard, so it answered 404 for that path while
native answers 400 later, when `std::stoi("")` throws. The guard was wrong and is
removed; `/slots/` is now a route in EXP449, whose gate and mutations were re-run
(426 cases, 6/6).

## Scope

P8-02 still needs the httplib transport itself and the streaming chunk provider.
The task queue and the handler bodies remain native.
