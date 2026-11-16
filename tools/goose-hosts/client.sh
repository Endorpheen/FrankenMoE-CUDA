#!/usr/bin/env bash
# Shell client for the Goose decision hosts (tools/goose-hosts/*-main.c).
#
# Product scripts source this file and call:
#
#   if goosehost_init "$ROOT" env BMOE_GOOSE_ENV; then ... goosehost_ask ... fi
#
# A host is on by default (BMOE_GOOSE_<NAME>=1); the flag exists only to turn it
# off. A missing host binary is not fatal: the caller falls back to its inline
# decisions and the client says so on stderr.
#
# Protocol: one request line per call, `OK <value>` in reply, strings hex
# encoded, `-` for the empty string. Nothing here decides anything.

goosehost_init() {
    local root=$1 name=$2 optout=$3
    GOOSEHOST_BIN="${root}/build/goose-hosts/${name}"
    GOOSEHOST_ON=1
    eval "local off=\${${optout}:-1}"
    if [ "${off}" = "0" ]; then
        GOOSEHOST_ON=0
        return 1
    fi
    if [ ! -x "${GOOSEHOST_BIN}" ]; then
        echo "$(basename "$0"): ${GOOSEHOST_BIN} is missing, using the inline decisions" \
             "(build it with: bash tools/goose-hosts/build.sh)" >&2
        GOOSEHOST_ON=0
        return 1
    fi
    return 0
}

# Hex of a value; "-" is the empty string.
goosehost_hex() {
    if [ -z "${1:-}" ]; then printf '%s' '-'; return; fi
    printf '%s' "$1" | od -An -tx1 | tr -d ' \n'
}

# One request, one reply; prints the value with `OK ` stripped.
goosehost_ask() {
    local reply
    reply=$("${GOOSEHOST_BIN}" <<<"$1") || {
        echo "$(basename "$0"): goose host ${GOOSEHOST_BIN} failed" >&2
        exit 2
    }
    case "${reply}" in
        OK\ *) printf '%s' "${reply#OK }" ;;
        *) echo "$(basename "$0"): goose host refused: ${reply}" >&2; exit 2 ;;
    esac
}

# Hex reply -> raw bytes, newlines preserved.
goosehost_raw() {
    if [ "$1" = "-" ]; then printf '%s' ''; return; fi
    printf '%s' "$1" | xxd -r -p
}
