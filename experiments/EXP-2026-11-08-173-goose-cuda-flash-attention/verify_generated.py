import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
from tempfile import TemporaryDirectory

from generate_prefill_headers import function_body, materialize


EXPERIMENT = Path(__file__).resolve().parent
ROOT = EXPERIMENT.parents[1]
SOURCE = EXPERIMENT / "source"
COMPILER_SHA256 = "f849233b999ca9e931ca40dcbee51e292ec9e553d66bed32e2a4af27d46bdf3a"
MODULES = [('vector-device','vector-functions-generated.cuh','fg175vd_'),('loaders-device','loaders-functions-generated.cuh','fg175ld_'),('staging-device','staging-functions-generated.cuh','fg175qd_'),('mma-iter-device','mma-iter-functions-generated.cuh','fg175mi_'),('mma-process-device','mma-process-functions-generated.cuh','fg175mp_'),('mma-entry-device','mma-entry-device-functions-generated.cuh','fg175me_'),('mask-fixups-device','mask-fixups-device-functions-generated.cuh','fg175fx_'),('value-prefill-device','value-prefill-functions-generated.cuh','fg175cv_'),('host','host-qualified.c','fg175hh_')]


def bodies(text):
    return {
        match[1]: function_body(text, match)
        for match in re.finditer(r'(?m)^static [^\n{]+? (gg_function_\d+)\([^;{}]*\)\s*\{', text)
        if match[1] != 'gg_function_0'
    }


def main():
    parser = argparse.ArgumentParser(description="Verify pinned Goose output and CUDA compute headers")
    parser.add_argument("--prefill-headers", type=Path, help="directory receiving the verified prefill headers")
    args = parser.parse_args()
    compiler = Path(os.environ.get("EXP173_GOOSE", ROOT / "runtime/compiler/stage3-f849233b999c"))
    if hashlib.sha256(compiler.read_bytes()).hexdigest() != COMPILER_SHA256:
        raise SystemExit("EXP173 compiler checksum mismatch")

    rows = []
    with TemporaryDirectory(prefix="fg173-source-verify-") as directory:
        scratch = Path(directory)
        headers = materialize(SOURCE, args.prefill_headers or scratch)
        for name, header, prefix in MODULES:
            emitted = scratch / (name + ".c")
            subprocess.run([str(compiler), "-o", str(emitted), str(SOURCE / (name + ".goose"))], check=True)
            tracked = (SOURCE / (name + "-generated.c")).read_bytes()
            if emitted.read_bytes() != tracked:
                raise SystemExit(f"{name} generated C is stale")
            compute = headers[header].decode() if header in headers else (SOURCE / header).read_text()
            qualified = compute.replace(prefix, "").replace("static __device__ __forceinline__ ", "static ")
            functions = bodies(qualified)
            if functions != bodies(tracked.decode()):
                raise SystemExit(f"{name} compute header differs from actual Goose emission")
            if name != "host" and any(
                call in body for body in functions.values()
                for call in ("gg_allocate(", "gg_get_stack(", "gg_append(", "gg_ref(")
            ):
                raise SystemExit(f"{name} device code calls a host-only function")
            rows.append(dict(module=name, generated_c_sha256=hashlib.sha256(tracked).hexdigest(), function_count=len(functions)))
            print("PASS actual self-hosted emission and compute body", name, flush=True)

    receipt = dict(status="PASS_CURRENT_SELFHOSTED_C_AND_MECHANICAL_COMPUTE_HEADERS",
                   compiler_sha256=COMPILER_SHA256, modules=rows)
    (EXPERIMENT / "source-verify.json").write_text(json.dumps(receipt, indent=2) + "\n")


if __name__ == "__main__":
    main()
