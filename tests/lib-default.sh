# Prints the library directory a test should measure by default.
#
# Why this exists: on 2026-11-19 `builds/flint-acc/bin/libllama.so.0.7.0` turned out to differ from
# the copy in `builds/checkpoint-20261119-EXP1476` (bea5d6af against 0ffd98c2) with an mtime of 19:18,
# after the checkpoint was taken at 19:00, and no script under tools/ writes there. Its provenance is
# therefore unknown, and a test pointed at it measures an artefact nobody accepted. Rule 22 says
# accepted sets are immutable; this helper keeps the tests on what was actually accepted.
#
# Order: $APEX_SERVER_DIR, then the newest release set, then the newest checkpoint, then the working
# directory - and the caller is expected to print which one it used.
lib_default() {
    local root=$1 c newest=""
    if [ -n "${APEX_SERVER_DIR:-}" ]; then printf '%s' "$APEX_SERVER_DIR"; return; fi
    for c in /home/end0/AI/LlamaServer/frankenmoe-release-*; do
        [ -f "$c/libllama.so.0" ] && newest=$c
    done
    if [ -z "$newest" ]; then
        for c in "$root"/builds/checkpoint-*; do
            [ -f "$c/libllama.so.0" ] && newest=$c
        done
    fi
    printf '%s' "${newest:-$root/builds/flint-acc/bin}"
}
