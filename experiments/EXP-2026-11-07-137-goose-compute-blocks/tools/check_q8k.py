#!/usr/bin/env python3
"""Compare frozen Goose Q8_K with native control under identical build flags."""
from pathlib import Path
import os
import shlex
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
cc = shlex.split(os.environ.get('CC', 'cc'))
flags = ['-O3', '-march=native', '-ffp-contract=off', '-fPIC', '-shared']
with tempfile.TemporaryDirectory(prefix='franken-goose-q8k-') as temporary:
    work = Path(temporary)
    (work / 'build').mkdir()
    (work / 'results').mkdir()
    sources = [
        ('unit.c', 'libgoose-q8kv2-quant.so'),
        ('native-same-flags.c', 'libq8k-native-sameflags.so'),
    ]
    for source, library in sources:
        subprocess.run(cc + flags + [str(root / 'source/q8k-fixed' / source),
            '-lm', '-o', str(work / 'build' / library)], check=True)
    program = (root / 'source/q8k-fixed/fg_q8k_sameflags_check.py').read_text()
    old = "r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107')"
    assert program.count(old) == 1
    program = program.replace(old, 'r=Path(' + repr(temporary) + ')', 1)
    subprocess.run([sys.executable, '-c', program], check=True)
