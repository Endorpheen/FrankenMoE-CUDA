#!/usr/bin/env python3
"""Diff the Goose draft against the native legacy oracle.

Inputs (produced by build.sh):
  native.tsv         probe --tsv
  native_cases.tsv   probe --cases-tsv
  native_names.tsv   probe --names-tsv
  goose.tsv          ./host
"""

import sys

NAMES = [
    "CHATML", "LLAMA_2", "LLAMA_2_SYS", "LLAMA_2_SYS_BOS", "LLAMA_2_SYS_STRIP",
    "MISTRAL_V1", "MISTRAL_V3", "MISTRAL_V3_TEKKEN", "MISTRAL_V7", "MISTRAL_V7_TEKKEN",
    "PHI_3", "PHI_4", "FALCON_3", "ZEPHYR", "MONARCH", "GEMMA", "ORION", "OPENCHAT",
    "VICUNA", "VICUNA_ORCA", "DEEPSEEK", "DEEPSEEK_2", "DEEPSEEK_3", "DEEPSEEK_OCR",
    "COMMAND_R", "LLAMA_3", "CHATGLM_3", "CHATGLM_4", "GLMEDGE", "MINICPM",
    "EXAONE_3", "EXAONE_4", "EXAONE_MOE", "RWKV_WORLD", "GRANITE_3_X",
    "GRANITE_4_0", "GRANITE_4_1", "GIGACHAT", "MEGREZ", "YANDEX", "BAILING",
    "BAILING_THINK", "BAILING2", "LLAMA4", "SMOLVLM", "DOTS1", "HUNYUAN_MOE",
    "OPENAI_MOE", "HUNYUAN_DENSE", "HUNYUAN_VL", "KIMI_K2", "SEED_OSS", "GROK_2",
    "PANGU_EMBED", "SOLAR_OPEN",
]
NID = {n: i for i, n in enumerate(NAMES)}


def rows(path):
    with open(path, encoding="utf-8") as f:
        return [line.rstrip("\n").split("\t") for line in f if line.strip()]


def compare(tag, native, goose):
    ok = gap = bad = 0
    gaps = []
    if len(native) != len(goose):
        print(f"{tag}: line count {len(native)} != {len(goose)}")
    for a, b in zip(native, goose):
        eid = NID[a[1]]
        if b[0] != a[0]:
            print(f"{tag}: label {a[0]} != {b[0]}")
            bad += 1
            continue
        if int(b[1]) != eid:
            print(f"{tag}: detect {a[0]} native={a[1]} goose={b[1]}")
            bad += 1
            continue
        glen = int(b[2])
        if glen < 0:
            gap += 1
            gaps.append(a[1])
            continue
        if str(glen) != a[2] or b[3] != a[3]:
            print(f"{tag}: prompt {a[0]} native_len={a[2]} goose_len={glen}")
            bad += 1
        else:
            ok += 1
    print(f"{tag}: ok={ok} gaps={gap} bad={bad}")
    if gaps:
        print(f"{tag}: gap formats {sorted(set(gaps))}")
    return bad, gap


def main():
    # Optional explicit path, so parallel mutation workers never rewrite the
    # shared baseline goose.tsv.
    goose_path = sys.argv[1] if len(sys.argv) > 1 else "goose.tsv"
    goose = rows(goose_path)
    det = [r for r in goose if r[0] == "det"]
    cases = [r for r in goose if r[0].startswith(("case-", "gap-"))]
    parts = [r for r in goose if r[0] in ("parts", "parts-empty")]
    templates = [r for r in goose if not r[0].startswith(("det", "case-", "gap", "parts")) and r[0] != "built"]
    templates = templates[: len(rows("native.tsv"))]

    bad = 0
    for i, a in enumerate(rows("native_names.tsv")):
        if i >= len(det) or det[i][1] != a[1] or det[i][2] != a[2]:
            print(f"detect mismatch at {a[1]}")
            bad += 1
    print(f"detection: {len(rows('native_names.tsv')) - bad}/{len(rows('native_names.tsv'))} identical")

    b1, _ = compare("templates", rows("native.tsv"), templates)
    b2, _ = compare("cases", rows("native_cases.tsv"), cases)

    b3 = 0
    np = rows("native_parts.tsv")
    pok = 0
    if len(np) != len(parts):
        print(f"parts: line count {len(np)} != {len(parts)}")
        b3 = 1
    for a, b in zip(np, parts):
        if a[0] != b[0] or a[1:] != b[1:]:
            print(f"parts: mismatch {a[0]} native={a[1:3]} goose={b[1:3]}")
            b3 = 1
        else:
            pok += 1
    print(f"parts: ok={pok} gaps=0 bad={0 if b3 == 0 else 1}")
    return 0 if bad == 0 and b1 == 0 and b2 == 0 and b3 == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
