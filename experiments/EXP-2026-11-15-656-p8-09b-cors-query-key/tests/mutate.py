#!/usr/bin/env python3
"""P8-09 batch 2 mutation run: twelve mutations per slice, two halves of six.

    bash tests/build.sh all           # once, so build/httplib.o and the gates exist
    python3 tests/mutate.py 1         # half 1: six cors, six qry, six key
    python3 tests/mutate.py 2         # half 2: the other six of each

Each mutation rewrites one decision of one Goose module, regenerates that
module, compiles the generated C at -O0, relinks the slice fixture against the
already built native dependencies and reruns its gate. A surviving mutation is a
hole in the gate; every mutation must be caught. Builds are serial on purpose.
"""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path

B = Path(__file__).resolve().parent.parent
OUT = B / "build"
WORK = OUT / "mut"
GOOSE = "/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose"
REPO = Path("/home/end0/AI/FrankenMoE-CUDA")
VENDOR = REPO / "upstream/llama.cpp/vendor"
COMMON = REPO / "upstream/llama.cpp/common"
CC = "gcc-13"
CXX = "g++-13"

SRC = {
    "cors": B / "source" / "cors-policy.goose",
    "qry": B / "source" / "query-transport.goose",
    "key": B / "source" / "api-key-policy.goose",
}
FIX = {
    "cors": B / "tests" / "fixture_cors.cpp",
    "qry": B / "tests" / "fixture_qry.cpp",
    "key": B / "tests" / "fixture_key.cpp",
}
SUM = {"cors": "FGCORS", "qry": "FQRY", "key": "FKEY"}

# ── half 1: six per slice ───────────────────────────────────────────────────
M1 = [
    ("cors", "wildcard-with-credentials branch is disabled",
     '    if credentials != 0 && cors_eq(origins, "*") {',
     '    if credentials != 0 && cors_eq(origins, "**") {'),
    ("cors", "the localhost branch stops checking the origin host",
     '        if origin.len != 0 && origin_local != 0 {',
     '        if origin.len != 0 {'),
    ("cors", "the non-localhost origin no longer warns",
     '        if origin.len != 0 {\n            res[1] = 1;\n        }',
     '        if origin.len != 0 {\n            res[1] = 0;\n        }'),
    ("cors", "the static branch answers with the request origin",
     '    let n = cors_put(out, 0, origins);',
     '    let n = cors_put(out, 0, origin);'),
    ("cors", "127.0.0.1 is looked up as 127.0.0.2",
     '    if cors_eq(host, "127.0.0.1") {',
     '    if cors_eq(host, "127.0.0.2") {'),
    ("cors", "::1 is looked up as ::1:0",
     '    if cors_eq(host, "::1") {',
     '    if cors_eq(host, "::1:0") {'),

    ("qry", "the decimal hex digit 9 is rejected",
     '    if c >= 48 && c <= 57 {\n        return c - 48;',
     '    if c >= 48 && c <= 56 {\n        return c - 48;'),
    ("qry", "lower-case hex digits are off by one",
     '        return c - 87;',
     '        return c - 86;'),
    ("qry", "an invalid %XX pair is replaced instead of kept",
     '                n = n + 1;\n                i = i + 3;\n            } else {\n                if n >= out.len {\n                    return 0 - 1;\n                }\n                out[n] = src[i];',
     '                n = n + 1;\n                i = i + 3;\n            } else {\n                if n >= out.len {\n                    return 0 - 1;\n                }\n                out[n] = 63;'),
    ("qry", "a form component keeps '+' as a plus",
     '        } else if src[i] == 43 && plus_space != 0 {',
     '        } else if src[i] == 43 && plus_space != 1 {'),
    ("qry", "the encoder writes a space instead of a plus",
     '        } else if c == 32 {\n            if n >= out.len {\n                return 0 - 1;\n            }\n            out[n] = 43;',
     '        } else if c == 32 {\n            if n >= out.len {\n                return 0 - 1;\n            }\n            out[n] = 32;'),
    ("qry", "the encoder writes %3B for a plus",
     '            out[n + 1] = 50;\n            out[n + 2] = 66;',
     '            out[n + 1] = 51;\n            out[n + 2] = 66;'),

    ("key", "an empty key set no longer skips validation",
     '    if koff.len - 1 <= 0 {',
     '    if koff.len - 1 < 0 {'),
    ("key", "/health is looked up as /healthz",
     '    if key_eq(path, "/health") {',
     '    if key_eq(path, "/healthz") {'),
    ("key", "/v1/health is looked up as /v1/healthz",
     '    if key_eq(path, "/v1/health") {',
     '    if key_eq(path, "/v1/healthz") {'),
    ("key", "the frontend path table is never consulted",
     '        if key_eq(path, key_name(paths, off, i)) {',
     '        if key_eq(path, paths[0 .. 0]) {'),
    ("key", "Authorization is only read when it is absurdly long",
     '    if auth.len != 0 {\n        return key_put(out, 0, auth);',
     '    if auth.len > 1000 {\n        return key_put(out, 0, auth);'),
    ("key", "the Bearer strip keeps the space",
     '        return key_put(out, 0, value[KEY_BEARER.len .. value.len]);',
     '        return key_put(out, 0, value[KEY_BEARER.len - 1 .. value.len]);'),
]

# ── half 2: six per slice ───────────────────────────────────────────────────
M2 = [
    ("cors", "a parse failure is treated as a parsed host",
     '    if parsed == 0 {\n        return 0;\n    }',
     '    if parsed < 0 {\n        return 0;\n    }'),
    ("cors", "the preflight method is misspelled",
     '    if cors_eq(method, "OPTIONS") {',
     '    if cors_eq(method, "OPTION") {'),
    ("cors", "the credentials header says false when credentials are on",
     'const CORS_CREDENTIALS_TRUE: const u8[:] = "true";',
     'const CORS_CREDENTIALS_TRUE: const u8[:] = "false";'),
    ("cors", "the credentials header says true when credentials are off",
     'const CORS_CREDENTIALS_FALSE: const u8[:] = "false";',
     'const CORS_CREDENTIALS_FALSE: const u8[:] = "true";'),
    ("cors", "the preflight body is answered as text/plain",
     'const CORS_PREFLIGHT_TYPE: const u8[:] = "text/html";',
     'const CORS_PREFLIGHT_TYPE: const u8[:] = "text/plain";'),
    ("cors", "the wildcard branch echoes the configured origins",
     '    if credentials != 0 && cors_eq(origins, "*") {\n        let n = cors_put(out, 0, origin);',
     '    if credentials != 0 && cors_eq(origins, "*") {\n        let n = cors_put(out, 0, origins);'),

    ("qry", "tabs are not trimmed around a query token",
     '    return c == 32 || c == 9;',
     '    return c == 32;'),
    ("qry", "two different raw tokens of equal length are one token",
     '                    same = false;',
     '                    same = true;'),
    ("qry", "a token with an empty key is entered into the table",
     '    if key_end == a {',
     '    if key_end == a - 1 {'),
    ("qry", "every query value comes out empty",
     '        val_from = s + 1;',
     '        val_from = b;'),
    ("qry", "the first assignment of a key wins instead of the last",
     '        var j: i64 = best + 1;',
     '        var j: i64 = total;'),
    ("qry", "a path param value decodes with the form rule",
     '            let v_at = qry_decode(raw, 0, ov_blob[v_used .. ov_blob.len]);',
     '            let v_at = qry_decode(raw, 1, ov_blob[v_used .. ov_blob.len]);'),

    ("key", "a bare \"Bearer \" is not stripped",
     '    if value.len >= KEY_BEARER.len && key_eq(value[0 .. KEY_BEARER.len], KEY_BEARER) {',
     '    if value.len > KEY_BEARER.len && key_eq(value[0 .. KEY_BEARER.len], KEY_BEARER) {'),
    ("key", "an empty Authorization no longer falls back to X-Api-Key",
     '    if cand.len == 0 {\n        cand = apikey;\n    }',
     '    if cand.len == 0 {\n        cand = auth;\n    }'),
    ("key", "the 401 body message changes case",
     '{\\"error\\":{\\"message\\":\\"Invalid API Key\\"',
     '{\\"error\\":{\\"message\\":\\"Invalid API key\\"'),
    ("key", "the 401 body reports code 403",
     '\\"code\\":401}}',
     '\\"code\\":403}}'),
    ("key", "the 401 content type loses its charset",
     'const KEY_ERROR_TYPE: const u8[:] = "application/json; charset=utf-8";',
     'const KEY_ERROR_TYPE: const u8[:] = "application/json";'),
    ("key", "the public-endpoint test is inverted",
     '    if key_public(path, paths, poff) != 0 {',
     '    if key_public(path, paths, poff) != 1 {'),
]


def build_gate(name: str, mod_c: Path, mod_h_dir: Path, gate: Path) -> subprocess.CompletedProcess:
    cmd = [CXX, "-O2", "-std=c++17", "-pthread", f"-I{mod_h_dir}", f"-I{COMMON}", f"-I{VENDOR}",
           str(FIX[name]), str(mod_c.with_suffix(".o"))]
    if name == "qry":
        cmd += [str(OUT / "httplib.o")]
    cmd += ["-o", str(gate)]
    return subprocess.run(cmd, capture_output=True, text=True)


def run_one(index: int, entry) -> str:
    name, what, old, new = entry
    src = SRC[name]
    text = src.read_text()
    n = text.count(old)
    if n != 1:
        print(f"  BROKEN  {name} #{index}: pattern occurs {n} times: {what}")
        return "broken"

    WORK.mkdir(parents=True, exist_ok=True)
    tag = f"{name}-{index}"
    goose_src = WORK / f"{tag}.goose"
    goose_src.write_text(text.replace(old, new, 1))

    gen = subprocess.run([GOOSE, "--standalone", "-o", str(WORK / f"{tag}.c"),
                          "--header", str(WORK / f"{name}.h"), str(goose_src)],
                         capture_output=True, text=True)
    if gen.returncode != 0:
        print(f"  BROKEN  {name} #{index}: goose refused the mutation: {what}")
        print("          " + (gen.stdout + gen.stderr).strip().splitlines()[-1])
        return "broken"

    comp = subprocess.run([CC, "-O0", "-std=c11", "-w", "-c", str(WORK / f"{tag}.c"),
                           "-o", str(WORK / f"{tag}.o")], capture_output=True, text=True)
    if comp.returncode != 0:
        print(f"  BROKEN  {name} #{index}: generated C does not compile: {what}")
        print("          " + comp.stderr.strip().splitlines()[-1])
        return "broken"

    gate = WORK / f"gate-{tag}"
    link = build_gate(name, WORK / f"{tag}.c", WORK, gate)
    if link.returncode != 0:
        print(f"  BROKEN  {name} #{index}: fixture does not link: {what}")
        print("          " + link.stderr.strip().splitlines()[-1])
        return "broken"

    res = subprocess.run([str(gate)], capture_output=True, text=True, timeout=300)
    out = res.stdout.strip().splitlines()
    summary = out[-1] if out else "<no output>"
    if res.returncode == 0:
        print(f"  SURVIVED {name} #{index}: {what}")
        print(f"           {summary}")
        return "survived"
    print(f"  caught  {name} #{index}: {what}")
    print(f"          {summary}")
    return "caught"


def main() -> int:
    if len(sys.argv) != 2 or sys.argv[1] not in {"1", "2"}:
        print("usage: mutate.py 1|2", file=sys.stderr)
        return 2
    half = sys.argv[1]
    table = M1 if half == "1" else M2

    if not (OUT / "httplib.o").exists():
        print("build/httplib.o is missing: run bash tests/build.sh all first", file=sys.stderr)
        return 2

    counts = {"caught": 0, "survived": 0, "broken": 0}
    per_slice = {k: {"caught": 0, "survived": 0, "broken": 0} for k in SRC}

    print(f"mutation half {half}: {len(table)} mutations")
    for i, entry in enumerate(table, 1):
        result = run_one(i, entry)
        counts[result] += 1
        per_slice[entry[0]][result] += 1

    ok = counts["survived"] == 0 and counts["broken"] == 0
    for name in ("cors", "qry", "key"):
        c = per_slice[name]
        print(f"half {half} {name}: caught={c['caught']} survived={c['survived']} broken={c['broken']}")
    print(f"half {half} total: caught={counts['caught']} survived={counts['survived']} "
          f"broken={counts['broken']} -> {'OK' if ok else 'FAIL'}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
