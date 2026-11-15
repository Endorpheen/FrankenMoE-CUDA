#!/usr/bin/env python3
"""Mutation half of the P8-12 differential gate.

Thirty-six mutations, three sites of twelve each, and each site is split into
two halves of six:

  M*  asset content type   mime-table.goose   MIME_TEXT / MIME_EXT / MIME_EXT_ID
                                              and the extension search
  C*  Cache-Control        web-assets.goose   WEB_CACHE_* literals and the
                                              policy branches
  T*  tool dispatch        tools-pure.goose   the seven names and tool_index

Every mutation is one semantic change of the shipped code; the gate must report
at least one divergence for it, otherwise the gate is not a gate.

The mutations are applied to a copy of the composed unit, tools/server/
fg-goose-server/server.goose. Composition is a concatenation, so the functions
there are byte copies of the module sources; mutating the copy is the same edit
as mutating the module and recomposing, without touching the repository.

Nothing here starts a server and nothing loads a model.
"""
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TREE = ROOT / "flint-cpu"
GATE = ROOT / "gate"
GOOSE_BIN = Path("/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose")
UNIT = TREE / "tools/server/fg-goose-server/server.goose"
WORK = GATE / "mut"
CXX = "g++-13"
CC = "gcc-13"
INC = [
    f"-I{TREE}/tools/server/fg-goose-server",
    f"-I{TREE}/common",
    f"-I{TREE}/vendor",
    f"-I{TREE}",
    f"-I{TREE}/ggml/include",
]

# The ON run of the differential gate: the Goose arm of every site.
GATE_ARGS = ["--status", "ref", "--cors", "ref", "--mime", "ref", "--cache", "ref",
             "--tools", "ref", "--tools-raw", "ref", "--mime-raw", "ref", "--cache-raw", "ref"]


def unique(source, old, new, mid):
    if source.count(old) != 1:
        raise SystemExit(f"{mid}: source text matches {source.count(old)} times, expected 1")
    return source.replace(old, new)


MIME_TEXT = 'const MIME_TEXT: const u8[:][12] = [\n    "text/html; charset=utf-8",'
MIME_EXT = 'const MIME_EXT: const u8[:][12] = [\n    "html",'
MIME_LAST_DOT = 'fn mime_last_dot(name: const u8[:]) -> i64 {\n    var i: i64 = name.len - 1;'
MIME_OF_NAME = '    let ext = name[dot + 1 .. name.len];'
MIME_SLICE_EQ = 'fn mime_slice_eq(a: const u8[:], b: const u8[:]) -> bool {\n    if a.len != b.len {\n        return false;\n    }'
WEB_PUT = 'fn web_put(out: u8[:], at: i64, s: const u8[:]) -> i64 {\n    var i: i64 = 0;\n\n    while i < s.len {\n        out[at + i] = s[i];\n        i = i + 1;\n    }\n    return at + s.len;\n}'
WEB_CACHE = 'export fn web_cache_text(policy: i64, out: u8[:]) -> i64 {\n    if policy == WEB_POLICY_IMMUTABLE {\n        return web_put(out, 0, WEB_CACHE_IMMUTABLE);\n    }\n    if policy == WEB_POLICY_REVALIDATE || policy == WEB_POLICY_NOCACHE {\n        return web_put(out, 0, WEB_CACHE_NO_CACHE);\n    }\n    return 0 - 1;\n}'
TOOL_NAMES = 'fn tool_name_impl(idx: i64, out: u8[:]) -> i64 {\n    if idx == TOOL_READ_FILE {\n        return tp_put(out, 0, "read_file");'
TOOL_INDEX = 'export fn tool_index(name: const u8[:]) -> i64 {\n    var i: i64 = 0;\n\n    while i < TOOL_COUNT {'
TP_EQ = 'fn tp_eq(a: const u8[:], b: const u8[:]) -> bool {\n    if a.len != b.len {\n        return false;\n    }'

# (id, site, half, function, old, new, what the edit does)
MUTATIONS = [
    # ── MIME, half 1: the two tables ─────────────────────────────────────────
    ("M1", "mime", "table", "MIME_TEXT",
     MIME_TEXT, MIME_TEXT.replace('"text/html; charset=utf-8"', '"text/html;charset=utf-8"'),
     "the html type loses the space after the semicolon"),
    ("M2", "mime", "table", "MIME_TEXT",
     '    "application/javascript",', '    "text/javascript",',
     "the js type changes family"),
    ("M3", "mime", "table", "MIME_TEXT",
     '    "application/octet-stream"\n];', '    "application/octet_stream"\n];',
     "the fallback type is misspelled"),
    ("M4", "mime", "table", "MIME_EXT",
     '    "webmanifest",', '    "webmanifes",',
     "the manifest extension loses its last byte"),
    ("M5", "mime", "table", "MIME_EXT_ID",
     'const MIME_EXT_ID: i64[12] = [0, 1, 2, 3, 4, 5, 6, 7, 7, 8, 9, 10];',
     'const MIME_EXT_ID: i64[12] = [0, 1, 2, 3, 4, 5, 6, 7, 8, 8, 9, 10];',
     "jpeg answers the icon type"),
    ("M6", "mime", "table", "MIME_EXT_ID",
     'const MIME_EXT_ID: i64[12] = [0, 1, 2, 3, 4, 5, 6, 7, 7, 8, 9, 10];',
     'const MIME_EXT_ID: i64[12] = [0, 1, 2, 3, 4, 5, 5, 7, 7, 8, 9, 10];',
     "png answers the svg type"),

    # ── MIME, half 2: the extension search ──────────────────────────────────
    ("M7", "mime", "search", "mime_of_name",
     '            return MIME_EXT_ID[i];', '            return MIME_EXT_ID[MIME_EXT_N - 1 - i];',
     "the extension id is read from the wrong end of the table"),
    ("M8", "mime", "search", "mime_of_name",
     '    if dot < 0 {\n        return MIME_OCTET;\n    }', '    if dot < 0 {\n        return MIME_HTML;\n    }',
     "a name without a dot is served as html"),
    ("M9", "mime", "search", "mime_of_name",
     MIME_OF_NAME, '    let ext = name[dot .. name.len];',
     "the dot stays in the extension"),
    ("M10", "mime", "search", "mime_slice_eq",
     MIME_SLICE_EQ,
     'fn mime_slice_eq(a: const u8[:], b: const u8[:]) -> bool {\n    if a.len != b.len {\n        return true;\n    }',
     "a length mismatch is accepted as a match"),
    ("M11", "mime", "search", "MIME_EXT",
     '    "svg",', '    "SVG",',
     "the svg extension becomes case sensitive the wrong way"),
    ("M12", "mime", "search", "MIME_EXT",
     '    "woff2"\n];', '    "woff3"\n];',
     "the woff2 extension is misspelled"),

    # ── Cache-Control, half 1: the literals ─────────────────────────────────
    ("C1", "cache", "literal", "WEB_CACHE_NO_CACHE",
     'const WEB_CACHE_NO_CACHE: const u8[:] = "no-cache";',
     'const WEB_CACHE_NO_CACHE: const u8[:] = "no-cach";',
     "the revalidate literal loses its last byte"),
    ("C2", "cache", "literal", "WEB_CACHE_IMMUTABLE",
     'const WEB_CACHE_IMMUTABLE: const u8[:] = "public, max-age=31536000, immutable";',
     'const WEB_CACHE_IMMUTABLE: const u8[:] = "public, max-age=31536000, immutable ";',
     "the immutable literal gains a trailing space"),
    ("C3", "cache", "literal", "WEB_CACHE_IMMUTABLE",
     'const WEB_CACHE_IMMUTABLE: const u8[:] = "public, max-age=31536000, immutable";',
     'const WEB_CACHE_IMMUTABLE: const u8[:] = "public, max-age=3153600, immutable";',
     "the max-age loses a digit"),
    ("C4", "cache", "literal", "WEB_CACHE_NO_CACHE",
     'const WEB_CACHE_NO_CACHE: const u8[:] = "no-cache";',
     'const WEB_CACHE_NO_CACHE: const u8[:] = "no-store";',
     "the revalidate literal becomes another directive"),
    ("C5", "cache", "literal", "WEB_CACHE_IMMUTABLE",
     'const WEB_CACHE_IMMUTABLE: const u8[:] = "public, max-age=31536000, immutable";',
     'const WEB_CACHE_IMMUTABLE: const u8[:] = "public, max-age=31536000";',
     "the immutable literal loses its directive"),
    ("C6", "cache", "literal", "WEB_CACHE_NO_CACHE",
     'const WEB_CACHE_NO_CACHE: const u8[:] = "no-cache";',
     'const WEB_CACHE_NO_CACHE: const u8[:] = "no-cache, no-store";',
     "the revalidate literal gains a directive"),

    # ── Cache-Control, half 2: the branches ─────────────────────────────────
    ("C7", "cache", "branch", "web_cache_text",
     WEB_CACHE, WEB_CACHE.replace('    return 0 - 1;\n}', '    return web_put(out, 0, WEB_CACHE_NO_CACHE);\n}'),
     "an unknown policy answers the revalidate literal instead of -1"),
    ("C8", "cache", "branch", "web_cache_text",
     WEB_CACHE, WEB_CACHE.replace('        return web_put(out, 0, WEB_CACHE_IMMUTABLE);',
                                  '        return web_put(out, 0, WEB_CACHE_NO_CACHE);'),
     "the immutable policy answers the revalidate literal"),
    ("C9", "cache", "branch", "web_cache_text",
     WEB_CACHE, WEB_CACHE.replace('if policy == WEB_POLICY_REVALIDATE || policy == WEB_POLICY_NOCACHE {',
                                  'if policy == WEB_POLICY_REVALIDATE {'),
     "the nocache policy falls through to -1"),
    ("C10", "cache", "branch", "WEB_POLICY_NOCACHE",
     'const WEB_POLICY_NOCACHE: i64 = 2;', 'const WEB_POLICY_NOCACHE: i64 = 5;',
     "the nocache id no longer matches the call site's"),
    ("C11", "cache", "branch", "web_cache_text",
     WEB_CACHE, WEB_CACHE.replace('        return web_put(out, 0, WEB_CACHE_IMMUTABLE);',
                                  '        return web_put(out, 1, WEB_CACHE_IMMUTABLE);'),
     "the immutable text is written one byte late"),
    ("C12", "cache", "branch", "web_put",
     WEB_PUT, WEB_PUT.replace('    return at + s.len;\n}', '    return at + s.len - 1;\n}'),
     "web_put reports one byte less than it wrote"),

    # ── tool dispatch, half 1: the names ────────────────────────────────────
    ("T1", "tools", "names", "tool_name_impl",
     TOOL_NAMES, TOOL_NAMES.replace('"read_file"', '"read_fil"'),
     "read_file loses its last byte"),
    ("T2", "tools", "names", "tool_name_impl",
     'return tp_put(out, 0, "grep_search");', 'return tp_put(out, 0, "Grep_search");',
     "grep_search changes case"),
    ("T3", "tools", "names", "tool_name_impl",
     'return tp_put(out, 0, "get_info");', 'return tp_put(out, 0, "get_information");',
     "get_info is renamed"),
    ("T4", "tools", "names", "tool_name_impl",
     'return tp_put(out, 0, "exec_shell_command");', 'return tp_put(out, 0, "exec_shell");',
     "exec_shell_command is shortened"),
    ("T5", "tools", "names", "tool_name_impl",
     'return tp_put(out, 0, "edit_file");', 'return tp_put(out, 0, "edit");',
     "edit_file is shortened"),
    ("T6", "tools", "names", "tool_name_impl",
     'return tp_put(out, 0, "write_file");', 'return tp_put(out, 0, "write_file ");',
     "write_file gains a trailing space"),

    # ── tool dispatch, half 2: the search ───────────────────────────────────
    ("T7", "tools", "search", "tool_index",
     '        if tp_eq(name, buf[0 .. n]) {\n            return i;\n        }',
     '        if tp_eq(name, buf[0 .. n]) {\n            return i + 1;\n        }',
     "the index is off by one"),
    ("T8", "tools", "search", "tool_index",
     TOOL_INDEX, TOOL_INDEX.replace('while i < TOOL_COUNT {', 'while i < TOOL_COUNT - 1 {'),
     "the last tool of the table is never found"),
    ("T9", "tools", "search", "tool_index",
     '        if tp_eq(name, buf[0 .. n]) {', '        if tp_eq(name, buf[0 .. n - 1]) {',
     "the compare drops the last byte of the tool name"),
    ("T10", "tools", "search", "tool_index",
     '        if tp_eq(name, buf[0 .. n]) {\n            return i;\n        }\n        i = i + 1;\n    }\n    return 0 - 1;\n}',
     '        if tp_eq(name, buf[0 .. n]) {\n            return i;\n        }\n        i = i + 1;\n    }\n    return 0;\n}',
     "an unknown name answers the first tool instead of -1"),
    ("T11", "tools", "search", "tp_eq",
     TP_EQ, 'fn tp_eq(a: const u8[:], b: const u8[:]) -> bool {\n    if a.len != b.len {\n        return true;\n    }',
     "a length mismatch is accepted as a match"),
    ("T12", "tools", "search", "TOOL_GET_INFO",
     'const TOOL_GET_INFO: i64 = 6;', 'const TOOL_GET_INFO: i64 = 5;',
     "two tools share one id, so get_info has no name"),
]


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, **kw)


def build_gate(unit_obj, out):
    """Link the gate against the shipped bridge/switches and one unit object."""
    objs = [str(p) for p in sorted((GATE / "obj").glob("*.o")) if not p.name.startswith("host_server.")]
    objs.append(str(GATE / "obj" / "host_server.c.o"))
    objs.append(str(unit_obj))
    ui = str(TREE / "build-on" / "tools" / "ui" / "libllama-ui.a")
    cmd = [CXX, "-O1", "-std=c++17", "-w", *INC, f"-I{TREE}/build-on/tools/ui",
           str(GATE / "gate.cpp"), *objs, ui, "-o", str(out), "-lz"]
    r = run(cmd)
    if r.returncode != 0:
        print(r.stderr[-2000:], file=sys.stderr)
        raise SystemExit(f"link failed for {out}")
    return out


def build_unit(goose_src, tag):
    c = WORK / f"{tag}.c"
    h = WORK / f"{tag}.h"
    r = run([str(GOOSE_BIN), "--standalone", "-o", str(c), "--header", str(h), str(goose_src)])
    if r.returncode != 0:
        raise SystemExit(f"goose failed for {tag}: {r.stderr[-2000:]}")
    o = WORK / f"{tag}.o"
    r = run([CC, "-O1", "-w", "-std=c11", "-c", str(c), "-o", str(o)])
    if r.returncode != 0:
        raise SystemExit(f"gcc failed for {tag}: {r.stderr[-2000:]}")
    return o


def gate_verdict(binary):
    r = run([str(binary), *GATE_ARGS])
    summary = [ln for ln in r.stdout.splitlines() if ln.startswith("# cases=")]
    if summary:
        div = int(summary[-1].split("divergences=")[1].split()[0])
    else:
        # No summary line: the mutated unit died on the corpus. Any line it did
        # answer with a DIFF still counts, and the death itself is a divergence
        # (the module must answer, not abort).
        div = sum(1 for ln in r.stdout.splitlines() if ln.endswith("\tDIFF"))
        if r.returncode != 0 and div == 0 and r.stdout.strip():
            div = 1
    return r.returncode, div, r.stdout + r.stderr


def main():
    if WORK.exists():
        shutil.rmtree(WORK)
    WORK.mkdir(parents=True)
    # The linkage objects come from the current ON build.
    src = UNIT.read_text()
    (GATE / "obj").mkdir(exist_ok=True)
    for name in ("fg-goose-server.cpp.o", "fg-goose-switches.cpp.o", "host_server.c.o"):
        found = list(TREE.glob(f"build-on/tools/server/CMakeFiles/fg-goose-server-core.dir/**/{name}"))
        if not found:
            raise SystemExit(f"missing shipped object {name} in build-on")
        shutil.copy2(found[0], GATE / "obj" / name)

    rows = []
    # Control: the unmutated unit must pass, otherwise every "kill" is noise.
    control_goose = WORK / "control.goose"
    control_goose.write_text(src)
    control_o = build_unit(control_goose, "control")
    control_bin = build_gate(control_o, WORK / "gate-control")
    rc, div, _ = gate_verdict(control_bin)
    rows.append(("C0", "control", "-", "no edit", div, "PASS" if rc == 0 else "FAIL"))
    print(f"C0 control rc={rc} divergences={div}", flush=True)
    if rc != 0:
        raise SystemExit("control gate failed; fix the gate before mutating")

    for mid, site, half, fn, old, new, note in MUTATIONS:
        mutated = unique(src, old, new, mid)
        if mutated == src:
            raise SystemExit(f"{mid}: edit is a no-op")
        g = WORK / f"{mid}.goose"
        g.write_text(mutated)
        o = build_unit(g, mid)
        b = build_gate(o, WORK / f"gate-{mid}")
        rc, div, out = gate_verdict(b)
        killed = rc != 0 and div > 0
        rows.append((mid, f"{site}/{half}", fn, note, div, "KILLED" if killed else "SURVIVED"))
        print(f"{mid:3s} {site:5s} {half:7s} {fn:18s} divergences={div:2d} "
              f"{'KILLED' if killed else 'SURVIVED'}  ({note})", flush=True)
        (WORK / f"{mid}.gate.txt").write_text(out)

    out_tsv = ROOT / "audit" / "mutations-B.tsv"
    with out_tsv.open("w") as f:
        f.write("id\tsite/half\tfunction\tedit\tdivergences\tverdict\n")
        for row in rows:
            f.write("\t".join(str(c) for c in row) + "\n")

    ok = rows[0][5] == "PASS"
    for site in ("mime", "cache", "tools"):
        half1 = [r for r in rows if r[1] == f"{site}/table" or r[1] == f"{site}/literal" or r[1] == f"{site}/names"]
        half2 = [r for r in rows if r[1] in (f"{site}/search", f"{site}/branch")]
        k1 = sum(1 for r in half1 if r[5] == "KILLED")
        k2 = sum(1 for r in half2 if r[5] == "KILLED")
        print(f"\n{site}: half 1 {k1}/{len(half1)} killed, half 2 {k2}/{len(half2)} killed")
        ok = ok and k1 == len(half1) and k2 == len(half2)
    total = sum(1 for r in rows[1:] if r[5] == "KILLED")
    print(f"total {total}/{len(rows) - 1} killed; control {'PASS' if rows[0][5] == 'PASS' else 'FAIL'}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
