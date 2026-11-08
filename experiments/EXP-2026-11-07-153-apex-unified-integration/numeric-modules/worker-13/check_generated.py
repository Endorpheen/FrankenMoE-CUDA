#!/usr/bin/env python3
# check_generated.py - verify the generated device objects and the exact patch.
import os, re, subprocess, sys
W = "/work"
def read(p):
    return open(p, "r", encoding="utf-8").read()
def check_device(obj, cu, kernel):
    nm = subprocess.run(["/usr/bin/nm", "-a", obj], capture_output=True, text=True)
    if nm.returncode != 0:
        raise SystemExit("nm failed on " + obj)
    if "gg_function_1" not in nm.stdout:
        raise SystemExit("emitter entry gg_function_1 is not reachable in " + obj)
    if "fg153_goose" not in nm.stdout:
        raise SystemExit("kernel symbol missing in " + obj)
    text = read(cu)
    if kernel not in text or "gg_function_1((int64_t)(uintptr_t)&cfg, 0)" not in text:
        raise SystemExit("kernel wrapper missing in " + cu)
    if "gg_stacks" in text or "gg_get_stack" in text:
        raise SystemExit("shared global scratch remains in " + cu)
    body = text.split('extern "C" __global__ void', 1)[1]
    prefix = text.split('extern "C" __global__ void', 1)[0]
    used = set(re.findall(r"\b(fg153_[a-z0-9_]+)\s*\(", body))
    for name in used:
        if name.startswith("fg153_goose_"):
            continue
        if (" " + name + "(") not in prefix and ("*" + name + "(") not in prefix:
            raise SystemExit("missing strict prototype/definition for " + name + " in " + cu)
    print("FG153_DEVICE_CHECK %s kernel=%s entry=gg_function_1 PASS" % (os.path.basename(obj), kernel))
def check_patch():
    calls = {"gated_delta_net.cu": "fg153_goose_gdn_launch", "ssm-conv.cu": "fg153_goose_ssm_conv_launch"}
    for name, call in calls.items():
        text = read(os.path.join(W, "patched", name))
        if "FG153 Goose" not in text or "0 rolls back" not in text:
            raise SystemExit("patch marker missing in " + name)
        if text.count(call) != 1:
            raise SystemExit("bridge call missing/duplicated in " + name)
    for env in ("GGML_CUDA_GOOSE_GDN", "GGML_CUDA_GOOSE_SSM_CONV"):
        if env not in read(os.path.join(W, "patched", "fg153-goose-cuda.h")):
            raise SystemExit("rollback variable missing: " + env)
    print("FG153_PATCH_CHECK default=on rollback_env=2 PASS")
def main():
    check_device(os.path.join(W, "build", "gdn.o"), os.path.join(W, "build", "gdn.cu"), "fg153_goose_gdn_kernel")
    check_device(os.path.join(W, "build", "conv.o"), os.path.join(W, "build", "conv.cu"), "fg153_goose_ssm_conv_kernel")
    check_patch()
if __name__ == "__main__":
    main()
