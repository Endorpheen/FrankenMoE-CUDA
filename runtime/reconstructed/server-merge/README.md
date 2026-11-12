# Merged server sources

The four server-side ports were four separate copies of `server-context.cpp`, each with one bridge (see
`docs/goose-migration/MERGE-MAP.md`). This directory holds the single file that contains all of them, plus the queue patch.

## How it was merged

Upstream `tools/server/server-context.cpp` from the flint tree is the common ancestor, and each fork differs from it by only 29 to 55 lines.
`git merge-file` was applied fork by fork, starting from the upstream file.

Two conflicts appeared, both of the same kind: two ports added a declaration block at the same anchor. Both were resolved by keeping **both**
blocks, since neither contradicts the other -- the checkpoint block and the slot-selection block at the second anchor, the batch-compat and
stop-string blocks at the first.

## Verification

    prefixes present:  fg268p fg272s fg274b fg276t   (29 references)
    diff vs upstream:  158 changed lines
    syntax check:      rc=0 using the real compile command from build-cuda/compile_commands.json

`server-queue.cpp` needs no merge: it has a single fork, 42 lines from upstream, and also passes the syntax check with its recorded command.

## What is not done yet

These files are merged and compile, but no server binary has been built from them and nothing has been relinked, so the ports remain absent
from any shipped artifact. Building `llama-server` from these copies and relinking is the next step; running it against Apex needs agreement
first.
