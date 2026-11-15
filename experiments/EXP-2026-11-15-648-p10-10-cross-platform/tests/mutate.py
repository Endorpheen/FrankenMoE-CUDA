#!/usr/bin/env python3
"""Mutation gate for the P10-10 cross-platform slices.

12 textual mutations per slice, six in each half. Each mutant is written to a
scratch copy of the module (the source file is never touched), must pass
`goose check`, and must then make the slice's differential gate fail at -O0.
Any non-zero exit or any byte difference in the gate output counts as caught.

    python3 tests/mutate.py --module feat|plat|dev|all --half 1|2|all
"""
import argparse
import hashlib
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
GOOSE = os.environ.get("GOOSE_BIN", "/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose")
CC = os.environ.get("CC", "gcc-13")
CXX = os.environ.get("CXX", "g++-13")
UP = os.environ.get("FG_UPSTREAM", "/home/end0/AI/FrankenMoE-CUDA/upstream/llama.cpp")
OUT = os.environ.get("OUT_DIR", os.path.join(ROOT, "build"))

SLICES = {
    "feat": ("feat-cpu-features.goose", "fixture_feat.cpp"),
    "plat": ("plat-paths.goose", "fixture_plat.cpp"),
    "dev": ("dev-backend-select.goose", "fixture_dev.cpp"),
}

# (name, old, new)
FEAT_H1 = [
    ("F1 fp16 needs only one hwcap bit",
     "if f_bit(hwcap, F_HWCAP_FPHP) && f_bit(hwcap, F_HWCAP_ASIMDHP) {",
     "if f_bit(hwcap, F_HWCAP_FPHP) || f_bit(hwcap, F_HWCAP_ASIMDHP) {"),
    ("F2 the SVE vector length is not masked",
     "cnt = sve_raw & F_VL_LEN_MASK;",
     "cnt = sve_raw;"),
    ("F3 a failed prctl still yields a length",
     "if (bits & F_SVE) != 0 && sve_raw >= 0 {",
     "if (bits & F_SVE) != 0 {"),
    ("F4 dotprod reads the wrong hwcap bit",
     "const F_HWCAP_ASIMDDP: i64 = 20;",
     "const F_HWCAP_ASIMDDP: i64 = 21;"),
    ("F5 sve2 reads the wrong hwcap2 bit",
     "const F_HWCAP2_SVE2: i64 = 1;",
     "const F_HWCAP2_SVE2: i64 = 2;"),
    ("F6 the Apple decode ignores the sysctl result",
     "if (ok & key) != 0 && (vals & key) != 0 {",
     "if (vals & key) != 0 {"),
]

FEAT_H2 = [
    ("F7 the Windows decode drops the SME2 bit",
     "return f_emit(pf & F_ARCH64_ALL, 0, out);",
     "return f_emit(pf & 63, 0, out);"),
    ("F8 the aarch64 dotprod weight is halved",
     "        if (bits & F_DOTPROD) == 0 {\n            return 0;\n        }\n        score = score + 2;",
     "        if (bits & F_DOTPROD) == 0 {\n            return 0;\n        }\n        score = score + 1;"),
    ("F9 AVX512 needs only four of its five bits",
     "const F_X_AVX512_REQ: i64 = 128 + 256 + 512 + 1024 + 2048;",
     "const F_X_AVX512_REQ: i64 = 128 + 256 + 512 + 1024;"),
    ("F10 the AMX_INT8 weight collides with AVX512_VNNI",
     "        score = score + 2048;",
     "        score = score + 1024;"),
    ("F11 any vector length of at least 32 enables SVE",
     "if sve_cnt == F_QK8_0 {",
     "if sve_cnt >= F_QK8_0 {"),
    ("F12 the kleidiai name tries SME before SME2",
     "    if (mask & F_K_SME) == F_K_SME {",
     "    if (mask & F_K_SME2) == F_K_SME2 {"),
]

PLAT_H1 = [
    ("P1 the LLAMA_CACHE root loses its trailing separator",
     "        var n = p_put(out, 0, llama_cache);\n        return p_ensure(out, n, sep);",
     "        var n = p_put(out, 0, llama_cache);\n        return n;"),
    ("P2 the BSD family gets the Linux getpwuid fallback",
     "        } else if platform == PLAT_LINUX {\n            if pw_dir.len == 0 {\n                return 0 - 1;\n            }\n            n = p_put(out, 0, pw_dir);\n            n = p_put(out, n, \"/.cache/\");",
     "        } else if platform != PLAT_EMSCRIPTEN {\n            if pw_dir.len == 0 {\n                return 0 - 1;\n            }\n            n = p_put(out, 0, pw_dir);\n            n = p_put(out, n, \"/.cache/\");"),
    ("P3 a missing passwd entry yields a path",
     "            if pw_dir.len == 0 {\n                return 0 - 1;\n            }\n            n = p_put(out, 0, pw_dir);\n            n = p_put(out, n, \"/.cache/\");",
     "            if pw_dir.len < 0 {\n                return 0 - 1;\n            }\n            n = p_put(out, 0, pw_dir);\n            n = p_put(out, n, \"/.cache/\");"),
    ("P4 the Apple cache directory is misspelled",
     "\"/Library/Caches/\"",
     "\"/Library/Cache/\""),
    ("P5 a missing LOCALAPPDATA yields a path",
     "        if localappdata.len == 0 {\n            return 0 - 1;\n        }",
     "        if localappdata.len < 0 {\n            return 0 - 1;\n        }"),
    ("P6 Emscripten is reported as a missing home",
     "        n = p_put(out, 0, localappdata);\n    } else {\n        return 0 - 2;\n    }",
     "        n = p_put(out, 0, localappdata);\n    } else {\n        return 0 - 1;\n    }"),
]

PLAT_H2 = [
    ("P7 the Apple config root leaves the XDG branch",
     "    if platform == PLAT_LINUX || platform == PLAT_BSD || platform == PLAT_APPLE {",
     "    if platform == PLAT_LINUX || platform == PLAT_BSD {"),
    ("P8 the config getpwuid fallback moves to BSD",
     "        } else if platform == PLAT_LINUX {\n            if pw_dir.len == 0 {\n                return 0 - 1;\n            }\n            n = p_put(out, 0, pw_dir);\n            n = p_put(out, n, \"/.config/\");",
     "        } else if platform == PLAT_BSD {\n            if pw_dir.len == 0 {\n                return 0 - 1;\n            }\n            n = p_put(out, 0, pw_dir);\n            n = p_put(out, n, \"/.config/\");"),
    ("P9 the cache file name may contain a separator",
     "    if p_has(filename, sep) {",
     "    if p_has(filename, 0) {"),
    ("P10 the medium priority value is off by one",
     "    if prio == P_MEDIUM {\n        return 0 - 5;\n    }",
     "    if prio == P_MEDIUM {\n        return 0 - 4;\n    }"),
    ("P11 TERM=dumb no longer disables colour",
     "    if p_eq(term, \"dumb\") {",
     "    if p_eq(term, \"dum\") {"),
    ("P12 the Windows environment write becomes a POSIX set",
     "    if platform == PLAT_WIN {\n        return 2;\n    }",
     "    if platform == PLAT_WIN {\n        return 1;\n    }"),
]

DEV_H1 = [
    ("D1 the device name match becomes case sensitive",
     "        if d_lower(a[i]) != d_lower(b[i]) {",
     "        if a[i] != b[i] {"),
    ("D2 the type lookup takes the wrong device",
     "        if types[i] == want {",
     "        if types[i] != want {"),
    ("D3 init_best skips the IGPU step",
     "    i = d_type_index(types, n, DEV_IGPU);\n    if i >= 0 {\n        return i;\n    }",
     "    i = d_type_index(types, n, DEV_IGPU);"),
    ("D4 init_best falls back to an accelerator",
     "    return d_type_index(types, n, DEV_CPU);",
     "    return d_type_index(types, n, DEV_ACCEL);"),
    ("D5 equal scores let the later candidate win",
     "        if has_score[i] != 0 && scores[i] > best {",
     "        if has_score[i] != 0 && scores[i] >= best {"),
    ("D6 a candidate without a score symbol is used",
     "        if has_score[i] != 0 && scores[i] > best {",
     "        if scores[i] > best {"),
]

DEV_H2 = [
    ("D7 the Windows prefix gets the POSIX spelling",
     "        n = d_put(out, 0, \"ggml-\");",
     "        n = d_put(out, 0, \"libggml-\");"),
    ("D8 the candidate extension check is relaxed",
     "    if ext.len == 3 && ext[0] == 46 && ext[1] == 115 && ext[2] == 111 {",
     "    if ext.len >= 2 {"),
    ("D9 the trailing dash of the name prefix is optional",
     "    if at >= filename.len || filename[at] != 45 {",
     "    if at > filename.len {"),
    ("D10 a failed Linux readlink yields no prefix",
     "        if platform == DEV_LINUX {\n            return d_put(out, 0, \"./\");\n        }\n        return 0;",
     "        return 0;"),
    ("D11 the executable prefix loses its trailing separator",
     "    out[n] = use_sep;\n    return n + 1;",
     "    return n;"),
    ("D12 a failed current-directory lookup keeps the entry",
     "    if cwd_ok != 0 {",
     "    if cwd_ok >= 0 {"),
]

TABLES = {
    ("feat", 1): FEAT_H1, ("feat", 2): FEAT_H2,
    ("plat", 1): PLAT_H1, ("plat", 2): PLAT_H2,
    ("dev", 1): DEV_H1,  ("dev", 2): DEV_H2,
}


def build(module, tag, slice_key, source_text=None, opt="-O0"):
    """Generate, compile and link one gate; returns (ok, output)."""
    src = os.path.join(ROOT, "source", module)
    work = os.path.join(OUT, "mut", tag)
    os.makedirs(work, exist_ok=True)
    if source_text is not None:
        src = os.path.join(work, slice_key + ".goose")
        with open(src, "w") as f:
            f.write(source_text)
    gen = os.path.join(work, slice_key + ".c")
    hdr = os.path.join(work, slice_key + ".h")
    obj = os.path.join(work, tag + ".o")
    bin_path = os.path.join(work, tag + "-gate")
    r = subprocess.run([GOOSE, "--standalone", "-o", gen, "--header", hdr, src],
                       capture_output=True, text=True, timeout=300)
    if r.returncode != 0:
        return False, "goose failed: " + (r.stderr.strip().splitlines() or [""])[-1]
    r = subprocess.run([CC, opt, "-std=c11", "-w", "-c", gen, "-o", obj],
                       capture_output=True, text=True, timeout=300)
    if r.returncode != 0:
        return False, "cc failed: " + (r.stderr.strip().splitlines() or [""])[-1]
    fixture = os.path.join(work, slice_key + "-fixture.o")
    r = subprocess.run([CXX, "-O2", "-std=c++17", "-I", work, "-I", os.path.join(UP, "ggml", "src"),
                        "-c", os.path.join(HERE, SLICES[slice_key][1]), "-o", fixture],
                       capture_output=True, text=True, timeout=300)
    if r.returncode != 0:
        return False, "fixture failed: " + (r.stderr.strip().splitlines() or [""])[-1]
    r = subprocess.run([CXX, fixture, obj, "-o", bin_path],
                       capture_output=True, text=True, timeout=300)
    if r.returncode != 0:
        return False, "link failed: " + (r.stderr.strip().splitlines() or [""])[-1]
    r = subprocess.run([bin_path], capture_output=True, text=True, timeout=300)
    return r.returncode == 0, (r.stdout or "") + (r.stderr or "")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--module", default="all", choices=["feat", "plat", "dev", "all"])
    ap.add_argument("--half", default="all", choices=["1", "2", "all"])
    args = ap.parse_args()

    modules = ["feat", "plat", "dev"] if args.module == "all" else [args.module]
    halves = [1, 2] if args.half == "all" else [int(args.half)]

    os.makedirs(OUT, exist_ok=True)
    total = 0
    caught = 0
    failed = []

    for tag in modules:
        module = SLICES[tag][0]
        with open(os.path.join(ROOT, "source", module)) as f:
            original = f.read()
        base_ok, base_out = build(module, tag + "-baseline", tag, None)
        if not base_ok:
            print("BASELINE FAILED for %s: %s" % (tag, base_out.strip().splitlines()[-1]))
            return 2

        for half in halves:
            for name, old, new in TABLES[(tag, half)]:
                total += 1
                count = original.count(old)
                if count != 1:
                    print("SKIP %s [%d]: anchor occurs %d times" % (name, half, count))
                    failed.append(name)
                    continue
                mutated = original.replace(old, new, 1)
                if hashlib.sha256(mutated.encode()).hexdigest() == hashlib.sha256(original.encode()).hexdigest():
                    print("SKIP %s [%d]: mutation is a no-op" % (name, half))
                    failed.append(name)
                    continue
                ok, out = build(module, tag + "-" + str(total), tag, mutated)
                if ok and out == base_out:
                    print("MISSED %s [slice %s half %d]" % (name, tag, half))
                    failed.append(name)
                else:
                    caught += 1
                    first = ""
                    for line in out.splitlines():
                        if line.startswith("FAIL"):
                            first = " :: " + line
                            break
                    print("caught %s [slice %s half %d]%s" % (name, tag, half, first))

    print("\nmutations: %d/%d caught" % (caught, total))
    if failed:
        print("not caught: " + ", ".join(failed))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
