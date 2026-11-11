# EXP251: request-field decisions in Goose (APEX.REQUEST/SESSION)

The JSON library stays native -- it is a library, not inference logic. What the bridge exposes is exactly the
fields this path reads (`n_predict`, `top_k`, `top_p`), and every decision that follows is Goose: the default when a
field is missing, the integer type check, the `top_k >= 0` clamp, and the bit-exact float carry.

## Gate

    FG251S_GATE cases=15 failures=0

Fifteen request shapes, including missing fields, explicit `-1`, `top_k = -5`, `top_k = 1000`, `top_p = 0.0/1.0/2.0`,
wrong JSON types (`"x"`, `1.5`), `null`, and a full three-field request. Compared field by field against a direct
transcription of the server's handling.

## Status

Module and gate are complete. Wiring it into `server_context.cpp` is the next step; the previous function
(`n_remaining`/`has_budget`, EXP250) is already live in a real server run with a defect test proving execution.

## Live server and defect test

Wired into `server_context.cpp` for the per-request `n_predict` decision (`fg251s_apply` bounds the value before it
becomes the slot limit). The server built from the flint tree completes the accepted request with `e68df05b`, equal
probabilities and a successful restore.

Defect test (A -> B -> A): forcing the Goose bound to return 1 instead of the request value changed the completion to
`d03502c43d74a30b936740a9517dc4ea2b2ad7168caa0a774cefe793ce0b33e7`; restoring returned `e68df05b`. So the request
decisions are on the live path, not merely linked.

Build note: `host-qualified.c` is C11 (`atomic_int`, `_Thread_local`) and cannot be compiled as C++, so it is a
separate object from the C++ JSON bridge, with `main` renamed and a `fg251ds_run` wrapper exported.
