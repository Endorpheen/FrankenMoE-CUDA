#!/usr/bin/env bash
# EXP-052 model-free gate: build the synthetic two-variant CUDA-graph harness
# against the variants build and verify Igor's scenario list against the
# control path (graphs disabled). Runs are strictly sequential, no model files,
# no server, no profiler.
#
# For every scenario the numeric checksum line of the graphs run must equal the
# control run (GGML_CUDA_DISABLE_GRAPHS=1). Log-level assertions:
#   S1  alt      VARIANTS=2: both slot=0 and slot=1 reach decision=launch;
#                VARIANTS=1: zero decision=launch (EXP-050 trap reproduced),
#                reset/unstable/capture cycle present.
#   S1b rebuild  capture happens once, later rounds are decision=launch only
#                (a rebuild on the same metadata buffer does not invalidate the
#                captured graph - Igor's check 1).
#   S2  third    third shape causes capture (LRU eviction), checksums stay
#                equal, original shapes recover to launch.
#   S3  shift    key2 differs from key1 (new key), first_call + capture appear
#                after the shift, checksums equal.
#   S4  failupd  exec_update_failed marker present (seam), sibling slot still
#                launches afterwards, checksums equal.
#   S5  sweep    after 11 s sleep a direct_first_call reappears (key evicted),
#                re-convergence to launch, clean teardown.
#   mem          VRAM accounting from event=captured free_vram= lines and the
#                event=sizeof_node_props line.
#
# Requires build/exp052-variants and work/llama.cpp-exp052-variants to exist.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
ARCH="$ROOT/results/archive/EXP-2026-09-09-052"
BUILD="$ROOT/build/exp052-variants"

cd "$ROOT"

echo "== building harness =="
cc -O2 "$ARCH/exp052_gate.c" \
   -I "$ROOT/work/llama.cpp-exp052-variants/ggml/include" \
   -L "$BUILD/bin" -lggml -lggml-base -lggml-cuda -lm \
   -Wl,-rpath,"$BUILD/bin" \
   -o "$BUILD/exp052_gate"

PASS=0
FAIL=0

check() {
    local name="$1" ok="$2"
    if [ "$ok" = "1" ]; then
        echo "CHECK PASS: $name"
        PASS=$((PASS + 1))
    else
        echo "CHECK FAIL: $name"
        FAIL=$((FAIL + 1))
    fi
}

# run_scenario <tag> <scenario> <extra env...>
run_scenario() {
    local tag="$1" scenario="$2"; shift 2
    env "$@" GGML_GRAPH_DIAG=1 "$BUILD/exp052_gate" "$scenario" \
        > "$ARCH/gate-$tag.out" 2> "$ARCH/gate-$tag.log"
}

checksums_of() {
    grep '^gate: scenario=' "$ARCH/gate-$1.out" || true
}

echo "== control runs (GGML_CUDA_DISABLE_GRAPHS=1) =="
for s in alt rebuild third shift failupd sweep mem; do
    run_scenario "ctl-$s" "$s" GGML_CUDA_DISABLE_GRAPHS=1
done

echo "== S1 alt: VARIANTS=1 (default semantics must reproduce EXP-050) =="
run_scenario alt-v1 alt GGML_CUDA_GRAPH_VARIANTS=1
check "alt-v1 checksums equal control" \
    "$([ "$(checksums_of alt-v1)" = "$(checksums_of ctl-alt)" ] && [ -n "$(checksums_of ctl-alt)" ] && echo 1 || echo 0)"
check "alt-v1 zero launches (EXP-050 trap)" \
    "$([ "$(grep -c 'decision=launch' "$ARCH/gate-alt-v1.log")" = "0" ] && echo 1 || echo 0)"
check "alt-v1 reset/unstable/capture cycle present" \
    "$(grep -q 'decision=direct_warmup_reset' "$ARCH/gate-alt-v1.log" && \
      grep -q 'decision=direct_warmup_unstable' "$ARCH/gate-alt-v1.log" && \
      grep -q 'decision=capture' "$ARCH/gate-alt-v1.log" && echo 1 || echo 0)"

echo "== S1 alt: VARIANTS=2 (both variants must converge to launch) =="
run_scenario alt-v2 alt GGML_CUDA_GRAPH_VARIANTS=2
check "alt-v2 checksums equal control" \
    "$([ "$(checksums_of alt-v2)" = "$(checksums_of ctl-alt)" ] && echo 1 || echo 0)"
check "alt-v2 launch on slot=0" "$(grep -q 'decision=launch .*slot=0' "$ARCH/gate-alt-v2.log" && echo 1 || echo 0)"
check "alt-v2 launch on slot=1" "$(grep -q 'decision=launch .*slot=1' "$ARCH/gate-alt-v2.log" && echo 1 || echo 0)"
n_cap_alt=$(grep -c 'decision=capture' "$ARCH/gate-alt-v2.log" || true)
check "alt-v2 at most 3 captures total (2 warmups + margin), got $n_cap_alt" \
    "$([ "$n_cap_alt" -le 3 ] && [ "$n_cap_alt" -ge 1 ] && echo 1 || echo 0)"
check "alt-v2 no warmup reset after warm" \
    "$([ "$(grep -c 'decision=direct_warmup_reset' "$ARCH/gate-alt-v2.log")" = "0" ] && echo 1 || echo 0)"

echo "== S1b rebuild: captured graph survives ctx re-init on same buffer =="
run_scenario rb-v2 rebuild GGML_CUDA_GRAPH_VARIANTS=2
check "rebuild-v2 checksums equal control" \
    "$([ "$(checksums_of rb-v2)" = "$(checksums_of ctl-rebuild)" ] && echo 1 || echo 0)"
check "rebuild-v2 exactly one capture" \
    "$([ "$(grep -c 'decision=capture' "$ARCH/gate-rb-v2.log")" = "1" ] && echo 1 || echo 0)"
check "rebuild-v2 launches after capture (>=4)" \
    "$([ "$(grep -c 'decision=launch' "$ARCH/gate-rb-v2.log")" -ge 4 ] && echo 1 || echo 0)"
check "rebuild-v2 same key every round" \
    "$([ "$(grep 'gate: rebuild round' "$ARCH/gate-rb-v2.log" | sed 's/.*key=//' | sort -u | wc -l)" = "1" ] && echo 1 || echo 0)"

echo "== S2 third: LRU eviction and recovery =="
run_scenario th-v2 third GGML_CUDA_GRAPH_VARIANTS=2
check "third-v2 checksums equal control" \
    "$([ "$(checksums_of th-v2)" = "$(checksums_of ctl-third)" ] && echo 1 || echo 0)"
check "third-v2 evicted slot recaptured (>=3 captures)" \
    "$([ "$(grep -c 'decision=capture' "$ARCH/gate-th-v2.log")" -ge 3 ] && echo 1 || echo 0)"
check "third-v2 launch on slot=0 and slot=1" \
    "$(grep -q 'decision=launch .*slot=0' "$ARCH/gate-th-v2.log" && \
      grep -q 'decision=launch .*slot=1' "$ARCH/gate-th-v2.log" && echo 1 || echo 0)"

echo "== S3 shift: new key, same shape =="
run_scenario sh-v2 shift GGML_CUDA_GRAPH_VARIANTS=2
check "shift-v2 checksums equal control" \
    "$([ "$(checksums_of sh-v2)" = "$(checksums_of ctl-shift)" ] && echo 1 || echo 0)"
check "shift-v2 keys differ" \
    "$(grep -q 'gate: shift .* DIFFER' "$ARCH/gate-sh-v2.out" && echo 1 || echo 0)"
check "shift-v2 first_call after shift (>=2 total)" \
    "$([ "$(grep -c 'decision=direct_first_call' "$ARCH/gate-sh-v2.log")" -ge 2 ] && echo 1 || echo 0)"
check "shift-v2 launches on both keys" \
    "$([ "$(grep -c 'decision=launch' "$ARCH/gate-sh-v2.log")" -ge 2 ] && echo 1 || echo 0)"

echo "== S4 failupd: ExecUpdate failure destroys only the owning slot =="
run_scenario fu-v2 failupd GGML_CUDA_GRAPH_VARIANTS=2 GGML_GRAPH_TEST_FAIL_UPDATE=1
check "failupd-v2 checksums equal control" \
    "$([ "$(checksums_of fu-v2)" = "$(checksums_of ctl-failupd)" ] && echo 1 || echo 0)"
check "failupd-v2 exec_update_failed marker present" \
    "$(grep -q 'event=exec_update_failed' "$ARCH/gate-fu-v2.log" && echo 1 || echo 0)"
check "failupd-v2 exec_update_failed fired exactly once" \
    "$([ "$(grep -c 'event=exec_update_failed' "$ARCH/gate-fu-v2.log")" = "1" ] && echo 1 || echo 0)"
check "failupd-v2 sibling slot still launches after failure" \
    "$(grep -q 'decision=launch .*slot=1' "$ARCH/gate-fu-v2.log" && echo 1 || echo 0)"
fu_marker_line=$(grep -n 'event=exec_update_failed' "$ARCH/gate-fu-v2.log" | head -1 | cut -d: -f1)
fu_slot0_launch_line=$(grep -n 'decision=launch .*slot=0' "$ARCH/gate-fu-v2.log" | tail -1 | cut -d: -f1)
check "failupd-v2 re-instantiated slot=0 launches after the failure" \
    "$([ -n "$fu_marker_line" ] && [ -n "$fu_slot0_launch_line" ] && \
       [ "$fu_slot0_launch_line" -gt "$fu_marker_line" ] && echo 1 || echo 0)"

echo "== S5 sweep: 10 s eviction and clean teardown =="
run_scenario sw-v2 sweep GGML_CUDA_GRAPH_VARIANTS=2
check "sweep-v2 checksums equal control" \
    "$([ "$(checksums_of sw-v2)" = "$(checksums_of ctl-sweep)" ] && echo 1 || echo 0)"
check "sweep-v2 first_call reappears after idle (>=2 total)" \
    "$([ "$(grep -c 'decision=direct_first_call' "$ARCH/gate-sw-v2.log")" -ge 2 ] && echo 1 || echo 0)"
check "sweep-v2 teardown clean" \
    "$(grep -q 'gate: sweep teardown clean' "$ARCH/gate-sw-v2.log" && echo 1 || echo 0)"

echo "== S6 mem: measured device cost of the second variant slot =="
run_scenario mem-v1 mem GGML_CUDA_GRAPH_VARIANTS=1
run_scenario mem-v2 mem GGML_CUDA_GRAPH_VARIANTS=2
check "mem-v1 checksums equal control" \
    "$([ "$(checksums_of mem-v1)" = "$(checksums_of ctl-mem)" ] && [ -n "$(checksums_of ctl-mem)" ] && echo 1 || echo 0)"
check "mem-v2 checksums equal control" \
    "$([ "$(checksums_of mem-v2)" = "$(checksums_of ctl-mem)" ] && echo 1 || echo 0)"
used_of() { awk '/gate: mem /{b=0; a=0; for(i=1;i<=NF;i++){if($i~/^free_before=/){split($i,x,"="); b=x[2]}; if($i~/^free_after=/){split($i,x,"="); a=x[2]}}; print b-a}' "$ARCH/gate-$1.out"; }
used_ctl=$(used_of ctl-mem)
used_v1=$(used_of mem-v1)
used_v2=$(used_of mem-v2)
check "mem used grows ctl < v1 < v2 (ctl=$used_ctl v1=$used_v1 v2=$used_v2)" \
    "$([ "$used_ctl" -lt "$used_v1" ] && [ "$used_v1" -lt "$used_v2" ] && echo 1 || echo 0)"
echo "DEVICE: graph instances used ctl=$used_ctl v1(100 instances)=$used_v1 v2(200 instances)=$used_v2"
echo "DEVICE: measured extra slot (instance+graph) bytes = $(( (used_v2 - used_v1) / 100 ))"

echo "== memory accounting (from alt-v2 and ctl-alt logs) =="
grep -h 'event=sizeof_node_props' "$ARCH"/gate-*.log | sort -u || true
grep -h 'event=slot_struct' "$ARCH"/gate-*.log | sort -u || true
grep -h 'event=captured free_vram' "$ARCH/gate-alt-v2.log" || true
echo "(VRAM delta and host arithmetic are recorded in the EXP card)"

echo "== OFF check: no diagnostics without GGML_GRAPH_DIAG =="
env -u GGML_GRAPH_DIAG GGML_CUDA_GRAPH_VARIANTS=2 "$BUILD/exp052_gate" alt \
    > "$ARCH/gate-off.out" 2> "$ARCH/gate-off.log"
n_off=$(grep -c '\[EXP050' "$ARCH/gate-off.log" || true)
check "OFF emits zero diagnostic lines" "$([ "$n_off" = "0" ] && echo 1 || echo 0)"
check "OFF checksums equal control" \
    "$([ "$(checksums_of off)" = "$(checksums_of ctl-alt)" ] && echo 1 || echo 0)"

echo
echo "RESULT: PASS=$PASS FAIL=$FAIL"
[ "$FAIL" = "0" ]
