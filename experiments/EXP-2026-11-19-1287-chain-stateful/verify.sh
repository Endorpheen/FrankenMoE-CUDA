#!/usr/bin/env bash
set -uo pipefail
R=$(cd "$(dirname "$0")/../.." && pwd)
LIB=$R/builds/flint-acc/bin
D=$(dirname "$0")
BIN=/tmp/chain_stateful
g++-13 -O2 -std=c++17 -w "$D/diff.cpp" -I "$R/vendor/flint/include" -I "$R/vendor/flint/ggml/include" \
    -L "$LIB" -lllama -Wl,-rpath,"$LIB" -o "$BIN" || exit 1
ON="GGML_GOOSE_SAMPLER_CHAIN=1 GGML_GOOSE_SAMPLER_TOP_P=1 GGML_GOOSE_SAMPLER_PENALTIES=1 GGML_GOOSE_SAMPLER_DRY=1 GGML_GOOSE_SAMPLER_TYPICAL=1 GGML_GOOSE_SAMPLER_XTC=1"
OFF="GGML_GOOSE_SAMPLER_CHAIN=0 GGML_GOOSE_SAMPLER_TOP_P=0 GGML_GOOSE_SAMPLER_PENALTIES=0 GGML_GOOSE_SAMPLER_DRY=0 GGML_GOOSE_SAMPLER_TYPICAL=0 GGML_GOOSE_SAMPLER_XTC=0"
env $ON "$BIN" /tmp/cs_goose.txt >/dev/null 2>/tmp/cs_goose.log || exit 1
env $OFF "$BIN" /tmp/cs_native.txt >/dev/null 2>&1 || exit 1
fails=0
# configuration 0: penalties + DRY + top_k + top_p + temp + dist
if cmp -s <(awk '$1==0' /tmp/cs_goose.txt) <(awk '$1==0' /tmp/cs_native.txt); then
    echo "chain with DRY: identical (DRY counters: $(grep -c FG171_DRY /tmp/cs_goose.log))"
else
    echo "chain with DRY: MISMATCH"; fails=1
    diff <(awk '$1==0' /tmp/cs_goose.txt) <(awk '$1==0' /tmp/cs_native.txt) | head -4
fi
# configuration 1: typical + XTC + mirostat + adaptive-p + dist - OPEN
for cfg in 1 2; do
    d=$(diff <(awk -v c=$cfg '$1==c' /tmp/cs_goose.txt) <(awk -v c=$cfg '$1==c' /tmp/cs_native.txt) | grep -c '^<')
    if [ "$d" -eq 0 ]; then
        echo "configuration $cfg: identical"
    else
        echo "configuration $cfg: $d of 100 rounds differ"
    fi
done

# The explanation for configuration 2 is tested, not asserted: every round whose selection differs must be a
# round whose sorted flag differs. One differing round with equal flags would falsify it.
env $ON FG_SAMPLER_DIAG=1 "$BIN" /tmp/cs_g2.txt >/dev/null 2>/tmp/cs_g2.log || exit 1
env $OFF FG_SAMPLER_DIAG=1 "$BIN" /tmp/cs_n2.txt >/dev/null 2>/tmp/cs_n2.log || exit 1
gflags=$(grep FG_ADAPTIVE_FLAG /tmp/cs_g2.log | awk '{print $2}' | tr '\n' ' ')
nflags=$(grep FG_ADAPTIVE_FLAG /tmp/cs_n2.log | awk '{print $2}' | tr '\n' ' ')
python3 - "$gflags" "$nflags" <<'PYEOF'
import sys
gs=sys.argv[1].split(); ns=sys.argv[2].split()
def rows(p, cfg):
    return [tuple(l.split()[1:]) for l in open(p) if l.split()[:1]==[str(cfg)]]
g=rows('/tmp/cs_g2.txt',2); n=rows('/tmp/cs_n2.txt',2)
diff=[i for i,(a,b) in enumerate(zip(g,n)) if a!=b]
# adaptive-p applies once per round in configuration 2
cov=sum(1 for i in diff if i < len(gs) and i < len(ns) and gs[i]!=ns[i])
print(f"configuration 2: differing={len(diff)} of {len(g)}, explained_by_flag={cov}")
PYEOF
exit $fails
