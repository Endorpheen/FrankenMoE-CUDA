#!/usr/bin/env bash
# Samples this process group's GPU memory while a run proceeds, so the report can separate what is constant from what
# grows. Reads only: nvidia-smi queries, no allocation and no inference of its own.
#
# usage: vram-trace.sh <log> <pid-of-server> <pid-of-test>
#   writes <log>.tsv with: time_s, used_mib_total, used_mib_server, used_mib_other
set -uo pipefail
LOG=$1; SPID=$2; TPID=${3:-0}
OUT=$LOG.tsv
: > "$OUT"
start=$(date +%s)
while kill -0 "$SPID" 2>/dev/null; do
    if [ "$TPID" != "0" ] && ! kill -0 "$TPID" 2>/dev/null; then break; fi
    total=$(nvidia-smi --query-gpu=memory.used --format=csv,noheader,nounits 2>/dev/null | head -1 | tr -dc '0-9')
    server=$(nvidia-smi --query-compute-apps=pid,used_memory --format=csv,noheader,nounits 2>/dev/null \
             | awk -v p="$SPID" -F', *' '$1 == p {print $2}' | head -1 | tr -dc '0-9')
    [ -n "$server" ] || server=0
    other=$(( ${total:-0} - ${server:-0} ))
    printf '%s\t%s\t%s\t%s\n' "$(( $(date +%s) - start ))" "${total:-0}" "$server" "$other" >> "$OUT"
    sleep 0.5
done
python3 - "$OUT" <<'PY'
import sys
rows = [l.split('\t') for l in open(sys.argv[1]).read().strip().split('\n') if l]
if not rows:
    print("vram-trace: no samples"); raise SystemExit(0)
nums = [(int(r[0]), int(r[1]), int(r[2]), int(r[3])) for r in rows]
tot = [r[1] for r in nums]; srv = [r[2] for r in nums]; oth = [r[3] for r in nums]
print(f"vram-trace: samples={len(nums)} seconds={nums[-1][0]}")
print(f"vram-trace: total start={tot[0]} peak={max(tot)} end={tot[-1]} MiB")
print(f"vram-trace: this server start={srv[0]} peak={max(srv)} end={srv[-1]} MiB")
print(f"vram-trace: everything else start={oth[0]} peak={max(oth)} end={oth[-1]} MiB")
print(f"vram-trace: growth of this server over the run = {srv[-1] - srv[0]} MiB")
PY
