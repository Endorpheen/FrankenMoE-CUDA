#!/usr/bin/env python3
# Native side of the fixture gate (P10-09 S2).
#
# Runs the REAL upstream generator (tests/native/make_tiny_moe.py, copied byte
# for byte from scripts/make-tiny-moe.py) with recording stand-ins for gguf and
# numpy, and writes the plan as normalised rows:
#
#   W <build> <path> <arch> split=<n> small_first=<k>   writer construction
#   M <build> <i> <call> ...                            metadata, in order
#   T <build> <i> <name> <shape> <seed>                 tensor, in order
#   B <build> <banner>                                  the printed line
#   G <arch> <split> <message>                          the split guard
#
# Nothing is written to disk and no weight is generated: the stand-ins record
# the plan only, which is what the port owns.
#
# Usage: capture_fx.py <out.tsv>
import importlib.util
import sys
import types
from pathlib import Path

S = Path(__file__).resolve().parent.parent


class Tensor:
    def __init__(self, shape, seed):
        self.shape = shape
        self.seed = seed

    def astype(self, _dtype):
        return self

    def __mul__(self, _k):
        return self


class Recorder:
    def __init__(self, rows):
        self.rows = rows

    def _c(self, name, *args):
        flat = []
        for a in args:
            if isinstance(a, (list, tuple)):
                flat.append('[' + ','.join(str(x) for x in a) + ']')
            else:
                flat.append(str(a))
        self.rows.append('%s %s' % (name, ' '.join(flat)))


def make_gguf(meta, tensors, ctor):
    m = types.ModuleType('gguf')

    class TokenType:
        UNKNOWN = 2
        CONTROL = 3
        BYTE = 6

    class LlamaFileType:
        ALL_F32 = 0

    class GGUFWriter:
        def __init__(self, path, arch, **kw):
            ctor.append('%s %s split=%s small_first=%s'
                        % (path, arch, kw.get('split_max_tensors', 0),
                           int(kw.get('small_first_shard', False))))
            self.rec = Recorder(meta)

        def __getattr__(self, name):
            def call(*args):
                self.rec._c(name, *args)
            return call

        def add_tensor(self, name, t):
            tensors.append('%s %s %d' % (name, 'x'.join(str(d) for d in t.shape), t.seed))

        def close(self):
            pass

    m.TokenType = TokenType
    m.LlamaFileType = LlamaFileType
    m.GGUFWriter = GGUFWriter
    return m


def make_numpy():
    m = types.ModuleType('numpy')
    m.float32 = 'f32'
    holder = {'seed': 0}

    class Gen:
        def standard_normal(self, shape):
            return Tensor(shape, holder['seed'])

    def default_rng(seed):
        holder['seed'] = seed
        return Gen()

    m.random = types.SimpleNamespace(default_rng=default_rng)
    return m


def load_native():
    sys.modules['gguf'] = make_gguf([], [], [])
    sys.modules['numpy'] = make_numpy()
    path = S / 'tests' / 'native' / 'make_tiny_moe.py'
    spec = importlib.util.spec_from_file_location('make_tiny_moe', str(path))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def build(mod, build_name, path, arch, split):
    """One build, with its own recorders so the rows stay ordered by stage."""
    meta, tensors, ctor, banners = [], [], [], []
    sys.modules['gguf'] = make_gguf(meta, tensors, ctor)
    sys.modules['numpy'] = make_numpy()
    mod.gguf = sys.modules['gguf']
    mod.np = sys.modules['numpy']
    out = []
    if arch == 'gemma4':
        mod.build_gemma4(path)
    else:
        mod.build_qwen3moe(path, split)
    out.extend('W %s %s' % (build_name, c) for c in ctor)
    out.extend('M %s %d %s' % (build_name, i, r) for i, r in enumerate(meta))
    out.extend('T %s %d %s' % (build_name, i, r) for i, r in enumerate(tensors))
    banner = ('wrote %s: ' % path) + (
        'gemma4, 4 layers (3 MoE, fused gate_up), 8 experts (top-2), vocab 259'
        if arch == 'gemma4' else
        'qwen3moe, 4 layers, 8 experts (top-2), vocab 259')
    out.append('B %s %s' % (build_name, banner))
    return out


def main():
    out_path = Path(sys.argv[1]) if len(sys.argv) > 1 else S / 'build' / 'native_fx.tsv'
    mod = load_native()
    rows = []
    rows += build(mod, 'qwen3moe', '/tmp/out/tiny-moe.gguf', 'qwen3moe', 0)
    rows += build(mod, 'gemma4', '/tmp/out/tiny-moe-gemma4.gguf', 'gemma4', 0)
    split = build(mod, 'qwen3moe-split', '/tmp/out/tiny-moe-split.gguf', 'qwen3moe', 20)
    rows += [r for r in split if r.startswith(('W ', 'B '))]
    # the split guard, from the real entry point
    sys.argv = ['make-tiny-moe.py', '--arch', 'gemma4', '--split-max-tensors', '20']
    try:
        mod.main()
        rows.append('G gemma4 accepted')
    except SystemExit as e:
        rows.append('G gemma4 refused %s' % e)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text('\n'.join(rows) + '\n')
    print('rows: %d' % len(rows))


if __name__ == '__main__':
    main()
