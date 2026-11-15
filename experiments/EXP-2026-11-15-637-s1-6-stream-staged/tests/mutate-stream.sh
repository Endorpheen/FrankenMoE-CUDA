#!/usr/bin/env bash
# Self-test of the S1.6 gate: mutate the NATIVE transcription, rebuild only the
# bridge, and require the differential gate to fail. The Goose module and the
# canon .goose files are not touched — a landed mutation would prove the gate
# compares the two implementations over the whole state, not just the return
# value.
#
# Usage: bash tests/mutate-stream.sh
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
G=${FG_GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
CXX=${FG_CXX:-g++}
OUT=$ROOT/build-stream-selftest
EV=$ROOT/evidence
mkdir -p "$OUT/mut" "$EV"

cd "$ROOT"
"$G" --standalone -o "$OUT/g.c" --header "$OUT/g.h" goose/stream/stream_policy_all.goose >/dev/null
gcc -O1 -std=c11 -w -c "$OUT/g.c" -o "$OUT/g.o"
g++ -O1 -std=c++17 -Icommon -c tests/fg_goose_stream_gate.cpp -o "$OUT/gate.o"

# name | sed expression on common/fg-stream-policy.cpp
MUTATIONS=(
  "reread_off|s/if (ever.data\[id\]) sacc.data\[3\]++;/;/"
  "budget_no_clamp|s/if (budget > cur) return budget <= total ? budget : total;/if (budget > cur) return budget;/"
  "wave_hits_emitted|s/if (miss.data\[i\] == 0) continue;/if (miss.data[i] != 0) continue;/"
  "subtract_tail_closed|s/if (b < rb.data\[i\]) {/if (b <= rb.data[i]) {/"
  "promote_ignored|s/if (promote) {/if (promote \&\& 0) {/"
  "verdict_ignores_veto|s/return gathered_refs > 0 \&\& disq_refs == 0 ? 1 : 0;/return gathered_refs > 0 ? 1 : 0;/"
  "evict_ever_clear|s/    ever\[id\] = 1;   \/\/ the next read of this entry pays twice/    ever[id] = 0;/"
)

caught=0
total=0
{
  echo "# gate self-test: mutated native transcription vs the untouched Goose module"
  for m in "${MUTATIONS[@]}"; do
      name=${m%%|*}
      expr=${m#*|}
      total=$((total + 1))
      cp common/fg-stream-policy.cpp "$OUT/mut/$name.cpp"
      sed -i "$expr" "$OUT/mut/$name.cpp"
      if ! cmp -s common/fg-stream-policy.cpp "$OUT/mut/$name.cpp"; then :; else
          echo "$name NOT_APPLIED"
          continue
      fi
      g++ -O1 -std=c++17 -DFG_GOOSE_STREAM=1 -Igoose-gen -Icommon \
          -c "$OUT/mut/$name.cpp" -o "$OUT/mut/$name.o"
      g++ "$OUT/gate.o" "$OUT/mut/$name.o" "$OUT/g.o" -o "$OUT/mut/$name.bin"
      line=$("$OUT/mut/$name.bin" | tail -1 || true)
      fails=$(sed -n 's/.*failures=\([0-9]*\).*/\1/p' <<<"$line")
      if [ "${fails:-0}" -gt 0 ]; then caught=$((caught + 1)); fi
      echo "$name caught=$([ "${fails:-0}" -gt 0 ] && echo yes || echo NO) $line"
  done
  echo "FG641_SELFTEST caught=$caught/$total"
} | tee "$EV/gate-selftest.txt"
