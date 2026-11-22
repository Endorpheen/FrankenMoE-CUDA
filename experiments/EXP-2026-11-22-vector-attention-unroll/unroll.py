"""Expand only the two row loops of the extracted EXP-1665 vector kernel."""

from pathlib import Path
import argparse


def expand(source: str, factor: int) -> str:
    first = source.index("export fn fv_fattn_vec_1(")
    last = source.index("export fn fv_combine(", first)
    kernel = source[first:last]
    marker = "        for key in 32 {"
    if kernel.count(marker) != 2:
        raise ValueError("expected two 32-key loops")
    for _ in range(2):
        start = kernel.index(marker)
        opening = start + len(marker) - 1
        depth, ending = 1, opening + 1
        while depth:
            depth += (kernel[ending] == "{") - (kernel[ending] == "}")
            ending += 1
        body = kernel[opening + 1:ending - 1].strip("\n ").rstrip().splitlines()
        rows = []
        for row in range(factor):
            index = str(row) if factor == 32 else f"row_group * {factor} + {row}"
            rows.append("        {\n            let key: i64 = " + index + ";\n            " +
                        body[0] + "\n" + "\n".join(body[1:]) + "\n        }")
        replacement = "\n".join(rows)
        if factor != 32:
            replacement = f"        for row_group in {32 // factor} {{\n{replacement}\n        }}"
        kernel = kernel[:start] + replacement + kernel[ending:]
    return source[:first] + kernel + source[last:]


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("factor", type=int, choices=(4, 32))
    args = parser.parse_args()
    args.output.write_text(expand(args.source.read_text(), args.factor))
