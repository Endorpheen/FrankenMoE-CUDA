#!/usr/bin/env python3
"""Semantic mutation gate for draft.goose.

Each mutation is a single source edit with a reason, applied to a copy of
draft.goose. The copy is compiled, the host is built against it, and the same
native fixtures are diffed. "caught" means compare.py reported a difference.
"""

import os
import shutil
import subprocess
import sys
import concurrent.futures

BASE = os.path.dirname(os.path.abspath(__file__))
DRAFT = os.path.join(BASE, "..", "source", "runtime.goose")
GOOSE = "/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose"
MUT = os.path.join(BASE, "mut")

# (id, category, reason, old, new)
MUTATIONS = [
    (
        "M01",
        "detection order",
        "ChatML branch returns before PHI_4/SMOLVLM precedence, so <|im_sep|> is lost",
        '''    if has(t, "<|im_start|>") {
        if has(t, "<|im_sep|>") {
            return FMT_PHI_4;
        }
        if has(t, "<end_of_utterance>") {
            return FMT_SMOLVLM;
        }
        return FMT_CHATML;
    }''',
        '''    if has(t, "<|im_start|>") {
        return FMT_CHATML;
    }''',
    ),
    (
        "M02",
        "detection marker",
        "gemma marker gains a trailing space, so <start_of_turn> sources are not detected",
        'if has(t, "<start_of_turn>") {',
        'if has(t, "<start_of_turn> ") {',
    ),
    (
        "M03",
        "default template choice",
        "builtin-name table disabled, so names fall through to substring detection",
        '''    let by_name: i64 = name_id(t);
    if by_name != FMT_UNKNOWN {
        return by_name;
    }''',
        '''    // mutation: name table disabled
    let by_name: i64 = FMT_UNKNOWN;''',
    ),
    (
        "M04",
        "role separator",
        "chatml role/content separator newline becomes a colon-space",
        '''o = plain(dst, o, "<|im_start|>", get_role(roles, roffs[i], rlens[i]), "\\n",''',
        '''o = plain(dst, o, "<|im_start|>", get_role(roles, roffs[i], rlens[i]), ": ",''',
    ),
    (
        "M05",
        "role separator",
        "llama3 header ends with one newline instead of two",
        '''"<|end_header_id|>\\n\\n", trim_native(get_content(cs, coffs[i], clens[i])), "<|eot_id|>");''',
        '''"<|end_header_id|>\\n", trim_native(get_content(cs, coffs[i], clens[i])), "<|eot_id|>");''',
    ),
    (
        "M06",
        "role separator",
        "phi3 drops the newline after the role tag",
        '''o = plain(dst, o, "<|", get_role(roles, roffs[i], rlens[i]), "|>\\n",
                      get_content(cs, coffs[i], clens[i]), "<|end|>\\n");''',
        '''o = plain(dst, o, "<|", get_role(roles, roffs[i], rlens[i]), "|>",
                      get_content(cs, coffs[i], clens[i]), "<|end|>\\n");''',
    ),
    (
        "M07",
        "system handling",
        "gemma stops folding accumulated system text into the next non-model turn",
        '''                var j: i64 = sys_from;
                var pending: i64 = 0 as i64;
                while j < i {
                    if get_role(roles, roffs[j], rlens[j]) == "system" {
                        o = putb(dst, o, trim_native(get_content(cs, coffs[j], clens[j])));
                        pending = 1 as i64;
                    }
                    j += 1 as i64;
                }''',
        '''                var pending: i64 = 0 as i64;''',
    ),
    (
        "M08",
        "system handling",
        "llama2 system wrapper condition inverted: plain llama2 gets <<SYS>>, sys variant does not",
        '''                if fmt != FMT_LLAMA_2 {
                    o = putb(dst, o, "<<SYS>>\\n");''',
        '''                if fmt == FMT_LLAMA_2 {
                    o = putb(dst, o, "<<SYS>>\\n");''',
    ),
    (
        "M09",
        "BOS/EOS literal",
        "gigachat system branch drops the <s> BOS literal",
        '''        if n > 0 as i64 && get_role(roles, roffs[0], rlens[0]) == "system" {
            o = putb(dst, o, "<s>");
            o = putb(dst, o, get_content(cs, coffs[0], clens[0]));''',
        '''        if n > 0 as i64 && get_role(roles, roffs[0], rlens[0]) == "system" {
            o = putb(dst, o, get_content(cs, coffs[0], clens[0]));''',
    ),
    (
        "M10",
        "trim",
        "trim_native becomes the identity, so padded content keeps its spaces",
        '''fn trim_native(s: const u8[:]) -> const u8[:] {
    var a: i64 = 0 as i64;
    var b: i64 = s.len;
    while a < b && is_space_native(s[a]) {
        a += 1 as i64;
    }
    while b > a && is_space_native(s[b - 1 as i64]) {
        b -= 1 as i64;
    }
    return s[a..b];
}''',
        '''fn trim_native(s: const u8[:]) -> const u8[:] {
    return s;
}''',
    ),
    (
        "M11",
        "generation tail",
        "add_generation_prompt test inverted for every builder",
        "if add_ass != 0 as i64 {",
        "if add_ass == 0 as i64 {",
    ),
    (
        "M13",
        "new format: chatglm4 prefix",
        "chatglm4 loses the <sop> separator inside its fixed prefix",
        'o = putb(dst, o, "[gMASK]<sop>");',
        'o = putb(dst, o, "[gMASK]sop");',
    ),
    (
        "M14",
        "new format: exaone-moe separator",
        "exaone-moe drops the newline after the user role tag",
        'o = putb(dst, o, "<|user|>\\n");',
        'o = putb(dst, o, "<|user|>");',
    ),
    (
        "M15",
        "new format: bailing2 synthetic system",
        "bailing2 no longer inserts the fixed system turn when none is present",
        'o = putb(dst, o, "<role>SYSTEM</role>detailed thinking off<|role_end|>");',
        'o = putb(dst, o, "");',
    ),
    (
        "M16",
        "new format: hunyuan-dense tail",
        "hunyuan-dense assistant turn uses the wrong closing marker",
        'o = plain(dst, o, "<｜hy_Assistant｜>", "", "", content, "<｜hy_place▁holder▁no▁2｜>");',
        'o = plain(dst, o, "<｜hy_Assistant｜>", "", "", content, "<｜hy_place▁holder▁no▁3｜>");',
    ),
    (
        "M17",
        "new format: gpt-oss assistant close",
        "gpt-oss closes the assistant turn with <|end|> instead of <|return|>",
        'o = putb(dst, o, "<|return|>");',
        'o = putb(dst, o, "<|end|>");',
    ),
    (
        "M18",
        "new format: minicpm user wrapper",
        "minicpm stops emitting the <AI> marker after a user turn",
        'o = putb(dst, o, "<AI>");',
        'o = putb(dst, o, "");',
    ),
    (
        "M12",
        "content-parts merge",
        "merge adds the separator even when the content is still empty",
        "        if o > 0 as i64 {\n            o = putc(dst, o, 10 as u8);",
        "        if o >= 0 as i64 {\n            o = putc(dst, o, 10 as u8);",
    ),
]


def run(cmd, **kw):
    return subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, **kw)


def run_one(item):
    """Build and gate one mutation. Every write stays under mut/<id>/."""
    mid, category, reason, old, new = item
    base = open(DRAFT, encoding="utf-8").read()
    d = os.path.join(MUT, mid)

    count = base.count(old)
    if count != 1:
        return (mid, category, "not-applied", f"source matches {count} times, expected 1", "")

    src = base.replace(old, new)
    os.makedirs(d, exist_ok=True)
    src_path = os.path.join(d, "draft.goose")
    open(src_path, "w", encoding="utf-8").write(src)

    env = dict(os.environ)
    env["GOOSE_HOME"] = os.path.join(d, ".goose")
    build = run([GOOSE, "-o", os.path.join(d, "draft.c"), "--standalone",
                 "--header", os.path.join(d, "draft.h"), src_path], env=env)
    if build.returncode != 0:
        return (mid, category, "build-failed", reason, build.stdout[-600:])

    obj = run(["gcc", "-O0", "-c", os.path.join(d, "draft.c"), "-o", os.path.join(d, "draft.o")])
    if obj.returncode != 0:
        return (mid, category, "object-failed", reason, obj.stdout[-600:])

    host = run(["g++", "-O0", "-I", d, "-I", BASE, os.path.join(BASE, "host.cpp"),
                os.path.join(d, "draft.o"), "-o", os.path.join(d, "host"), "-lm"])
    if host.returncode != 0:
        return (mid, category, "host-failed", reason, host.stdout[-600:])

    with open(os.path.join(d, "goose.tsv"), "w", encoding="utf-8") as f:
        f.write(run([os.path.join(d, "host")]).stdout)
    gate = run([sys.executable, os.path.join(BASE, "compare.py"),
                os.path.join(d, "goose.tsv")], cwd=BASE)
    lines = [ln for ln in gate.stdout.splitlines()
             if ("mismatch" in ln or ": prompt " in ln or ": detect " in ln
                 or ": label " in ln or ln.startswith("parts:"))]
    caught = gate.returncode != 0
    return (mid, category, "caught" if caught else "MISSED", reason, lines[0] if lines else "")


def main():
    half = 0
    jobs = 1
    args = sys.argv[1:]
    if len(args) >= 2 and args[0] == "--half":
        half = int(args[1])
        args = args[2:]
    if len(args) >= 2 and args[0] == "--jobs":
        jobs = int(args[1])
        args = args[2:]
    os.makedirs(MUT, exist_ok=True)
    shutil.copy(os.path.join(BASE, "goose.tsv"), os.path.join(MUT, "baseline_goose.tsv"))

    half_size = (len(MUTATIONS) + 1) // 2
    batch = MUTATIONS if half == 0 else MUTATIONS[(half - 1) * half_size:half * half_size]
    if jobs > 1:
        with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as ex:
            results = list(ex.map(run_one, batch))
    else:
        results = [run_one(item) for item in batch]
    for mid, category, verdict, reason, first in results:
        print(f"{mid}: {verdict} [{category}] {first[:110]}")
    caught = sum(1 for r in results if r[2] == "caught")
    expected = len(batch)
    print(f"\nMUTATIONS {caught}/{expected} caught")
    suffix = "" if half == 0 else f"-half{half}"
    with open(os.path.join(MUT, f"results{suffix}.md"), "w", encoding="utf-8") as f:
        f.write("| id | category | mutation | verdict | first failure |\n")
        f.write("|---|---|---|---|---|\n")
        for mid, category, verdict, reason, first in results:
            f.write(f"| {mid} | {category} | {reason} | {verdict} | `{first.replace('|', '/')}` |\n")
    if half not in (0, 1, 2):
        print(f"GATE FAILURE: --half must be 0, 1 or 2, got {half}")
        return 2
    if len(batch) == 0:
        print("GATE FAILURE: empty mutation batch")
        return 2
    if len(results) != expected:
        print(f"GATE FAILURE: expected {expected} results, got {len(results)}")
        return 1
    if caught != expected:
        missed = ", ".join(r[0] for r in results if r[2] != "caught")
        print(f"GATE FAILURE: {expected - caught} of {expected} not caught: {missed}")
        return 1
    print(f"GATE OK: {caught}/{expected} caught")
    return 0


if __name__ == "__main__":
    sys.exit(main())
