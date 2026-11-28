#!/usr/bin/env python3
"""Refresh public optimization tables and charts from the GooseInfer registry."""

from __future__ import annotations

import argparse
import io
import json
import re
import subprocess
import sys
import zipfile
from collections import defaultdict
from html import escape
from pathlib import Path
from xml.etree import ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "docs/optimizations"
REGISTRY = ROOT / "docs/goose-migration/FRIED-GOOSE-OPTIMIZATION-REGISTRY.md"
WORKBOOK = OUT / "optimization-matrix.xlsx"
NS = "{http://schemas.openxmlformats.org/spreadsheetml/2006/main}"
XML_SPACE = "{http://www.w3.org/XML/1998/namespace}space"
START = "<!-- optimization-summary:start -->"
END = "<!-- optimization-summary:end -->"
PREFIX = "integration/goose-standalone/"
FAMILIES = (
    ("Attention and KV", r"flash|attention|attn|(?:^|_)kv(?:_|$)"),
    ("Gated DeltaNet and SSM", r"gdn|ssm|conv|recur|decay"),
    ("MoE and routing", r"moe|route|topk|top_k|expert"),
    ("Normalization and softmax", r"norm|softmax|sink|reduce"),
    ("Matrix products", r"matrix|matmul|mm[qv]|gemm|project|gate_up|dense_|^iq[234]|^q[468]|^pq2"),
    ("Packing and gather", r"pack|unpack|gather|embed|quant|rows"),
    ("RoPE and transforms", r"rope|fwht|transform"),
    ("Epilogues and pointwise", r"epilogue|situ|gate|sigmoid|swiglu|add|bias|vector|residual|tanh|scale"),
)


def read(path):
    return (ROOT / path).read_text(encoding="utf-8")


def table(headers, rows):
    def line(cells):
        return "| " + " | ".join(str(cell).replace("|", r"\|") for cell in cells) + " |"

    return "\n".join([line(headers), line(["---"] * len(headers)), *(line(r) for r in rows)])


def registry_methods(coverage):
    text = REGISTRY.read_text(encoding="utf-8")
    compiler, runtime = text.split("## Accepted GooseInfer runtime optimizations", 1)
    accepted = []
    for line in runtime.splitlines():
        match = re.fullmatch(r"\| (.*?) \| (.*?) \| (.*?) \| \[(EXP-\d+)\]\(\.\./\.\./([^)]+)\) \|", line)
        if match:
            title, scope, result, code, evidence = match.groups()
            accepted.append((code, title, scope, result, evidence))
    experimental = []
    for line in compiler.splitlines():
        match = re.fullmatch(r"\| (.*?) \| Candidate \| \[([^]]+)\]\(([^)]+)\) \|", line)
        if match:
            title, citation, uri = match.groups()
            code = citation.split()[0] if citation.startswith("EXP-") else "Apex sampler"
            evidence = str(REGISTRY.relative_to(ROOT).parent / uri) if not uri.startswith("#") else str(REGISTRY.relative_to(ROOT)) + uri
            experimental.append((code, title, "Compiler", "Evaluation pending.", evidence))
    if not accepted or not experimental:
        raise ValueError("Missing accepted or experimental rows in the optimization registry")
    records = []
    for status, entries in (("Confirmed optimization", accepted), ("Experimental", experimental)):
        for code, name, scope, result, source in entries:
            rule = coverage["methods"].get(code, {})
            records.append((code, name, status, scope, rule.get("condition", ""), source))
    for row in coverage["potential"]:
        records.append((row["id"], row["name"], "Potential", "To be evaluated", row["condition"], "coverage.json"))
    return accepted, experimental, records


def inventory():
    rows = {}

    def add(backend, name, model, source, kind):
        key = backend, model, name
        if key in rows and kind == rows[key][2]:
            sources = rows[key][1].split(" · ")
            if source not in sources:
                rows[key] = rows[key][0], " · ".join([*sources, source]), kind
            return
        if key not in rows:
            family = next((label for label, rule in FAMILIES if re.search(rule, name, re.I)), "Review source")
            rows[key] = family, source, kind

    manifests = subprocess.check_output(
        ["git", "ls-files", "-z", PREFIX + "*-entries.json"], cwd=ROOT, text=True
    ).rstrip("\0").split("\0")
    if not manifests or not manifests[0]:
        raise ValueError("No CUDA entry manifests found")
    for path in manifests:
        if "/cuda/images/" in path:
            model = "Inkling 2 Mini"
        elif "/gigachat4/" in path:
            model = "GigaChat4"
        elif "apex-" in path:
            model = "Apex 35B"
        elif "-bonsai-" in path:
            model = "Ternary Bonsai"
        else:
            model = "Shared"
        for entry in json.loads(read(path)):
            add("CUDA", entry["name"], model, path, "CUDA manifest")

    bonsai = PREFIX + "universal/bonsai/image-host.goose"
    for mode, names in re.findall(r'fn bonsai_(pipeline|mtp)_kernel_names\(\).*?vocab_text\("([^"]+)"\)', read(bonsai), re.S):
        for name in filter(None, names.split(r"\0")):
            add("CUDA", name, "Ternary Bonsai", bonsai, f"Loader table ({mode})")

    driver = PREFIX + "universal/cuda/driver.goose"
    names = {name for backend, _, name in rows if backend == "CUDA"}
    for name in sorted(set(re.findall(r'"((?:goose|giga|i2)_[a-z][a-zA-Z0-9_]+)"', read(driver))) - names):
        add("CUDA", name, "Review dispatch", driver, "Driver string")

    vulkan = "tools/build-goose-vulkan.sh"
    shaders = re.search(r"for shader in (.*?)\s*; do", read(vulkan))
    if not shaders:
        raise ValueError("Vulkan shader list was not found")
    for name in shaders.group(1).split():
        add("Vulkan", name, "Shared", vulkan, "SPIR-V build recipe")

    cpu = PREFIX + "universal/cpu/cpu.goose"
    sections = re.split(r"(?m)^(?:export )?fn (\w+)\(", read(cpu))
    bodies = dict(zip(sections[1::2], sections[2::2]))
    for dispatcher in ("entry", "state_entry", "fg143_node_run", "graph_regular"):
        calls = re.findall(r"\breturn\s+([a-z]\w*)\s*\(", bodies[dispatcher])
        for name in calls:
            if name in bodies:
                add("CPU", name, "Shared", cpu, "Call from a compute dispatcher")
    numeric = PREFIX + "universal/inkling/numeric.goose"
    for name in re.findall(r"(?m)^export fn (inkling_row_major_i2_\w+|i2_depth_mix_cached)\(", read(numeric)):
        add("CPU", name, "Inkling 2 Mini", numeric, "Exported compute function")
    host = PREFIX + "universal/inkling/serial-host.goose"
    for backend, name, path in (
        ("CPU", "i2_embed_row", PREFIX + "universal/inkling/model.goose"),
        ("CPU", "fg_i2_embed_prefill", host),
        ("CPU", "fg_i2_prefill_partition", host),
        ("CUDA", "i2_batch_verifier_logits", host),
        ("CUDA", "launch_expert_overlap", driver),
    ):
        if not re.search(r"\bfn " + name + r"\(", read(path)):
            raise ValueError(f"Compute path {name} missing from {path}")
        add(backend, name, "Inkling 2 Mini", path, "Explicit orchestration path")
    return [(backend, name, model, *value) for (backend, model, name), value in sorted(rows.items())]


def combinations(kernels, methods, coverage):
    rules = {**coverage["methods"], **{row["id"]: row for row in coverage["potential"]}}
    current_symbols = {name for _, name, model, *_ in kernels if model == "Inkling 2 Mini"}
    for code, rule in rules.items():
        missing = set(rule.get("verified", ())) - current_symbols
        if missing:
            raise ValueError(f"Verified paths for {code} are missing from inventory: {', '.join(sorted(missing))}")
    result = []
    for backend, kernel, model, family, _, _ in kernels:
        for code, _, _, _, _, uri in methods:
            rule = rules.get(code, {})
            if model == "Inkling 2 Mini" and kernel in rule.get("verified", ()):
                status, evidence = "Confirmed", uri
            elif rule.get("pattern") and re.search(rule["pattern"], kernel, re.I):
                status, evidence = "Candidate for review", ""
            else:
                status, evidence = "Not assessed", ""
            result.append((status, code, kernel, backend, model, family, evidence))
    priority = {"Confirmed": 0, "Candidate for review": 1, "Not assessed": 2}
    return sorted(result, key=lambda x: (priority[x[0]], x[2], x[3], x[4], x[1]))


def local_link(path):
    return "../../" + path if path != "coverage.json" else path


def accepted_table(accepted, home=False):
    def link(code, source):
        return f"[{code}]({source if home else local_link(source)})"

    return table(("Optimization", "Verified scope", "Measured result", "Evidence"),
                 ((title, scope, result, link(code, source)) for code, title, scope, result, source in accepted))


def matrix_doc(kernels, pairs, methods):
    checked = defaultdict(list)
    prospects = defaultdict(list)
    for status, code, kernel, backend, model, _, evidence in pairs:
        key = backend, model, kernel
        if status == "Confirmed":
            checked[key].append(f"[{code}]({local_link(evidence)})")
        elif status == "Candidate for review":
            prospects[key].append(code)
    sections = defaultdict(list)
    for backend, kernel, model, family, source, scheme in kernels:
        key = backend, model, kernel
        sources = " · ".join(f"[{Path(path).name}]({local_link(path)})" for path in source.split(" · "))
        sections[family].append((backend, model, f"`{kernel}`", ", ".join(checked[key]) or "—",
                                 ", ".join(prospects[key]) or "—", sources))
    intro = (
        "# Optimization applicability matrix\n\n"
        f"The catalog contains {len(kernels)} static entries and {len(methods)} methods "
        f"({len(pairs):,} combinations). [Download the filterable workbook](optimization-matrix.xlsx).\n\n"
        "**Confirmed** links to a measured experiment for the specified shape. "
        "**Candidates** are name-based prompts for source review and measurement. "
        "Combinations absent from both columns remain unassessed.\n\n"
        "The catalog observes build and dispatch declarations. A static entry needs a runtime trace "
        "to establish actual use, geometry and model coverage. Shared symbols can serve multiple models.\n"
    )
    return intro + "".join(
        f"\n## {family}\n\n" + table(("Backend", "Model family", "Entry", "Confirmed", "Candidates", "Source"), rows) + "\n"
        for family, rows in sorted(sections.items())
    )


def checkpoint_means(accepted):
    points = []
    for code, _, scope, _, path in accepted:
        if not scope.startswith("Inkling"):
            continue
        card = read(path)
        match = re.search(r"(?m)^\| (?:Candidate|Split-half) mean \| (\d+(?:\.\d+)?) \| (\d+(?:\.\d+)?) \|", card)
        if match:
            points.append((code, float(match[1]), float(match[2]), path))
    return points[-7:]


def chart(points):
    if len(points) < 2:
        raise ValueError("Need at least two accepted Inkling mean rows for the trend")
    width, height = 850, 300
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}" role="img" aria-labelledby="title desc">',
           '<title id="title">Inkling 2 Mini accepted checkpoints</title>',
           '<desc id="desc">Warm single-request prefill and decode on RTX 4070, 32K prompt and 512 output tokens.</desc>',
           '<rect width="850" height="300" rx="12" fill="#f6f8fb"/>',
           '<g font-family="system-ui, sans-serif"><text x="24" y="33" font-size="18" font-weight="700" fill="#172b40">Inkling 2 Mini on RTX 4070</text>',
           '<text x="24" y="54" font-size="12" fill="#4b5d70">32K/512 · accepted development checkpoints · tok/s</text>']
    for panel, (label, index, color) in enumerate((("Prefill", 1, "#196455"), ("Decode", 2, "#244b85"))):
        left = 62 + 423 * panel
        top, plot_width, plot_height = 99, 320, 148
        values = [p[index] for p in points]
        step = 50 if index == 1 else 5
        low = int(min(values) // step) * step
        high = (int(max(values) // step) + 1) * step
        if high - low < step * 2:
            low -= step
        coords = []
        out.append(f'<text x="{left}" y="82" fill="{color}" font-size="13" font-weight="700">{label}</text>')
        for tick in range(low, high + 1, step):
            y = top + plot_height * (high - tick) / (high - low)
            out.append(f'<line x1="{left}" y1="{y:.1f}" x2="{left+plot_width}" y2="{y:.1f}" stroke="#cfd9e3"/>')
            out.append(f'<text x="{left-7}" y="{y+3:.1f}" text-anchor="end" font-size="10" fill="#4b5d70">{tick}</text>')
        for j, point in enumerate(points):
            x = left + plot_width * j / (len(points)-1)
            y = top + plot_height * (high - point[index]) / (high - low)
            coords.append((x, y))
            out.append(f'<text x="{x:.1f}" y="266" text-anchor="middle" font-size="10" fill="#4b5d70">{escape(point[0][4:])}</text>')
        data = " ".join(f"{x:.1f},{y:.1f}" for x, y in coords)
        out.append(f'<polyline points="{data}" fill="none" stroke="{color}" stroke-width="2"/>')
        for (x, y), value in zip(coords, values):
            out.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="3" fill="{color}"><title>{value:.2f} tok/s</title></circle>')
    out.append("</g></svg>\n")
    return "\n".join(out)


def workbook_data(coverage, kernels, methods, pairs):
    return [
        ("Overview", ["Subsystem", "Possible transfer", "Status", "Evidence or review condition"], coverage["directions"]),
        ("Matrix", ["Status", "Method", "Kernel or function", "Backend", "Model family", "Subsystem", "Evidence"], pairs),
        ("Kernels", ["Backend", "Kernel or function", "Model family", "Subsystem", "Repository source", "Inventory basis"], kernels),
        ("Methods", ["Code", "Method", "Status", "Scope", "Review condition", "Evidence"], methods),
    ]


def column(index):
    name = ""
    while index:
        index, digit = divmod(index - 1, 26)
        name = chr(65 + digit) + name
    return name


def column_number(address):
    name = re.match(r"[A-Z]+", address).group()
    index = 0
    for letter in name:
        index = index * 26 + ord(letter) - 64
    return index


def sheet_data(sheet, strings, size):
    content = sheet.find(NS + "sheetData")
    rows = []
    for row in content.findall(NS + "row"):
        if int(row.get("r")) < 6:
            continue
        record = [""] * size
        for cell in row.findall(NS + "c"):
            kind = cell.get("t")
            value = cell.findtext(NS + "v", default="")
            if kind == "s":
                value = strings[int(value)]
            elif kind == "inlineStr":
                value = "".join(t.text or "" for t in cell.iter(NS + "t"))
            record[column_number(cell.get("r")) - 1] = value
        rows.append(tuple(record))
    return rows


def replace_sheet(sheet, table_xml, values):
    content = sheet.find(NS + "sheetData")
    rows = content.findall(NS + "row")
    style = {}
    for row in rows:
        number = int(row.get("r"))
        if number >= 6:
            key = min(7, number)
            if key not in style:
                style[key] = ({k: v for k, v in row.attrib.items() if k != "r"},
                              {column_number(cell.get("r")): cell.get("s") for cell in row})
            content.remove(row)
    for number, record in enumerate(values, start=6):
        row_style, cell_styles = style.get(min(7, number), ({}, {}))
        row = ET.SubElement(content, NS + "row", {**row_style, "r": str(number)})
        for j, value in enumerate(record, start=1):
            if not value:
                continue
            cell = ET.SubElement(row, NS + "c", {"r": column(j) + str(number), "t": "inlineStr"})
            if cell_styles.get(j) is not None:
                cell.set("s", cell_styles[j])
            word = ET.SubElement(ET.SubElement(cell, NS + "is"), NS + "t")
            word.text = value
            if value != value.strip():
                word.set(XML_SPACE, "preserve")
    end = column(len(values[0])) + str(len(values) + 5)
    dimension = sheet.find(NS + "dimension")
    if dimension is not None:
        dimension.set("ref", "A1:" + end)
    ref = "A6:" + end
    table_xml.set("ref", ref)
    table_xml.find(NS + "autoFilter").set("ref", ref)


def update_workbook(specs, check):
    if not WORKBOOK.is_file():
        print(f"Missing workbook layout: {WORKBOOK.relative_to(ROOT)}", file=sys.stderr)
        return False
    changes = {}
    with zipfile.ZipFile(WORKBOOK) as old:
        tabs = ET.fromstring(old.read("xl/workbook.xml")).findall(NS + "sheets/" + NS + "sheet")
        if [tab.get("name") for tab in tabs] != [name for name, _, _ in specs]:
            raise ValueError("Workbook sheet names differ from the published layout")
        strings = []
        if "xl/sharedStrings.xml" in old.namelist():
            strings = ["".join(t.text or "" for t in item.iter(NS + "t"))
                       for item in ET.fromstring(old.read("xl/sharedStrings.xml"))]
        for index, (_, headers, data) in enumerate(specs, start=1):
            values = [tuple(headers), *(tuple(r) for r in data)]
            sheet_path = f"xl/worksheets/sheet{index}.xml"
            table_path = f"xl/tables/table{index}.xml"
            sheet = ET.fromstring(old.read(sheet_path))
            table_xml = ET.fromstring(old.read(table_path))
            table_range = f"A6:{column(len(headers))}{len(values) + 5}"
            if sheet_data(sheet, strings, len(headers)) != values or table_xml.get("ref") != table_range:
                if check:
                    return False
                replace_sheet(sheet, table_xml, values)
                changes[sheet_path] = ET.tostring(sheet, encoding="utf-8", xml_declaration=True)
                changes[table_path] = ET.tostring(table_xml, encoding="utf-8", xml_declaration=True)
        if not changes:
            return True
        output = io.BytesIO()
        with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED) as new:
            for part in old.infolist():
                new.writestr(part, changes.get(part.filename, old.read(part)))
    WORKBOOK.write_bytes(output.getvalue())
    return True


def root_summary(text, accepted):
    if START not in text or END not in text:
        raise ValueError("Add the optimization-summary marker block to README.md")
    before, tail = text.split(START, 1)
    _, after = tail.split(END, 1)
    summary = "\n\n" + accepted_table(accepted[-6:], home=True) + "\n\n"
    summary += "See the [optimization documentation](docs/optimizations/README.md) for the full registry, applicability matrix and experiment trends.\n\n"
    return before + START + summary + END + after


def public_readme(accepted, experimental, coverage, points):
    return (
        "# GooseInfer optimizations\n\n"
        "[Applicability matrix on GitHub](MATRIX.md) · [Download Excel](optimization-matrix.xlsx) · "
        "[Engineering registry](../goose-migration/FRIED-GOOSE-OPTIMIZATION-REGISTRY.md)\n\n"
        "## Confirmed optimizations\n\n" + accepted_table(accepted) + "\n\n"
        "## Experimental candidates\n\n" +
        table(("Candidate", "Status", "Evidence"),
              ((name, "Experimental", f"[{code}]({local_link(source)})") for code, name, _, _, source in experimental)) +
        "\n\n## Potential directions\n\n" +
        table(("Subsystem", "Possible transfer", "Status", "Evidence or review condition"),
              (row for row in coverage["directions"] if row[2] == "Potential")) +
        "\n\n## Inkling checkpoint trend\n\n"
        "Single-response development measurements on RTX 4070 with Inkling 2 Mini AD-IQ4_XS, "
        "Q4_0 KV and a 32K/512 workload. Each point is the published candidate mean from its own experiment.\n\n"
        "![Inkling prefill and decode throughput by accepted checkpoint](inkling-checkpoints.svg)\n\n" +
        table(("Checkpoint", "Prefill, tok/s", "Decode, tok/s"),
              ((f"[{code}]({local_link(source)})", f"{prefill:.2f}", f"{decode:.2f}") for code, prefill, decode, source in points)) +
        "\n\n## Refresh the documentation\n\n"
        "After updating the engineering registry, experiment cards or kernel declarations, run "
        "`python3 tools/update-optimization-docs.py` from the repository root. "
        "The command refreshes this page, the GitHub matrix, the root README summary, the chart and the data in the Excel layout.\n\n"
        "Use `coverage.json` to record verified entry names and review conditions. "
        "New registry methods enter the matrix even when their kernel coverage is still unassessed. "
        "CI runs `python3 tools/update-optimization-docs.py --check` to detect stale public outputs. "
        "The generator reads only the public registry, public experiment cards and declared source paths.\n"
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--check", action="store_true", help="Check that generated outputs match their sources.")
    mode.add_argument("--workbook-data", action="store_true", help="Print data for authoring the workbook layout.")
    args = parser.parse_args()
    coverage = json.loads((OUT / "coverage.json").read_text(encoding="utf-8"))
    accepted, experimental, methods = registry_methods(coverage)
    kernels = inventory()
    pairs = combinations(kernels, methods, coverage)
    specs = workbook_data(coverage, kernels, methods, pairs)
    if args.workbook_data:
        print(json.dumps(specs))
        return 0
    points = checkpoint_means(accepted)
    root_readme = ROOT / "README.md"
    outputs = {
        root_readme: root_summary(root_readme.read_text(encoding="utf-8"), accepted),
        OUT / "README.md": public_readme(accepted, experimental, coverage, points),
        OUT / "MATRIX.md": matrix_doc(kernels, pairs, methods),
        OUT / "inkling-checkpoints.svg": chart(points),
    }
    stale = []
    for path, content in outputs.items():
        if path.is_file() and path.read_text(encoding="utf-8") == content:
            continue
        if args.check:
            stale.append(path.relative_to(ROOT))
        else:
            path.write_text(content, encoding="utf-8")
    if not update_workbook(specs, args.check):
        stale.append(WORKBOOK.relative_to(ROOT))
    if stale:
        print("Refresh with python3 tools/update-optimization-docs.py:", file=sys.stderr)
        print(*stale, sep="\n", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
