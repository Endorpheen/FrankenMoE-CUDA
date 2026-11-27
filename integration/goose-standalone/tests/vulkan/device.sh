#!/usr/bin/env bash
set -euo pipefail

probe=${1:?Specify the Linux probe executable}
icd=${2:?Specify a Vulkan software ICD manifest}
test -r "$icd"

expect_failure() {
    local expected=$1
    shift
    local output
    if output=$("$@" 2>&1); then
        printf 'Unexpected success: %s\n' "$output" >&2
        return 1
    fi
    printf '%s\n' "$output"
    [[ "$output" == *"$expected"* ]]
}

export VK_DRIVER_FILES="$icd"
unset VK_ICD_FILENAMES
expect_failure 'no eligible AMD Vulkan 1.3 device' "$probe"
expect_failure 'requested device is absent' "$probe" --device-index 0
expect_failure 'requested device is absent' "$probe" --device-index 4294967294 --allow-cpu
expect_failure 'Usage:' "$probe" --device-index -1
expect_failure 'Usage:' "$probe" --allow-cpu
expect_failure 'create instance' env VK_DRIVER_FILES=/dev/null "$probe"
result=$("$probe" --device-index 0 --allow-cpu --self-test)
printf '%s\n' "$result"
[[ "$result" == *'timeline=1 synchronization2=1'* && "$result" == *'roundtrip=65540 bytes PASS'* ]]
