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

## Built, and it runs

`link_server.py` follows the recipe the live experiments used: compile the merged file with the recorded compile command, replace
`server-context.cpp.o` inside a copy of `libserver-context.a`, relink `libllama-server-impl.so` and append all four bridge objects.

    compile rc = 0
    link rc    = 0, libllama-server-impl.so = 7,624,752 bytes
    fg268p 7 defined / 0 undefined
    fg272s 5 defined / 0 undefined
    fg274b 3 defined / 0 undefined
    fg276t 4 defined / 0 undefined
    undefined fg symbols in total: 0

The `llama-server` binary was then linked against that library, 15,960 bytes, and `--help` runs and prints the parameter block -- so this is
a real executable, not just a library that links. No model was loaded.

## The queue, and the complete server set

`server-queue.cpp.o` turned out to live in the same `libserver-context.a` as the context object, so it needed no special handling -- the script
now compiles both merged files and replaces both members:

    compile server-context.cpp rc = 0
    compile server-queue.cpp   rc = 0
    link rc = 0, libllama-server-impl.so = 7,633,696 bytes

    fg268p 7 defined / 0 undefined    fg270q 5 / 0
    fg272s 5 / 0                      fg274b 3 / 0
    fg276t 4 / 0
    undefined fg symbols in total: 0

So all five server-side ports -- checkpoints, queue, slot selection, batch compatibility, stop strings -- are in one library and in one
executable built from it, and `--help` runs.
