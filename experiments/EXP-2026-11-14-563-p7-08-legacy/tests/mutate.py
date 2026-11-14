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


def main():
    half = 0
    if len(sys.argv) > 2 and sys.argv[1] == "--half":
        half = int(sys.argv[2])
    os.makedirs(MUT, exist_ok=True)
    shutil.copy(os.path.join(BASE, "goose.tsv"), os.path.join(MUT, "baseline_goose.tsv"))
    base = open(DRAFT, encoding="utf-8").read()
    results = []

    half_size = (len(MUTATIONS) + 1) // 2
    batch = MUTATIONS if half == 0 else MUTATIONS[(half - 1) * half_size:half * half_size]
    for mid, category, reason, old, new in batch:
        count = base.count(old)
        if count < 1:
            print(f"{mid}: SOURCE NOT FOUND ({category})")
            results.append((mid, category, "not-applied", "", ""))
            continue
        src = base.replace(old, new)
        d = os.path.join(MUT, mid)
        os.makedirs(d, exist_ok=True)
        src_path = os.path.join(d, "draft.goose")
        shutil.copyfile(DRAFT, src_path)
        open(src_path, "w", encoding="utf-8").write(src)

        build = run([GOOSE, "-o", os.path.join(d, "draft.c"), "--standalone",
                     "--header", os.path.join(d, "draft.h"), src_path])
        if build.returncode != 0:
            print(f"{mid}: BUILD FAILED (syntax mutation, rejected)\n{build.stdout[-600:]}")
            results.append((mid, category, "build-failed", "", ""))
            continue

        obj = run(["gcc", "-O0", "-c", os.path.join(d, "draft.c"), "-o", os.path.join(d, "draft.o")])
        if obj.returncode != 0:
            print(f"{mid}: OBJECT BUILD FAILED\n{obj.stdout[-600:]}")
            results.append((mid, category, "object-failed", "", ""))
            continue
        host = run(["g++", "-O0", "-I", d, "-I", BASE, os.path.join(BASE, "host.cpp"),
                    os.path.join(d, "draft.o"), "-o", os.path.join(d, "host"), "-lm"])
        if host.returncode != 0:
            print(f"{mid}: HOST BUILD FAILED\n{host.stdout[-600:]}")
            results.append((mid, category, "host-failed", "", ""))
            continue

        with open(os.path.join(d, "goose.tsv"), "w", encoding="utf-8") as f:
            f.write(run([os.path.join(d, "host")]).stdout)
        shutil.copy(os.path.join(d, "goose.tsv"), os.path.join(BASE, "goose.tsv"))
        gate = run([sys.executable, os.path.join(BASE, "compare.py")], cwd=BASE)
        shutil.copy(os.path.join(MUT, "baseline_goose.tsv"), os.path.join(BASE, "goose.tsv"))

        lines = [ln for ln in gate.stdout.splitlines()
                 if ("mismatch" in ln or ": prompt " in ln or ": detect " in ln
                     or ": label " in ln or ln.startswith("parts:"))]
        caught = gate.returncode != 0
        first = lines[0] if lines else ""
        results.append((mid, category, "caught" if caught else "MISSED", reason, first))
        print(f"{mid}: {'caught' if caught else 'MISSED'} [{category}] {first[:110]}")

    shutil.copy(os.path.join(MUT, "baseline_goose.tsv"), os.path.join(BASE, "goose.tsv"))
    caught = sum(1 for r in results if r[2] == "caught")
    print(f"\nMUTATIONS {caught}/{len(batch)} caught")
    suffix = "" if half == 0 else f"-half{half}"
    with open(os.path.join(MUT, f"results{suffix}.md"), "w", encoding="utf-8") as f:
        f.write("| id | category | mutation | verdict | first failure |\n")
        f.write("|---|---|---|---|---|\n")
        for mid, category, verdict, reason, first in results:
            f.write(f"| {mid} | {category} | {reason} | {verdict} | `{first.replace('|', '/')}` |\n")
    return 0 if caught == len(batch) else 1


if __name__ == "__main__":
    sys.exit(main())
