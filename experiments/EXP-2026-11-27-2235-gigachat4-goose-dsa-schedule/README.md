# EXP-2235: Stage the GigaChat4 DSA scheduler in Goose

The universal build now generates the experimental DSA image from tracked
sources. The default runtime keeps its existing DSA route. The candidate is
selected only by GOOSE_GIGA_DSA_CERTIFIED=1 on eligible GigaChat4 graphs.
The existing DSA and MLA dispatch policies have also moved to Goose.

The CPU probe compared 522 launches and their argument bits across 21 shapes,
including partial 512-row tiles, and exercised early exit on driver errors
and undersized workspaces. Linux and Windows linked. Existing GigaChat4
dense, vector, DSA and MLA images remained byte-identical. The experimental
image matches the previous 128-block fallback microbenchmark image built
with CUDA 12.6 ptxas. No model throughput or peak memory was measured.

```bash
out=$(mktemp -d)
src=integration/goose-standalone/universal/cpu/giga-dsa-schedule.goose
probe=experiments/EXP-2026-11-27-2235-gigachat4-goose-dsa-schedule
"$GOOSE_BIN" --standalone -o "$out/schedule.c" --header "$out/schedule.h" "$src"
gcc-13 -O2 -I"$out" "$probe/probe.c" "$out/schedule.c" -lm -o "$out/probe"
"$out/probe"
```
