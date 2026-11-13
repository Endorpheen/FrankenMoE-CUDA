"""Builds the three invalid GGUF fixtures used by EXP-302: invalid metadata type, unsupported version, truncation."""
import struct, sys
from pathlib import Path

def gguf(path, ver=3, kv=None):
    b = struct.pack('<IIQQ', 0x46554747, ver, 0, len(kv) if kv else 0)
    for k, t, v in (kv or []):
        kb = k.encode(); b += struct.pack('<Q', len(kb)) + kb + struct.pack('<I', t)
        if t == 8:   b += struct.pack('<Q', len(v)) + v.encode()
        elif t == 4: b += struct.pack('<I', v)

    Path(path).write_bytes(b)

out = Path(sys.argv[1] if len(sys.argv) > 1 else '.')
out.mkdir(parents=True, exist_ok=True)
gguf(out / 'bad-type.gguf', kv=[('general.architecture', 99, 0)])
gguf(out / 'bad-version.gguf', ver=999, kv=[('general.architecture', 8, 'llama')])
gguf(out / 'truncated.gguf', kv=[('general.architecture', 8, 'llama'), ('general.name', 8, 'x')])
b = (out / 'truncated.gguf').read_bytes()
(out / 'truncated.gguf').write_bytes(b[:len(b) - 4])
