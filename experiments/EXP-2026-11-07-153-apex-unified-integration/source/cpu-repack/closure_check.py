#!/usr/bin/env python3
import hashlib, json, sys

EXPECTED = {
    "/inputs/native/ggml/src/ggml-quants.c": "5574a2dccf7c07e75b143733e04a5412d3d8c819e7945f5217a7b83a2b2ff8ab",
    "/inputs/native/ggml/src/ggml-cpu/repack.cpp": "5fa9f24cc2631609f40fa47a64078f77dccbe9f327711af91af671cd29b8b87d",
    "/inputs/native/ggml/src/ggml-cpu/arch/x86/repack.cpp": "c69735f83caf854172cac78eed2cbc022ef81c077e5f6aca55a49d3404a02331",
    "/inputs/canonical-storage-contract.md": "7b5b55287df863b37b73c941bc5315447978060954863c279632abc9abb900df",
}

def sha(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()

errors = []
for path, want in EXPECTED.items():
    try:
        got = sha(path)
    except OSError as e:
        errors.append(f"missing {path}: {e}")
        continue
    if got != want:
        errors.append(f"hash {path}: {got} != {want}")

with open("cpu-final/closure-map.json", "r", encoding="utf-8") as f:
    closure = json.load(f)
entry = closure.get("active_ported", {}).get("Q4_K_repack_extra_buffer", {})
if entry.get("reachable") is not True or entry.get("status") != "PORTED_GOOSE":
    errors.append("Q4_K repack is not recorded as reachable/PORTED_GOOSE")
if closure.get("explicit_remaining_imports"):
    errors.append("explicit_remaining_imports is not empty")
for key in ("Q6_K_repack", "Q5_K_repack", "Q2_K_repack"):
    if key not in closure.get("unreachable_on_profile", {}):
        errors.append(f"missing unreachable proof: {key}")

if errors:
    print("FG006_CPU_FINAL_CLOSURE FAILED")
    for e in errors:
        print("ERROR", e)
    sys.exit(1)

print("FG006_CPU_FINAL_CLOSURE PASS q4_repack=PORTED q6_q5_q2=UNREACHABLE remaining=0")
