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
