#!/usr/bin/env python3
"""Overlay the selected integration copies onto the staged flint tree."""
from pathlib import Path
import shutil
BASE = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = BASE/'experiments'; SRC = BASE/'runtime/reconstructed/src'

picks = {
 'llama-batch.cpp': 243, 'llama-context.cpp': 244, 'llama-graph.cpp': 200, 'llama-hparams.cpp': 200,
 'llama-kv-cache.cpp': 233, 'llama-memory-recurrent.cpp': 232, 'llama-model-loader.cpp': 201,
 'llama-sampler.cpp': 212, 'llama-vocab.cpp': 220, 'unicode.cpp': 218,
 'llama-kv-cache.h': 233, 'llama-kv-cells.h': 233,
}
for name, num in picks.items():
    cands = sorted(E.glob(f'EXP-*-{num}-*/integration/{name}'))
    assert cands, (name, num)
    dst = SRC/name
    assert dst.exists(), ('staged file missing', name)
    shutil.copy2(cands[0], dst)
    print(f'{name:28s} <- EXP{num} ({dst.stat().st_size} bytes)')
