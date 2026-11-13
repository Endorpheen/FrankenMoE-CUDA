# EXP360: batch shapes and concurrency on the matched candidate

Run after the out-ids fix (EXP359) to confirm the batch ports behave across shapes rather than only on one.

## Sequential shapes

| flags | result |
|---|---|
| `-b 32 -ub 16 -np 1` | identical to native |
| `-b 64 -ub 64 -np 2` | identical |
| `-b 256 -ub 256 -np 4` | identical |

## Concurrency

Four requests released at once against `-np 4`, matched by request index rather than by arrival order:

    req0 SAME   req1 SAME   req2 SAME   req3 SAME

All four continuations identical to native.

This is the shape that broke before the fix in EXP359 -- two concurrent slots tripped the out-ids assert -- so the sweep is a regression test for that
defect as much as a new check. It would have failed on the previous build.

## Scope

Everything here runs a 24- or 12-token generation on a 260K model with a 1024-token context. It exercises batch splitting, classification, sequence
sets, compatibility positions and out-ids across four shapes; it says nothing about 32K prompts, expert routing or memory behaviour, which need Apex.
