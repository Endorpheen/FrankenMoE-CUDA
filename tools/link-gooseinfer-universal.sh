#!/usr/bin/env bash
set -euo pipefail

target=${1:?usage: link-gooseinfer-universal.sh linux|windows INPUT_LIST OUTPUT}
list=${2:?Missing object list}
output=${3:?Missing output path}

case "$target" in
 linux)
    compiler=${CC:-gcc-13}
    options=(-Wl,--gc-sections)
    libraries=(-lm -ldl -pthread)
    ;;
 windows)
    compiler=${WINDOWS_CC:-x86_64-w64-mingw32-gcc}
    options=(-static -Wl,--gc-sections,--no-insert-timestamp,--stack,67108864)
    libraries=(-lws2_32 -lpsapi -lwinpthread -lm)
    ;;
 *)
    printf 'Unknown GooseInfer target: %s\n' "$target" >&2
    exit 2
    ;;
esac

mapfile -t inputs < "$list"
if [[ ${#inputs[@]} -eq 0 ]]; then
    printf 'Empty GooseInfer linker input list: %s\n' "$list" >&2
    exit 1
fi
for input in "${inputs[@]}"; do
    test -s "$input" || { printf 'Missing GooseInfer linker input: %s\n' "$input" >&2; exit 1; }
done
mkdir -p -- "$(dirname -- "$output")"
exec "$compiler" "${options[@]}" "${inputs[@]}" "${libraries[@]}" -o "$output"
