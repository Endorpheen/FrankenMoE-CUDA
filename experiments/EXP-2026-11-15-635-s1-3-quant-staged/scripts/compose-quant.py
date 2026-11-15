#!/usr/bin/env python3
"""Compose the P7-04 quant modules into one Goose translation unit.

The seven canonical modules live side by side in goose/model/:

    quant-types, quant-allow, quant-rows, quant-rows2,
    quant-dequant, quant-dequant2, quant-weighted

The input directory is normally goose/model-staged: the canon files copied
verbatim from goose/model plus the staged fixes in patches/. The script itself
never edits a module.

They were written as independent units, so a few top-level names repeat:
`main` (dropped), the ggml type ids and geometry constants (identical values,
declared once), and a set of module-internal helpers (same name, different
body or different callers).  This script concatenates the modules in a fixed
order and resolves the collisions deterministically:

  * a duplicate `const` keeps the first declaration, and the script fails if
    the two declarations differ;
  * a duplicate `fn` whose body is byte-identical keeps the first definition;
  * a duplicate `fn` listed in RENAMES is prefixed in the later module only,
    definition and call sites together;
  * any other duplicate `fn` is an error;
  * `fn main()` is dropped: a library unit has no entry point.

The result is deterministic: same inputs, same bytes, same SHA-256.
"""

import hashlib
import re
import sys
from pathlib import Path

MODULES = [
    "quant-types",
    "quant-allow",
    "quant-rows",
    "quant-rows2",
    "quant-dequant",
    "quant-dequant2",
    "quant-weighted",
]

# Module -> {name in the module: unique name in the composed unit}.
RENAMES = {
    # Host primitives (extern declarations) are bound to a C symbol per module,
    # so the module-visible names collide even when the C names differ. The
    # later module keeps its binding and gets a suffix.
    "quant-rows2": {
        "f32_bits": "f32_bits_2",
        "f32_of_bits": "f32_of_bits_2",
        "fabs32": "fabs32_2",
        "fail": "fail_2",
        "floor32": "floor32_2",
        "iq2_grid": "iq2_grid_2",
        "iq2_map": "iq2_map_2",
        "iq2_nb": "iq2_nb_2",
        "iq3_grid": "iq3_grid_2",
        "iq3_map": "iq3_map_2",
        "iq3_nb": "iq3_nb_2",
        "ld_f16": "ld_f16_2",
        "ld_f32": "ld_f32_2",
        "ld_u8": "ld_u8_2",
        "ldexp32": "ldexp32_2",
        "log232": "log232_2",
        "lround32": "lround32_2",
        "nearest": "nearest_2",
        "sort16": "sort16_2",
        "sqrt32": "sqrt32_2",
        "st_f16": "st_f16_2",
        "st_f32": "st_f32_2",
        "st_u16": "st_u16_2",
        "st_u32": "st_u32_2",
        "st_u8": "st_u8_2",
        "tmp": "tmp_2",
    },
    "quant-dequant2": {
        "scale_min_k4": "qd_scale_min_k4",
        "e8m0_half": "qd_e8m0_half",
        "iq2_grid": "iq2_grid_d",
        "iq3_grid": "iq3_grid_d",
    },
    "quant-weighted": {
        # helpers copied from quant-rows2 with a weighted variant
        "sa": "qw_sa",
        "sgetf": "qw_sgetf",
        "ssetf": "qw_ssetf",
        "sgetb": "qw_sgetb",
        "ssetb": "qw_ssetb",
        "imin": "qw_imin",
        "imax": "qw_imax",
        "fmax32": "qw_fmax32",
        "i8val": "qw_i8val",
        "f2i32": "qw_f2i32",
        "u8of": "qw_u8of",
        "scale_min_k4": "qw_scale_min_k4",
        "make_qkx2": "qw_make_qkx2",
        "make_q3": "qw_make_q3",
        "make_qx": "qw_make_qx",
        "kf4": "qw_kf4",
        "e8m0_half": "qw_e8m0_half",
        "best_index_mxfp4": "qw_best_index_mxfp4",
        "ue4m3_from_f32": "qw_ue4m3_from_f32",
        "ue4m3_to_f32": "qw_ue4m3_to_f32",
        "iq4nl_val": "qw_iq4nl_val",
        "best_index_int8": "qw_best_index_int8",
        "iq4_nl_block": "qw_iq4_nl_block",
        "iq3_best_nb": "qw_iq3_best_nb",
        "iq2_best_nb": "qw_iq2_best_nb",
        "iq1_best_nb2": "qw_iq1_best_nb2",
        "ld_u32v": "qw_ld_u32v",
        # rows dispatcher with the imatrix arm
        "qz_rows": "qw_rows",
        # host primitives rebound to the qz_* C symbols
        "f32_bits": "f32_bits_w",
        "f32_of_bits": "f32_of_bits_w",
        "fabs32": "fabs32_w",
        "fail": "fail_w",
        "floor32": "floor32_w",
        "iq2_grid": "iq2_grid_w",
        "iq2_map": "iq2_map_w",
        "iq2_nb": "iq2_nb_w",
        "iq3_grid": "iq3_grid_w",
        "iq3_map": "iq3_map_w",
        "iq3_nb": "iq3_nb_w",
        "ld_f16": "ld_f16_w",
        "ld_f32": "ld_f32_w",
        "ld_u8": "ld_u8_w",
        "ldexp32": "ldexp32_w",
        "log232": "log232_w",
        "lround32": "lround32_w",
        "nearest": "nearest_w",
        "sort16": "sort16_w",
        "sqrt32": "sqrt32_w",
        "st_f16": "st_f16_w",
        "st_f32": "st_f32_w",
        "st_u16": "st_u16_w",
        "st_u32": "st_u32_w",
        "st_u8": "st_u8_w",
        "tmp": "tmp_w",
    },
}

FN_RE = re.compile(r"^(?:export )?fn ([A-Za-z0-9_]+)[^{]*\{", re.M)
CONST_RE = re.compile(r"^const ([A-Za-z0-9_]+):[^;]*;", re.M)


def block_end(text, start):
    """Index just past the body that starts at the `{` at `start`."""
    depth = 0
    i = start
    while i < len(text):
        c = text[i]
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return i + 1
        i += 1
    raise ValueError("unbalanced braces")


def split_functions(text):
    """[(name, start, end)] for every top-level fn, including `main`."""
    out = []
    for m in FN_RE.finditer(text):
        start = m.start()
        end = block_end(text, m.end() - 1)
        out.append((m.group(1), start, end))
    return out


def apply_renames(text, table):
    for old, new in table.items():
        text = re.sub(r"\b%s\b" % re.escape(old), new, text)
    return text


def main():
    model = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("goose/model-staged")
    out_path = Path(sys.argv[2]) if len(sys.argv) > 2 else Path("goose-gen/quant-stack.goose")
    out_path.parent.mkdir(parents=True, exist_ok=True)

    chunks = ["// Composed P7-04 quant stack: one Goose translation unit.\n"
              "// Generated by scripts/compose-quant.py from goose/model/*.goose; do not edit.\n"
              "import std;\n"]
    seen_const = {}   # name -> (module, declaration text)
    seen_fn = {}      # name -> (module, body)

    for mod in MODULES:
        src = (model / (mod + ".goose")).read_text()
        src = apply_renames(src, RENAMES.get(mod, {}))

        parts = []
        cursor = 0
        for name, start, end in split_functions(src):
            parts.append(src[cursor:start])
            body = src[start:end]
            if name == "main":
                # drop the entry point, keep the blank lines around it
                cursor = end
                continue
            if name in seen_fn:
                other_mod, other_body = seen_fn[name]
                if other_body.strip() != body.strip():
                    raise SystemExit(
                        "duplicate fn %r differs between %s and %s; add a rename"
                        % (name, other_mod, mod))
                parts.append("// %s: identical copy of %s dropped\n" % (name, other_mod))
                cursor = end
                continue
            seen_fn[name] = (mod, body)
            parts.append(body)
            cursor = end
        parts.append(src[cursor:])
        text = "".join(parts)

        # constants: keep the first declaration per name, require equal text
        def keep(m):
            name, whole = m.group(1), m.group(0)
            if name in seen_const:
                other_mod, other = seen_const[name]
                if other != whole:
                    raise SystemExit("const %r differs between %s and %s" % (name, other_mod, mod))
                return "// const %s: identical copy of %s dropped\n" % (name, other_mod)
            seen_const[name] = (mod, whole)
            return whole

        text = CONST_RE.sub(keep, text)

        chunks.append("\n// ---- composed from goose/model/%s.goose ----\n" % mod)
        chunks.append(text)

    composed = "".join(chunks)
    out_path.write_text(composed)

    digest = hashlib.sha256(composed.encode()).hexdigest()
    print("composed %d modules -> %s" % (len(MODULES), out_path))
    print("bytes=%d lines=%d" % (len(composed), composed.count("\n")))
    print("fn declarations=%d const declarations=%d"
          % (len(FN_RE.findall(composed)), len(CONST_RE.findall(composed))))
    print("sha256=%s" % digest)
    for mod in MODULES:
        p = model / (mod + ".goose")
        print("input %s %s" % (hashlib.sha256(p.read_bytes()).hexdigest(), p))


if __name__ == "__main__":
    main()
