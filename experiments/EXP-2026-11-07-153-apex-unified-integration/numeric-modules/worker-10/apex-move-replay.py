#!/usr/bin/env python3
"""Hash-bound, default-on source patch for the worker-10 Apex move block.

Patches only copies below worker-10/patched.  Each edit is bound to the full
source sha256 and to one exact function signature.  Rollback mode reproduces
the original file byte-for-byte (the runtime FG153_APEX_MOVE=0 gate is the
second rollback layer).
"""
from __future__ import annotations
import argparse, hashlib, json, sys
from pathlib import Path

ANCHORS = {
    "cpy.cu": {
        "sha256": "d0bee0ec1fb79f6fdbe1d39719c2a595e0bd6f51bbaea6536284848f80637341",
        "signature": "void ggml_cuda_cpy(ggml_backend_cuda_context & ctx, const ggml_tensor * src0, ggml_tensor * src1) {",
        "hook": "    if (fg153_apex_move_enabled() && fg153_apex_move_cpy(ctx, src0, src1)) {\n        return;\n    }",
        "count": "fg153_apex_move_cpy(",
    },
    "getrows.cu": {
        "sha256": "3691d71610908bd1bbe38729ee143649a99da349ab3077a5cedd155137767adb",
        "signature": "void ggml_cuda_op_get_rows(ggml_backend_cuda_context & ctx, ggml_tensor * dst) {",
        "hook": "    if (fg153_apex_move_enabled() && fg153_apex_move_get_rows(ctx, dst)) {\n        return;\n    }",
        "count": "fg153_apex_move_get_rows(",
    },
    "set-rows.cu": {
        "sha256": "24654fe55234c12b0d4d1e9c78871509fc5348f7e3ff123e146838c861a8c8a9",
        "signature": "void ggml_cuda_op_set_rows(ggml_backend_cuda_context & ctx, ggml_tensor * dst) {",
        "hook": "    if (fg153_apex_move_enabled() && fg153_apex_move_set_rows(ctx, dst)) {\n        return;\n    }",
        "count": "fg153_apex_move_set_rows(",
    },
    "convert.cu": {
        "sha256": "4984ec7d0a7d0a054dc7d1189d0ef01482c3ac2346eb31d945d53357b62b78fc",
        "signature": "to_fp32_cuda_t ggml_get_to_fp32_cuda(ggml_type type) {",
        "hook": "    if (fg153_apex_move_enabled() && type == GGML_TYPE_Q4_0) {\n        return fg153_apex_move_to_fp32_q4_0;\n    }",
        "count": "fg153_apex_move_to_fp32_q4_0",
    },
}
INCLUDE = '#include "apex-move-cuda.h"\n'


def sha(text: str) -> str:
    return hashlib.sha256(text.encode()).hexdigest()


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("--input-dir", default="/inputs/native/ggml/src/ggml-cuda")
    ap.add_argument("--out-dir", default="patched")
    ap.add_argument("--rollback", action="store_true")
    ap.add_argument("--report", default="hook-report.json")
    args = ap.parse_args(argv)
    src_dir = Path(args.input_dir)
    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    report = {"status": "OK", "default_on": not args.rollback, "rollback_gate": "FG153_APEX_MOVE=0", "files": {}}
    for name, a in ANCHORS.items():
        source = (src_dir / name).read_text(encoding="utf-8")
        digest = sha(source)
        if digest != a["sha256"]:
            print(f"{name}: source hash mismatch: {digest}", file=sys.stderr)
            return 3
        if source.count(a["signature"]) != 1:
            print(f"{name}: signature is not unique", file=sys.stderr)
            return 3
        if args.rollback:
            out = source
            inserted = 0
        else:
            if INCLUDE in source:
                print(f"{name}: already patched", file=sys.stderr)
                return 3
            if a["count"] in source:
                print(f"{name}: hook symbol already present", file=sys.stderr)
                return 3
            out = source.replace(a["signature"], INCLUDE + a["signature"] + "\n" + a["hook"], 1)
            if out.count(a["count"]) != 1:
                print(f"{name}: hook insertion failed", file=sys.stderr)
                return 3
            inserted = 1
        target = out_dir / name
        target.write_text(out, encoding="utf-8")
        report["files"][name] = {
            "source_sha256": digest,
            "output_sha256": sha(out),
            "signature": a["signature"],
            "hook": a["hook"],
            "hook_inserted": bool(inserted),
        }
    report["source_files"] = len(ANCHORS)
    report["patched_files"] = len(ANCHORS) if not args.rollback else 0
    Path(args.report).write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"status": report["status"], "patched": report["patched_files"],
                      "rollback": args.rollback}, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
