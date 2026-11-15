#!/bin/bash
# Staged S1.2 split-plan differential.
#
# The native gguf-split tool is built from the pinned upstream source in both
# builds. build-off has the native predicate and the native writer; build-on
# has the Goose split plan (gsp_should_split) and the Goose writer. Both read
# the same synthetic input, and every plan line, shard name, shard byte and
# merged file must be identical.
#
# Usage: bash scripts/split-gate.sh [work-dir]
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORK="${1:-/tmp/fg630-split}"
# The mutation runs point these two at other build directories; the default is
# the staged OFF/ON pair.
OFF="${OFF_DIR:-$ROOT/build-off}"
ON="${ON_DIR:-$ROOT/build-on}"

pass=0
fail=0

ok()   { pass=$((pass + 1)); printf 'OK\t%s\t%s\n' "$1" "$2"; }
bad()  { fail=$((fail + 1)); printf 'FAIL\t%s\t%s\t%s\n' "$1" "$2" "$3"; }

rm -rf "$WORK"
mkdir -p "$WORK"
"$OFF/fg-gguf-make" "$WORK/in.gguf" 128 4096 > "$WORK/make.log" 2>&1 || {
    echo "cannot create the synthetic input" >&2
    exit 1
}

# run_arm <case> <binary> <tag> <args...>
run_arm() {
    local case_name="$1" bin="$2" tag="$3"
    shift 3
    mkdir -p "$WORK/$case_name/$tag"
    ( cd "$WORK/$case_name/$tag" && "$bin" "$@" "$WORK/in.gguf" "$WORK/$case_name/$tag/shard" \
        > "$WORK/$case_name/$tag/stdout.txt" 2> "$WORK/$case_name/$tag/stderr.txt" )
    return $?
}

# compare_case <case> <label-for-checks>
compare_case() {
    local case_name="$1"
    local off_dir="$WORK/$case_name/off"
    local on_dir="$WORK/$case_name/on"

    # the two arms write into different directories; the paths in the log are
    # the only allowed difference
    sed "s|$off_dir|OUT|g" "$off_dir/stdout.txt" > "$WORK/$case_name/off.norm"
    sed "s|$on_dir|OUT|g"  "$on_dir/stdout.txt"  > "$WORK/$case_name/on.norm"
    if diff -q "$WORK/$case_name/off.norm" "$WORK/$case_name/on.norm" > /dev/null; then
        ok "$case_name" "plan-and-log"
    else
        bad "$case_name" "plan-and-log" "stdout differs"
    fi

    local n_off n_on
    n_off=$(cd "$off_dir" && ls shard-*.gguf 2>/dev/null | wc -l)
    n_on=$(cd "$on_dir" && ls shard-*.gguf 2>/dev/null | wc -l)
    if [ "$n_off" = "$n_on" ] && [ "$n_off" -gt 0 ]; then
        ok "$case_name" "shard-count=$n_off"
    else
        bad "$case_name" "shard-count" "off=$n_off on=$n_on"
    fi

    local f mismatch=0
    for f in $(cd "$off_dir" && ls shard-*.gguf 2>/dev/null); do
        if ! cmp -s "$off_dir/$f" "$on_dir/$f"; then
            bad "$case_name" "shard-bytes $f" "differs"
            mismatch=1
        fi
    done
    if [ "$mismatch" = 0 ] && [ "$n_off" -gt 0 ]; then
        ok "$case_name" "shard-bytes"
    fi
}

# 0. full-file parity through gguf_write_to_file: header, KV, descriptors,
#    alignment padding and the whole data section
"$ON/fg-gguf-make" "$WORK/in-on.gguf" 128 4096 > /dev/null 2>&1
if cmp -s "$WORK/in.gguf" "$WORK/in-on.gguf"; then
    ok "make" "full-file-bytes"
else
    bad "make" "full-file-bytes" "differs"
fi

# 1. size mode: three shards
run_arm size-1M "$OFF/fg-gguf-split" off --split --split-max-size 1M
run_arm size-1M "$ON/fg-gguf-split"  on  --split --split-max-size 1M
compare_case size-1M

# 2. tensor mode: four shards
run_arm tensor-32 "$OFF/fg-gguf-split" off --split --split-max-tensors 32
run_arm tensor-32 "$ON/fg-gguf-split"  on  --split --split-max-tensors 32
compare_case tensor-32

# 3. tensor mode with an empty first shard
run_arm tensor-40-notf "$OFF/fg-gguf-split" off --split --split-max-tensors 40 --no-tensor-first-split
run_arm tensor-40-notf "$ON/fg-gguf-split"  on  --split --split-max-tensors 40 --no-tensor-first-split
compare_case tensor-40-notf

# 4. dry run: the plan text only, no files
run_arm dryrun-1M "$OFF/fg-gguf-split" off --split --split-max-size 1M --dry-run
run_arm dryrun-1M "$ON/fg-gguf-split"  on  --split --split-max-size 1M --dry-run
if diff -q "$WORK/dryrun-1M/off/stdout.txt" "$WORK/dryrun-1M/on/stdout.txt" > /dev/null; then
    ok "dryrun-1M" "plan-text"
else
    bad "dryrun-1M" "plan-text" "differs"
fi

# 4b. an input whose padded tensor bytes reach 1,000,000 exactly, so a size
#     limit of 1M is hit on equality: `>` keeps the tensor, `>=` splits
mkdir -p "$WORK/exact/off" "$WORK/exact/on"
"$OFF/fg-gguf-make" "$WORK/exact/in.gguf" 62 4096 144 > /dev/null 2>&1
( cd "$WORK/exact" && "$OFF/fg-gguf-split" --split --split-max-size 1M in.gguf off/shard > off.txt 2>&1 )
( cd "$WORK/exact" && "$ON/fg-gguf-split"  --split --split-max-size 1M in.gguf on/shard  > on.txt 2>&1 )
# the tool interleaves its stdout/stderr differently between runs, so this
# case compares the plan lines only
grep -E '^(n_split|split [0-9])' "$WORK/exact/off.txt" > "$WORK/exact/off.norm"
grep -E '^(n_split|split [0-9])' "$WORK/exact/on.txt"  > "$WORK/exact/on.norm"
if diff -q "$WORK/exact/off.norm" "$WORK/exact/on.norm" > /dev/null; then
    ok "exact-1M" "plan-text"
else
    bad "exact-1M" "plan-text" "differs"
fi
n_exact_off=$(cd "$WORK/exact/off" && ls shard-*.gguf 2>/dev/null | wc -l)
n_exact_on=$(cd "$WORK/exact/on" && ls shard-*.gguf 2>/dev/null | wc -l)
if [ "$n_exact_off" = "$n_exact_on" ] && [ "$n_exact_off" -gt 0 ]; then
    ok "exact-1M" "shard-count=$n_exact_off"
else
    bad "exact-1M" "shard-count" "off=$n_exact_off on=$n_exact_on"
fi

# 5. merge: each arm merges its own shards back into one file
mkdir -p "$WORK/merge/off" "$WORK/merge/on"
( cd "$WORK/merge/off" && "$OFF/fg-gguf-split" --merge "$WORK/size-1M/off/shard-00001-of-00003.gguf" \
    "$WORK/merge/off/merged.gguf" > stdout.txt 2>&1 )
( cd "$WORK/merge/on" && "$ON/fg-gguf-split" --merge "$WORK/size-1M/on/shard-00001-of-00003.gguf" \
    "$WORK/merge/on/merged.gguf" > stdout.txt 2>&1 )
if cmp -s "$WORK/merge/off/merged.gguf" "$WORK/merge/on/merged.gguf"; then
    ok "merge" "merged-bytes"
else
    bad "merge" "merged-bytes" "differs"
fi
sed -e "s|$WORK/merge/off|OUT|g" -e "s|$WORK/size-1M/off|IN|g" \
    "$WORK/merge/off/stdout.txt" > "$WORK/merge/off.norm"
sed -e "s|$WORK/merge/on|OUT|g" -e "s|$WORK/size-1M/on|IN|g" \
    "$WORK/merge/on/stdout.txt" > "$WORK/merge/on.norm"
if diff -q "$WORK/merge/off.norm" "$WORK/merge/on.norm" > /dev/null; then
    ok "merge" "log"
else
    bad "merge" "log" "differs"
fi

# 6. the ON arm must really call the Goose plan and the Goose writer
mkdir -p "$WORK/witness"
FG_GOOSE_GGUF_STATS=1 "$ON/fg-gguf-split" --split --split-max-tensors 32 "$WORK/in.gguf" \
    "$WORK/witness/shard" > "$WORK/witness/stdout.txt" 2> "$WORK/witness/stderr.txt"
stats=$(grep -o 'FG630_STATS.*' "$WORK/witness/stderr.txt" || true)
split_calls=$(printf '%s' "$stats" | sed -n 's/.* split=\([0-9]*\).*/\1/p')
if [ -n "$split_calls" ] && [ "$split_calls" -gt 0 ]; then
    ok "witness" "goose split calls=$split_calls"
else
    bad "witness" "goose split calls" "no FG630_STATS line"
fi
meta_calls=$(printf '%s' "$stats" | sed -n 's/.* meta_size=\([0-9]*\).*/\1/p')
if [ -n "$meta_calls" ] && [ "$meta_calls" -gt 0 ]; then
    ok "witness" "goose meta_size calls=$meta_calls"
else
    bad "witness" "goose meta_size calls" "no FG630_STATS line"
fi

printf 'FG630_SPLIT checks=%d failures=%d\n' "$((pass + fail))" "$fail"
[ "$fail" -eq 0 ]
