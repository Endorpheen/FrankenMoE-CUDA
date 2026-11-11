# EXP250: the slot's token budget in Goose (APEX.REQUEST/SESSION, first function)

`server_slot::n_remaining()` and `has_budget()` run on every decode step of the agreed request. Both are pure
integer logic and cross the bridge as scalars.

## Gate

    FG250S_GATE cases=16547 failures=0

Nine values of `n_predict_max` (-1, 0, 1, 2, 7, 128, 4096, 32768, 1000000) against every `n_gen` in range,
comparing both functions with a direct C transcription of the slot methods.

## Live server

The Goose versions are wired into `server_context.cpp` and the server is built from the same flint tree:

    first_sha = e68df05b4829865dcf8cd2424febfaded15b055f56fec36fefc002650ef8e7c9
    probs_equal = true, restore_ok = true

## Defect test (A -> B -> A)

Forcing `has_budget` to 0 inside the Goose module and rebuilding only the server changed the completion to
`d03502c43d74a30b936740a9517dc4ea2b2ad7168caa0a774cefe793ce0b33e7`. Restoring the module and rebuilding returned
`e68df05b`. So this code is on the live request path of a real server run, not merely linked in.
