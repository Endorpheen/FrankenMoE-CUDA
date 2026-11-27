# EXP-2230: Commit GigaChat4 step state in Goose

Move the cursor validation and update into the existing Goose execution
function. The adapter supplies the selected target or draft cursor as a
one-element view. The policy no longer makes an external call to C.

The CPU probe exercises each result of arena allocation, upload, compute and
commit, checking cursor, arena ownership, reset and cleanup in both modes.
All 14 scenarios passed. The updated GigaChat4 objects linked on Linux and
Windows. Full-model throughput and memory are pending.

```bash
out=$(mktemp -d)
"$GOOSE_BIN" --standalone -o "$out/step.c" --header "$out/step.h" \
    integration/goose-standalone/gigachat4/execute-step.goose
gcc-15 -O2 -I"$out" "$out/step.c" \
    experiments/EXP-2026-11-27-2230-gigachat4-goose-step-commit/probe.c -lm -o "$out/probe"
"$out/probe"
```
