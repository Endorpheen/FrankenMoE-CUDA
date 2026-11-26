#!/usr/bin/env bash
set -euo pipefail

binary=${1:?Specify the Vulkan inference binary}
model=${2:?Specify the pinned Qwen2.5-0.5B Q4_0 GGUF}
shift 2
printf '7671c0c304e6ce5a7fc577bcb12aba01e2c155cc2efd29b2213c95b18edaf6ed  %s\n' "$model" | sha256sum --strict -c -
actual=$("$binary" "$model" "$@" --n-predict 4 -- 785 6722 315 9625 374)
expected=$'12095\n11\n323\n432'
if [[ "$actual" != "$expected" ]]; then
    printf 'Unexpected output IDs:\n%s\n' "$actual" >&2
    exit 1
fi
printf 'Qwen2.5 CPU reference output IDs: 4/4 PASS\n'
