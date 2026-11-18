#!/usr/bin/env python3
"""Check actual CUDA registration defaults on one anonymous page, without a model."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys

ap = argparse.ArgumentParser(description=__doc__)
ap.add_argument('--control', type=Path, required=True)
ap.add_argument('--candidate', type=Path, required=True)
a = ap.parse_args()
child = r'''
import ctypes, mmap, sys, json
from pathlib import Path
lib = ctypes.CDLL(str(Path(sys.argv[1])/'libggml-cuda.so.0'))
lib.ggml_backend_cuda_register_host_buffer.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
lib.ggml_backend_cuda_register_host_buffer.restype = ctypes.c_bool
lib.ggml_backend_cuda_unregister_host_buffer.argtypes = [ctypes.c_void_p]
page = mmap.mmap(-1, mmap.PAGESIZE)
view = (ctypes.c_char*mmap.PAGESIZE).from_buffer(page)
ptr = ctypes.addressof(view)
registered = lib.ggml_backend_cuda_register_host_buffer(ptr, mmap.PAGESIZE)
if registered:
    lib.ggml_backend_cuda_unregister_host_buffer(ptr)
del view
page.close()
print(json.dumps({'case':sys.argv[2], 'bytes':mmap.PAGESIZE, 'registered':registered}))
raise SystemExit(0 if registered == (sys.argv[3]=='1') else 1)
'''
for label, runtime, override, expected in [
    ('control-default', a.control, None, False),
    ('candidate-default', a.candidate, None, True),
    ('candidate-disabled', a.candidate, '0', False),
]:
    env = {k:v for k,v in os.environ.items() if k not in ('LD_PRELOAD','GGML_CUDA_REGISTER_HOST','GGML_CUDA_NO_PINNED')}
    env['LD_LIBRARY_PATH'] = str(runtime.resolve())
    if override is not None:
        env['GGML_CUDA_REGISTER_HOST'] = override
    q = subprocess.run([sys.executable, '-c', child, str(runtime.resolve()), label, str(int(expected))], env=env)
    print('exit:', q.returncode, flush=True)
    q.check_returncode()
print('host-registration hardware probe: 3/3 passed')
