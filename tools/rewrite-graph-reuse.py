#!/usr/bin/env python3
"""Move every remaining llm_graph_input_*::can_reuse conjunction to fg_gr_check, in one batch.

Safety is the point. The earlier attempt located function bodies by counting braces and produced a file that
compiled while bodies could have been spliced into the wrong function; the row counts not adding up was the only
symptom. This one:

  * finds a body by its exact signature and the next '\\n}\\n' at column 0 - these are top-level definitions, so
    the closing brace is unambiguous and no nesting is inferred;
  * refuses a function whose `res &=` count differs from what the scan recorded;
  * asserts, after rewriting, that the `res &=` count is unchanged and that exactly one fg_gr_check is present;
  * asserts the file's total `res &=` count is unchanged across the whole batch.

Run with --check to dry-run: it prints the plan and writes nothing.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "runtime/reconstructed/src/llama-graph.cpp"

SIG = re.compile(r"bool (llm_graph_input_[a-z_0-9]+)::(can_reuse[a-z_]*)\([^)]*\)\s*\{")


def bodies(text):
    """Yield (signature, start, end) using top-level closing braces only."""
    for m in SIG.finditer(text):
        end = text.find("\n}\n", m.end())
        if end < 0:
            continue
        yield m.group(0), m.start(), end + 3


def main():
    check_only = "--check" in sys.argv
    text = SRC.read_text()
    total_rows_before = text.count("res &=")
    plan = []

    for sig, start, end in bodies(text):
        block = text[start:end]
        if "fg_gr_check" in block:
            continue
        rows = [r.strip() for r in re.findall(r"res &= (.*?);", block)]
        if not rows:
            continue
        plan.append((sig, start, end, rows))

    print(f"{'function':<48} rows")
    for sig, _, _, rows in plan:
        name = sig.split("(")[0].replace("bool ", "")
        print(f"{name:<48} {len(rows)}")

    if check_only:
        print(f"\nplan only: {len(plan)} function(s), {total_rows_before} 'res &=' in file, nothing written")
        return 0

    for sig, start, end, rows in sorted(plan, key=lambda x: -x[1]):
        block = text[start:end]
        assert block.count("res &= ") == len(rows), sig
        pres, want, have = [], [], []
        for r in rows:
            parts = r.split(" == ")
            if len(parts) == 2:
                pres.append("1"); have.append(parts[0]); want.append(parts[1])
            else:
                pres.append("1"); have.append("((%s) ? 1 : 0)" % r); want.append("1")
        n = len(rows)
        goose = (
            "\n    // The conjunction is fg_gr_check: one row per native conjunction line, a row being\n"
            "    // 'must match, wanted value, actual value'. Sub-decisions such as can_reuse_kq_mask stay native\n"
            "    // and enter as a 1/0 row, so this moves where the conjunction is decided, not what is decided.\n"
            "    // The native body follows, behind GGML_GOOSE_GRAPH_REUSE=0.\n"
            "    if (fg_gr_enabled()) {\n"
            "        const int64_t present[%d] = { %s };\n"
            "        const int64_t want[%d]    = { %s };\n"
            "        const int64_t have[%d]    = { %s };\n"
            "        return fg_gr_check(present, want, have, %d) != 0;\n"
            "    }\n"
        ) % (n, ", ".join(pres), n,
             ", ".join("(int64_t) (%s)" % w for w in want),
             n, ", ".join("(int64_t) (%s)" % h for h in have), n)
        anchor = block.index("res &= ")
        new_block = block[:anchor] + goose + "\n" + block[anchor:]
        assert new_block.count("res &= ") == len(rows), sig
        assert new_block.count("fg_gr_check") == 1, sig
        text = text[:start] + new_block + text[end:]

    assert text.count("res &=") == total_rows_before, "row count changed across the batch"
    SRC.write_text(text)
    print(f"\nrewrote {len(plan)} function(s); 'res &=' count {total_rows_before} -> {text.count('res &=')}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
