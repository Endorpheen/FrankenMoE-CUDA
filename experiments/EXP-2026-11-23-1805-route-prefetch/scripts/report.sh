#!/usr/bin/env bash
# Summarise one arm directory against the EXP-1786 reference answer.
RUN="$1"; D=/home/end0/.codex/worktrees/pure-goose-inkling-mini/FrankenMoE-CUDA/build/exp1786-split-planes
pid=$(cat "$RUN/pid.txt")
echo "exact=$(jq -es '(.[1].ids|length)==512 and .[0].ids==.[1].ids and .[0].content==.[1].content and .[1].timings.prompt_n==32768 and .[1].timings.predicted_n==512 and .[1].timings.cache_n==0' "$D/apex-ab/candidate/r2.json" "$RUN/r2.json")"
echo "warm_vs_meas_same=$(jq -es '.[0].ids==.[1].ids' "$RUN/r1.json" "$RUN/r2.json")"
jq -c '.timings|{prompt_ms,prompt_per_second,predicted_ms,predicted_per_second}' "$RUN/r2.json"
echo "client_s=$(cat "$RUN/client-2-seconds.txt")"
grep -E '^(VmHWM|VmSwap):' "$RUN/status-after.txt" | tr '\n' ' '; echo
awk '/^pswpout /{if(NR==FNR) a=$2;else print "pswpout_delta=" ($2-a)}' "$RUN/vmstat-start.txt" "$RUN/vmstat-after.txt"
awk -F, -v p="$pid" '$1+0==p && $2+0>m {m=$2+0} END {print "peak_vram_mib=" m}' "$RUN/vram.csv"
awk -F, -v p="$pid" '$1+0!=p && $2+0>1024 {print "FOREIGN_CUDA>1G " $0; exit}' "$RUN/vram.csv"
cut -d' ' -f1 "$RUN/binary.sha256"
pf=$(jq '.timings.prompt_per_second' "$RUN/r2.json"); awk -v p="$pf" 'BEGIN{ if (p < 2900) print "INVALID: prefill " p " < 2900 tok/s, external interference"; }'
