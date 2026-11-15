#!/usr/bin/env python3
"""Derive the frozen port tables of this experiment from the base-commit canon.

The gate compares a committed Goose table against the native side, which
``tests/extract_native.py`` derives on every build. This script produces the
committed side: it reads the same headers and implementation tables once, and
writes the literal arrays into the Goose modules at ``// @@NAME@@`` markers.

Run it only when the canon moves on purpose; the result is reviewed and
committed. It is not part of the gate.

    python3 tests/gen_port_tables.py --inject          # rewrite the modules
    python3 tests/gen_port_tables.py --print abi       # show one table

The header identifier lists below are the selection decision of the slice: the
public numeric surface a consumer of libllama/libggml can observe. Everything
is resolved by the compiler against the real headers, never by hand.
"""
from __future__ import annotations

import subprocess
import sys
import tempfile
from pathlib import Path

EXP = Path(__file__).resolve().parent.parent
NATIVE = Path("/home/end0/AI/FrankenMoE-CUDA/upstream/llama.cpp")
CXX = "g++-13"
GOOSE = "/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose"

# ── public numeric surface (hdr_) ────────────────────────────────────────────
HDR_GGML = [
    # on-disk format
    "GGML_FILE_MAGIC", "GGML_FILE_VERSION", "GGML_QNT_VERSION", "GGML_QNT_VERSION_FACTOR",
    # object geometry and limits
    "GGML_MAX_DIMS", "GGML_MAX_PARAMS", "GGML_MAX_SRC", "GGML_MAX_N_THREADS",
    "GGML_MAX_OP_PARAMS", "GGML_MAX_NAME", "GGML_DEFAULT_N_THREADS",
    "GGML_DEFAULT_GRAPH_SIZE",
    # exit codes
    "GGML_EXIT_SUCCESS", "GGML_EXIT_ABORTED",
    # rope modes shared with llama_rope_type
    "GGML_ROPE_TYPE_NORMAL", "GGML_ROPE_TYPE_NEOX", "GGML_ROPE_TYPE_MROPE",
    "GGML_ROPE_TYPE_VISION", "GGML_ROPE_TYPE_IMROPE", "GGML_MROPE_SECTIONS",
    "GGML_N_TASKS_MAX",
    # precision and op hints
    "GGML_PREC_DEFAULT", "GGML_PREC_F32", "GGML_HINT_NONE", "GGML_HINT_SRC0_IS_HADAMARD",
    # status codes stored in ggml_status
    "GGML_STATUS_ALLOC_FAILED", "GGML_STATUS_FAILED", "GGML_STATUS_SUCCESS",
    "GGML_STATUS_ABORTED",
]

HDR_GGML_FTYPE = [
    "GGML_FTYPE_UNKNOWN", "GGML_FTYPE_ALL_F32", "GGML_FTYPE_MOSTLY_F16",
    "GGML_FTYPE_MOSTLY_Q4_0", "GGML_FTYPE_MOSTLY_Q4_1", "GGML_FTYPE_MOSTLY_Q4_1_SOME_F16",
    "GGML_FTYPE_MOSTLY_Q8_0", "GGML_FTYPE_MOSTLY_Q5_0", "GGML_FTYPE_MOSTLY_Q5_1",
    "GGML_FTYPE_MOSTLY_Q2_K", "GGML_FTYPE_MOSTLY_Q3_K", "GGML_FTYPE_MOSTLY_Q4_K",
    "GGML_FTYPE_MOSTLY_Q5_K", "GGML_FTYPE_MOSTLY_Q6_K", "GGML_FTYPE_MOSTLY_IQ2_XXS",
    "GGML_FTYPE_MOSTLY_IQ2_XS", "GGML_FTYPE_MOSTLY_IQ3_XXS", "GGML_FTYPE_MOSTLY_IQ1_S",
    "GGML_FTYPE_MOSTLY_IQ4_NL", "GGML_FTYPE_MOSTLY_IQ3_S", "GGML_FTYPE_MOSTLY_IQ2_S",
    "GGML_FTYPE_MOSTLY_IQ4_XS", "GGML_FTYPE_MOSTLY_IQ1_M", "GGML_FTYPE_MOSTLY_BF16",
    "GGML_FTYPE_MOSTLY_MXFP4", "GGML_FTYPE_MOSTLY_NVFP4", "GGML_FTYPE_MOSTLY_Q1_0",
    "GGML_FTYPE_MOSTLY_Q2_0",
]

HDR_LLAMA = [
    # session / state file formats
    "LLAMA_SESSION_VERSION", "LLAMA_STATE_SEQ_VERSION",
    "LLAMA_SESSION_MAGIC", "LLAMA_STATE_SEQ_MAGIC",
    "LLAMA_STATE_SEQ_FLAGS_NONE", "LLAMA_STATE_SEQ_FLAGS_SWA_ONLY",
    "LLAMA_STATE_SEQ_FLAGS_PARTIAL_ONLY", "LLAMA_STATE_SEQ_FLAGS_ON_DEVICE",
    # call contract
    "LLAMA_DEFAULT_SEED", "LLAMA_TOKEN_NULL",
    # vocab
    "LLAMA_VOCAB_TYPE_NONE", "LLAMA_VOCAB_TYPE_SPM", "LLAMA_VOCAB_TYPE_BPE",
    "LLAMA_VOCAB_TYPE_WPM", "LLAMA_VOCAB_TYPE_UGM", "LLAMA_VOCAB_TYPE_RWKV",
    "LLAMA_VOCAB_TYPE_PLAMO2",
    # rope
    "LLAMA_ROPE_TYPE_NONE", "LLAMA_ROPE_TYPE_NORM", "LLAMA_ROPE_TYPE_NEOX",
    "LLAMA_ROPE_TYPE_MROPE", "LLAMA_ROPE_TYPE_IMROPE", "LLAMA_ROPE_TYPE_VISION",
    # token classification
    "LLAMA_TOKEN_TYPE_UNDEFINED", "LLAMA_TOKEN_TYPE_NORMAL", "LLAMA_TOKEN_TYPE_UNKNOWN",
    "LLAMA_TOKEN_TYPE_CONTROL", "LLAMA_TOKEN_TYPE_USER_DEFINED",
    "LLAMA_TOKEN_TYPE_UNUSED", "LLAMA_TOKEN_TYPE_BYTE",
    "LLAMA_TOKEN_ATTR_UNDEFINED", "LLAMA_TOKEN_ATTR_UNKNOWN", "LLAMA_TOKEN_ATTR_UNUSED",
    "LLAMA_TOKEN_ATTR_NORMAL", "LLAMA_TOKEN_ATTR_CONTROL", "LLAMA_TOKEN_ATTR_USER_DEFINED",
    "LLAMA_TOKEN_ATTR_BYTE", "LLAMA_TOKEN_ATTR_NORMALIZED", "LLAMA_TOKEN_ATTR_LSTRIP",
    "LLAMA_TOKEN_ATTR_RSTRIP", "LLAMA_TOKEN_ATTR_SINGLE_WORD",
    # file type
    "LLAMA_FTYPE_ALL_F32", "LLAMA_FTYPE_MOSTLY_F16", "LLAMA_FTYPE_MOSTLY_Q4_0",
    "LLAMA_FTYPE_MOSTLY_Q4_1", "LLAMA_FTYPE_MOSTLY_Q8_0", "LLAMA_FTYPE_MOSTLY_Q5_0",
    "LLAMA_FTYPE_MOSTLY_Q5_1", "LLAMA_FTYPE_MOSTLY_Q2_K", "LLAMA_FTYPE_MOSTLY_Q3_K_S",
    "LLAMA_FTYPE_MOSTLY_Q3_K_M", "LLAMA_FTYPE_MOSTLY_Q3_K_L", "LLAMA_FTYPE_MOSTLY_Q4_K_S",
    "LLAMA_FTYPE_MOSTLY_Q4_K_M", "LLAMA_FTYPE_MOSTLY_Q5_K_S", "LLAMA_FTYPE_MOSTLY_Q5_K_M",
    "LLAMA_FTYPE_MOSTLY_Q6_K", "LLAMA_FTYPE_MOSTLY_IQ2_XXS", "LLAMA_FTYPE_MOSTLY_IQ2_XS",
    "LLAMA_FTYPE_MOSTLY_Q2_K_S", "LLAMA_FTYPE_MOSTLY_IQ3_XS", "LLAMA_FTYPE_MOSTLY_IQ3_XXS",
    "LLAMA_FTYPE_MOSTLY_IQ1_S", "LLAMA_FTYPE_MOSTLY_IQ4_NL", "LLAMA_FTYPE_MOSTLY_IQ3_S",
    "LLAMA_FTYPE_MOSTLY_IQ3_M", "LLAMA_FTYPE_MOSTLY_IQ2_S", "LLAMA_FTYPE_MOSTLY_IQ2_M",
    "LLAMA_FTYPE_MOSTLY_IQ4_XS", "LLAMA_FTYPE_MOSTLY_IQ1_M", "LLAMA_FTYPE_MOSTLY_BF16",
    "LLAMA_FTYPE_MOSTLY_TQ1_0", "LLAMA_FTYPE_MOSTLY_TQ2_0",
    "LLAMA_FTYPE_MOSTLY_MXFP4_MOE", "LLAMA_FTYPE_MOSTLY_NVFP4",
    "LLAMA_FTYPE_MOSTLY_Q1_0", "LLAMA_FTYPE_MOSTLY_Q2_0", "LLAMA_FTYPE_GUESSED",
    # rope scaling
    "LLAMA_ROPE_SCALING_TYPE_UNSPECIFIED", "LLAMA_ROPE_SCALING_TYPE_NONE",
    "LLAMA_ROPE_SCALING_TYPE_LINEAR", "LLAMA_ROPE_SCALING_TYPE_YARN",
    "LLAMA_ROPE_SCALING_TYPE_LONGROPE", "LLAMA_ROPE_SCALING_TYPE_MAX_VALUE",
    # pooling / attention / flash attention
    "LLAMA_POOLING_TYPE_UNSPECIFIED", "LLAMA_POOLING_TYPE_NONE",
    "LLAMA_POOLING_TYPE_MEAN", "LLAMA_POOLING_TYPE_CLS", "LLAMA_POOLING_TYPE_LAST",
    "LLAMA_POOLING_TYPE_RANK",
    "LLAMA_ATTENTION_TYPE_UNSPECIFIED", "LLAMA_ATTENTION_TYPE_CAUSAL",
    "LLAMA_ATTENTION_TYPE_NON_CAUSAL",
    "LLAMA_FLASH_ATTN_TYPE_AUTO", "LLAMA_FLASH_ATTN_TYPE_DISABLED",
    "LLAMA_FLASH_ATTN_TYPE_ENABLED",
    # placement
    "LLAMA_SPLIT_MODE_NONE", "LLAMA_SPLIT_MODE_LAYER", "LLAMA_SPLIT_MODE_ROW",
    "LLAMA_SPLIT_MODE_TENSOR",
    "LLAMA_LOAD_MODE_AUTO", "LLAMA_LOAD_MODE_NONE", "LLAMA_LOAD_MODE_MMAP",
    "LLAMA_LOAD_MODE_MLOCK", "LLAMA_LOAD_MODE_MMAP_MLOCK", "LLAMA_LOAD_MODE_DIRECT_IO",
    "LLAMA_LAZY_MODE_OFF", "LLAMA_LAZY_MODE_AUTO", "LLAMA_LAZY_MODE_ON",
    "LLAMA_CONTEXT_TYPE_DEFAULT", "LLAMA_CONTEXT_TYPE_MTP",
    # kv overrides and sampling metadata keys
    "LLAMA_KV_OVERRIDE_TYPE_INT", "LLAMA_KV_OVERRIDE_TYPE_FLOAT",
    "LLAMA_KV_OVERRIDE_TYPE_BOOL", "LLAMA_KV_OVERRIDE_TYPE_STR",
    "LLAMA_MODEL_META_KEY_SAMPLING_SEQUENCE", "LLAMA_MODEL_META_KEY_SAMPLING_TOP_K",
    "LLAMA_MODEL_META_KEY_SAMPLING_TOP_P", "LLAMA_MODEL_META_KEY_SAMPLING_MIN_P",
    "LLAMA_MODEL_META_KEY_SAMPLING_XTC_PROBABILITY",
    "LLAMA_MODEL_META_KEY_SAMPLING_XTC_THRESHOLD",
    "LLAMA_MODEL_META_KEY_SAMPLING_TEMP", "LLAMA_MODEL_META_KEY_SAMPLING_PENALTY_LAST_N",
    "LLAMA_MODEL_META_KEY_SAMPLING_PENALTY_REPEAT", "LLAMA_MODEL_META_KEY_SAMPLING_MIROSTAT",
    "LLAMA_MODEL_META_KEY_SAMPLING_MIROSTAT_TAU",
    "LLAMA_MODEL_META_KEY_SAMPLING_MIROSTAT_ETA",
]

HDR_BACKEND = ["GGML_BACKEND_META_MAX_DEVICES"]

# Runtime limits the public API exposes as functions; extracted from src/llama.cpp.
HDR_LIMITS = ["llama_max_devices", "llama_max_tensor_buft_overrides"]

CXX_PROBE = r"""
#include <cstdio>
#include <cstdint>
#include "llama.h"
#include "ggml.h"
#include "ggml-backend.h"
int main() {
    #define P(x) std::printf("%s\t%lld\n", #x, (long long) (x))
%BODY%
    return 0;
}
"""


def header_values(names: list[str]) -> dict[str, int]:
    body = "\n".join(f"    P({n});" for n in names)
    src = CXX_PROBE.replace("%BODY%", body)
    with tempfile.TemporaryDirectory() as td:
        p = Path(td) / "probe.cpp"
        p.write_text(src)
        exe = Path(td) / "probe"
        cmd = [
            CXX, "-O1", "-std=c++17",
            f"-I{NATIVE}/include", f"-I{NATIVE}/ggml/include", str(p), "-o", str(exe),
        ]
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode != 0:
            sys.exit(f"header probe failed to compile:\n{r.stderr}")
        out = subprocess.run([str(exe)], capture_output=True, text=True)
        if out.returncode != 0:
            sys.exit("header probe failed to run")
    values: dict[str, int] = {}
    for line in out.stdout.splitlines():
        k, v = line.split("\t")
        values[k] = int(v)
    missing = [n for n in names if n not in values]
    if missing:
        sys.exit(f"probe did not report: {missing}")
    return values


def resolve_values(exprs: list[str], with_common: bool = False) -> list[int]:
    """Compile each C expression against the real headers and report its value."""
    uniq = sorted(set(exprs))
    body = "\n".join(f"    std::printf(\"%d\\t%lld\\n\", {i}, (long long) ({e}));"
                      for i, e in enumerate(uniq))
    decl = '#define GGML_COMMON_DECL_CPP\n#include "ggml.h"\n#include "ggml-common.h"\n' if with_common else ""
    src = f'#include <cstdio>\n#include <cstdint>\n{decl}int main() {{\n{body}\n    return 0;\n}}\n'
    with tempfile.TemporaryDirectory() as td:
        p = Path(td) / "res.cpp"
        p.write_text(src)
        exe = Path(td) / "res"
        cmd = [CXX, "-O1", "-std=c++17", f"-I{NATIVE}/include",
               f"-I{NATIVE}/ggml/include", f"-I{NATIVE}/ggml/src", str(p), "-o", str(exe)]
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode != 0:
            sys.exit(f"expression resolver failed:\n{r.stderr}")
        out = subprocess.run([str(exe)], capture_output=True, text=True)
    got = {}
    for line in out.stdout.splitlines():
        k, v = line.split("\t")
        got[uniq[int(k)]] = int(v)
    return [got[e] for e in exprs]


def goose_str(s: str) -> str:
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def table_abi() -> dict[str, str]:
    sys.path.insert(0, str(EXP / "tests"))
    import extract_native as E

    ggml_c = NATIVE / "ggml" / "src" / "ggml.c"
    entries, _line = E.parse_type_traits(ggml_c.read_text(errors="replace"))
    ggml_h = NATIVE / "ggml" / "include" / "ggml.h"
    type_enum = E.parse_ggml_type_enum(ggml_h.read_text(errors="replace"))

    def idx_of(t: str) -> int:
        return int(t) if t.isdigit() else type_enum[t]

    rows = sorted(entries, key=lambda e: idx_of(e[0]))
    count = type_enum["GGML_TYPE_COUNT"]
    if [idx_of(r[0]) for r in rows] != list(range(count)):
        sys.exit("type table is not a dense 0..COUNT-1 sequence")

    blck_vals = resolve_values([r[2] for r in rows], with_common=True)
    size_vals = resolve_values([r[3] for r in rows], with_common=True)
    labels = [f"{i} {r[1]}" for i, r in enumerate(rows)]

    def labelled(values) -> str:
        return "\n".join(f"    {v}, // {lab}" for v, lab in zip(values, labels))

    name = ",\n".join(f"    {goose_str(r[1])}" for r in rows)
    blck = labelled(blck_vals)
    size = labelled(size_vals)
    isq = labelled([1 if r[4] else 0 for r in rows])
    return {
        "ABI_COUNT": str(count),
        "ABI_NAME": name,
        "ABI_BLCK": blck,
        "ABI_SIZE": size,
        "ABI_ISQ": isq,
    }


def table_hdr() -> dict[str, str]:
    names = HDR_GGML + HDR_GGML_FTYPE + HDR_LLAMA + HDR_BACKEND
    values = header_values(names)

    limits = {}
    llama_cpp = (NATIVE / "src" / "llama.cpp").read_text(errors="replace")
    sys.path.insert(0, str(EXP / "tests"))
    import extract_native as E

    limits["llama_max_devices"] = E.parse_limit(llama_cpp, "llama_max_devices")
    limits["llama_max_tensor_buft_overrides"] = E.parse_limit(llama_cpp, "llama_max_tensor_buft_overrides")
    values.update(limits)

    keys = sorted(values)
    key_text = ",\n".join(f"    {goose_str(k)}" for k in keys)
    val_text = "\n".join(f"    {values[k]}, // {k}" for k in keys)
    return {"HDR_COUNT": str(len(keys)), "HDR_KEY": key_text, "HDR_VAL": val_text}


def table_nm() -> dict[str, str]:
    sys.path.insert(0, str(EXP / "tests"))
    import extract_native as E

    ggml_c = (NATIVE / "ggml" / "src" / "ggml.c").read_text(errors="replace")
    llama_cpp = (NATIVE / "src" / "llama.cpp").read_text(errors="replace")
    loader = (NATIVE / "src" / "llama-model-loader.cpp").read_text(errors="replace")
    model = (NATIVE / "src" / "llama-model.cpp").read_text(errors="replace")

    ftype_rows, ftype_default, ftype_prefix = E.parse_ftype(loader)
    ftype_enum = E.parse_enum((NATIVE / "include" / "llama.h").read_text(errors="replace"), "llama_ftype")
    flash = E.parse_cases(
        llama_cpp,
        "const char * llama_flash_attn_type_name(enum llama_flash_attn_type flash_attn_type) {",
        "flash",
        3,
    )
    load = E.parse_cases(llama_cpp, "const char * llama_load_mode_name(enum llama_load_mode load_mode) {", "load", 6)
    from_str = E.parse_from_str(llama_cpp)
    meta = E.parse_cases(model, "const char * llama_model_meta_key_str(llama_model_meta_key key) {", "meta", 12)
    status, status_default = E.parse_status(ggml_c)

    out: dict[str, str] = {}
    out["NM_FTYPE_COUNT"] = str(len(ftype_rows))
    out["NM_FTYPE_GUESSED"] = str(ftype_enum["LLAMA_FTYPE_GUESSED"])
    out["NM_FTYPE_VAL"] = ",\n".join(f"    {ftype_enum[k]}" for k, _ in ftype_rows)
    out["NM_FTYPE_TXT"] = ",\n".join(f"    {goose_str(v)}" for _, v in ftype_rows)
    out["NM_FTYPE_DEFAULT"] = goose_str(ftype_default)
    out["NM_FTYPE_PREFIX"] = goose_str(ftype_prefix)

    out["NM_LOAD_COUNT"] = str(len(load))
    load_enum = E.parse_enum((NATIVE / "include" / "llama.h").read_text(errors="replace"), "llama_load_mode")
    out["NM_LOAD_VAL"] = ",\n".join(f"    {load_enum[k]}" for k, _ in load)
    out["NM_LOAD_TXT"] = ",\n".join(f"    {goose_str(v)}" for _, v in load)
    out["NM_FROM_STR_COUNT"] = str(len(from_str))
    out["NM_FROM_STR_TXT"] = ",\n".join(f"    {goose_str(t)}" for _, t in from_str)
    out["NM_FROM_STR_VAL"] = ",\n".join(f"    {load_enum[v]}" for v, _ in from_str)

    flash_enum = E.parse_enum((NATIVE / "include" / "llama.h").read_text(errors="replace"), "llama_flash_attn_type")
    out["NM_FLASH_COUNT"] = str(len(flash))
    out["NM_FLASH_VAL"] = ",\n".join(f"    {flash_enum[k]}" for k, _ in flash)
    out["NM_FLASH_TXT"] = ",\n".join(f"    {goose_str(v)}" for _, v in flash)

    meta_enum = E.parse_enum((NATIVE / "include" / "llama.h").read_text(errors="replace"), "llama_model_meta_key")
    out["NM_META_COUNT"] = str(len(meta))
    out["NM_META_VAL"] = ",\n".join(f"    {meta_enum[k]}" for k, _ in meta)
    out["NM_META_TXT"] = ",\n".join(f"    {goose_str(v)}" for _, v in meta)

    out["NM_STATUS_COUNT"] = str(len(status))
    ggml_h = (NATIVE / "ggml" / "include" / "ggml.h").read_text(errors="replace")
    status_enum = E.parse_enum(ggml_h, "ggml_status")
    out["NM_STATUS_VAL"] = ",\n".join(f"    {status_enum[v]}" for v, _ in status)
    out["NM_STATUS_TXT"] = ",\n".join(f"    {goose_str(t)}" for _, t in status)
    out["NM_STATUS_DEFAULT"] = goose_str(status_default)
    return out


TABLES = {"abi": table_abi, "hdr": table_hdr, "nm": table_nm}
TEMPLATES = Path(__file__).resolve().parent / "templates"
MODULES = {
    "ABI_NAME": "source/ggml-type-table.goose",
    "ABI_BLCK": "source/ggml-type-table.goose",
    "ABI_SIZE": "source/ggml-type-table.goose",
    "ABI_ISQ": "source/ggml-type-table.goose",
    "ABI_COUNT": "source/ggml-type-table.goose",
    "HDR_KEY": "source/public-enum-values.goose",
    "HDR_VAL": "source/public-enum-values.goose",
    "HDR_COUNT": "source/public-enum-values.goose",
    "NM_FTYPE_VAL": "source/public-name-tables.goose",
    "NM_FTYPE_TXT": "source/public-name-tables.goose",
    "NM_FTYPE_DEFAULT": "source/public-name-tables.goose",
    "NM_FTYPE_PREFIX": "source/public-name-tables.goose",
    "NM_FTYPE_COUNT": "source/public-name-tables.goose",
    "NM_FTYPE_GUESSED": "source/public-name-tables.goose",
    "NM_LOAD_VAL": "source/public-name-tables.goose",
    "NM_LOAD_TXT": "source/public-name-tables.goose",
    "NM_LOAD_COUNT": "source/public-name-tables.goose",
    "NM_FROM_STR_TXT": "source/public-name-tables.goose",
    "NM_FROM_STR_VAL": "source/public-name-tables.goose",
    "NM_FROM_STR_COUNT": "source/public-name-tables.goose",
    "NM_FLASH_VAL": "source/public-name-tables.goose",
    "NM_FLASH_TXT": "source/public-name-tables.goose",
    "NM_FLASH_COUNT": "source/public-name-tables.goose",
    "NM_META_VAL": "source/public-name-tables.goose",
    "NM_META_TXT": "source/public-name-tables.goose",
    "NM_META_COUNT": "source/public-name-tables.goose",
    "NM_STATUS_VAL": "source/public-name-tables.goose",
    "NM_STATUS_TXT": "source/public-name-tables.goose",
    "NM_STATUS_COUNT": "source/public-name-tables.goose",
    "NM_STATUS_DEFAULT": "source/public-name-tables.goose",
}


def main() -> int:
    args = sys.argv[1:]
    if not args:
        print(__doc__)
        return 2
    if args[0] == "--print":
        which = args[1]
        for k, v in TABLES[which]().items():
            print(f"## {k}")
            print(v)
        return 0
    if args[0] == "--inject":
        merged: dict[str, str] = {}
        for fn in TABLES.values():
            merged.update(fn())
        injected = 0
        for rel in sorted(set(MODULES.values())):
            tmpl = TEMPLATES / (Path(rel).stem + ".tmpl")
            text = tmpl.read_text()
            for name, target in MODULES.items():
                if target == rel:
                    text = text.replace(f"@@{name}@@", merged[name])
            if "@@" in text:
                sys.exit(f"inject: unresolved marker in {tmpl}")
            (EXP / rel).write_text(text)
            injected += 1
        names = HDR_GGML + HDR_GGML_FTYPE + HDR_LLAMA + HDR_BACKEND
        out = ["/* Generated by tests/gen_port_tables.py -- do not edit.",
               "   One HDR() line per public identifier frozen by the hdr_ slice. */"]
        out += [f"HDR({n})" for n in names]
        (EXP / "tests" / "native_keys.inc").write_text("\n".join(out) + "\n")
        print(f"inject: filled {len(MODULES)} tables into {injected} modules from {TEMPLATES} "
              f"(and {len(names)} keys into tests/native_keys.inc)")
        return 0
    if args[0] == "--keys":
        names = HDR_GGML + HDR_GGML_FTYPE + HDR_LLAMA + HDR_BACKEND
        out = ["/* Generated by tests/gen_port_tables.py -- do not edit.",
               "   One HDR() line per public identifier frozen by the hdr_ slice. */"]
        out += [f"HDR({n})" for n in names]
        (EXP / "tests" / "native_keys.inc").write_text("\n".join(out) + "\n")
        print(f"keys: {len(names)} identifiers written to tests/native_keys.inc")
        return 0
    if args[0] == "--detach":
        merged: dict[str, str] = {}
        for fn in TABLES.values():
            merged.update(fn())
        done = 0
        for rel in sorted(set(MODULES.values())):
            path = EXP / rel
            text = path.read_text()
            for name, target in MODULES.items():
                if target == rel:
                    text = text.replace(merged[name], f"@@{name}@@")
            TEMPLATES.mkdir(exist_ok=True)
            (TEMPLATES / (Path(rel).stem + ".tmpl")).write_text(text)
            done += 1
        print(f"detach: {done} templates written to {TEMPLATES}")
        return 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main())
