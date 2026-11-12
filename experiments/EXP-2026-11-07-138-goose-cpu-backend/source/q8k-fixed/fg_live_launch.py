import json
import os
from pathlib import Path
import sys

root = Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107')
mode = sys.argv[1]
args = json.loads(Path('/home/end0/AI/FrankenMoE-CUDA/results/exp107-expert-overlap/launch-profiles.json').read_text())['profiles']['apex']
args[0] = str(root / 'run/llama-server')
args[args.index('-m') + 1] = '/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf'
env = dict(os.environ)
env['LD_LIBRARY_PATH'] = str(root / 'run')
env.pop('LD_PRELOAD', None)
if mode.startswith('rms') or mode.startswith('q8k'):
    bundle = root / 'variants/graph-plan/run'
    args[0] = str(bundle / 'llama-server')
    env['LD_LIBRARY_PATH'] = str(bundle)
    env['GGML_GOOSE_ALLOC'] = '1'
    env['GGML_GOOSE_COALESCE'] = '1'
    env['GGML_GOOSE_LIFETIME'] = '0'
    env['GGML_GOOSE_GRAPH_PLAN'] = '1'
    env['GGML_GOOSE_ALLOC_VERIFY'] = '0'
    env['GGML_GOOSE_RMS'] = '0' if mode.endswith('-off') else '1'
    env['LD_PRELOAD'] = ' '.join(str(root / 'build' / x) for x in [
        'libfranken-goose-silu.so', 'libfranken-goose-scale.so',
        'libfranken-goose-rms-block-trace.so' if mode.startswith('rms-trace') else 'libfranken-goose-rms-block.so'])
    if mode.startswith('q8k'):
        env['GGML_GOOSE_RMS'] = '1'
        env['GGML_GOOSE_Q8K'] = '0' if mode.endswith('-off') else '1'
        env['LD_PRELOAD'] += ' ' + str(root / 'build' / ('libfranken-goose-' + ('q8kv2' if mode.startswith('q8kv2') else 'q8k') + ('-trace.so' if '-trace' in mode else '.so')))
elif mode.startswith('graph'):
    bundle = root / 'variants/graph-plan/run'
    args[0] = str(bundle / 'llama-server')
    env['LD_LIBRARY_PATH'] = str(bundle)
    env['GGML_GOOSE_ALLOC'] = '1'
    env['GGML_GOOSE_COALESCE'] = '1'
    env['GGML_GOOSE_LIFETIME'] = '0'
    env['GGML_GOOSE_GRAPH_PLAN'] = '0' if mode.endswith('-off') else '1'
    env['GGML_GOOSE_ALLOC_VERIFY'] = '1' if mode.startswith('graph-trace') else '0'
    env['LD_PRELOAD'] = ' '.join(str(root / 'build' / x) for x in [
        'libfranken-goose-silu.so', 'libfranken-goose-scale.so', 'libfranken-goose-norm.so'])
elif mode.startswith('lifetime'):
    bundle = root / 'variants/tensor-lifetime/run'
    args[0] = str(bundle / 'llama-server')
    env['LD_LIBRARY_PATH'] = str(bundle)
    env['GGML_GOOSE_ALLOC'] = '1'
    env['GGML_GOOSE_COALESCE'] = '1'
    env['GGML_GOOSE_LIFETIME'] = '0' if mode.endswith('-off') else '1'
    env['GGML_GOOSE_ALLOC_VERIFY'] = '1' if mode.startswith('lifetime-trace') else '0'
    env['LD_PRELOAD'] = ' '.join(str(root / 'build' / x) for x in [
        'libfranken-goose-silu.so', 'libfranken-goose-scale.so', 'libfranken-goose-norm.so'])
elif mode.startswith('native-control'):
    bundle = Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda/bin')
    args[0] = str(bundle / 'llama-server')
    env['LD_LIBRARY_PATH'] = str(bundle)
elif mode.startswith('upstream'):
    bundle = Path('/home/end0/AI/LlamaServer/franken-goose-upstream-b12179/bundle/llama-b12179')
    args[0] = str(bundle / 'llama-server')
    env['LD_LIBRARY_PATH'] = str(bundle)
elif mode.startswith('coalesce'):
    bundle = root / 'variants/allocator-coalesce/run'
    args[0] = str(bundle / 'llama-server')
    env['LD_LIBRARY_PATH'] = str(bundle)
    env['GGML_GOOSE_ALLOC'] = '1'
    env['GGML_GOOSE_COALESCE'] = '0' if mode.endswith('-off') else '1'
    env['GGML_GOOSE_ALLOC_VERIFY'] = '1' if mode.startswith('coalesce-trace') else '0'
    env['LD_PRELOAD'] = ' '.join(str(root / 'build' / x) for x in [
        'libfranken-goose-silu.so', 'libfranken-goose-scale.so', 'libfranken-goose-norm.so'])
elif mode.startswith('allocator'):
    bundle = root / 'variants/allocator-reuse/run'
    args[0] = str(bundle / 'llama-server')
    env['LD_LIBRARY_PATH'] = str(bundle)
    env['GGML_GOOSE_ALLOC'] = '0' if mode.endswith('-off') else '1'
    env['GGML_GOOSE_ALLOC_VERIFY'] = '1' if mode.startswith('allocator-trace') else '0'
    env['LD_PRELOAD'] = ' '.join(str(root / 'build' / x) for x in [
        'libfranken-goose-silu.so', 'libfranken-goose-scale.so', 'libfranken-goose-norm.so'])
elif mode.startswith('tile'):
    bundle = root / 'variants/q4-tile/run'
    args[0] = str(bundle / 'llama-server')
    env['LD_LIBRARY_PATH'] = str(bundle)
    env['GGML_GOOSE_Q4_TILE'] = '0' if mode == 'tile-off' else '1'
    env['LD_PRELOAD'] = ' '.join(str(root / 'build' / x) for x in [
        'libfranken-goose-silu.so', 'libfranken-goose-scale.so', 'libfranken-goose-norm.so'])
elif mode == 'q4-trace':
    env['LD_PRELOAD'] = ' '.join(str(root / 'build' / x) for x in [
        'libfranken-goose-silu.so', 'libfranken-goose-scale.so', 'libfranken-goose-norm.so',
        'libfranken-goose-q4-trace.so'])
elif mode == 'q4':
    env['LD_PRELOAD'] = ' '.join(str(root / 'build' / x) for x in [
        'libfranken-goose-silu.so', 'libfranken-goose-scale.so', 'libfranken-goose-norm.so',
        'libfranken-goose-q4.so'])
elif mode == 'all-trace':
    env['LD_PRELOAD'] = ' '.join(str(root / 'build' / x) for x in [
        'libfranken-goose-silu-trace.so', 'libfranken-goose-scale-trace.so', 'libfranken-goose-norm-trace.so'])
elif mode == 'all':
    env['LD_PRELOAD'] = ' '.join(str(root / 'build' / x) for x in [
        'libfranken-goose-silu.so', 'libfranken-goose-scale.so', 'libfranken-goose-norm.so'])
elif not mode.startswith('original'):
    env['LD_PRELOAD'] = str(root / 'build' / ('libfranken-goose-silu-trace.so' if mode.startswith('silu-trace') else 'libfranken-goose-silu.so'))
if mode.endswith('-cpu'):
    args[args.index('-c')+1] = '1024'
    args[args.index('-ngl')+1] = '0'
    args[args.index('-b')+1] = '512'
    args[args.index('-ub')+1] = '512'
    args[args.index('--ctx-checkpoints')+1] = '0'
(root / 'results/current-pid.txt').write_text(str(os.getpid()))
(root / 'results/current-mode.txt').write_text(mode)
(root / 'results' / (mode + '-argv.json')).write_text(json.dumps(args, indent=2))
(root / 'results' / (mode + '-environment.json')).write_text(json.dumps({
    'LD_LIBRARY_PATH': env.get('LD_LIBRARY_PATH'), 'LD_PRELOAD': env.get('LD_PRELOAD'),
    'GGML_GOOSE_Q4_TILE': env.get('GGML_GOOSE_Q4_TILE'), 'GGML_GOOSE_RMS':env.get('GGML_GOOSE_RMS'), 'GGML_GOOSE_Q8K':env.get('GGML_GOOSE_Q8K'),
    'GGML_GOOSE_ALLOC':env.get('GGML_GOOSE_ALLOC'), 'GGML_GOOSE_ALLOC_VERIFY':env.get('GGML_GOOSE_ALLOC_VERIFY'),
    'GGML_GOOSE_COALESCE':env.get('GGML_GOOSE_COALESCE'), 'GGML_GOOSE_LIFETIME':env.get('GGML_GOOSE_LIFETIME'), 'GGML_GOOSE_GRAPH_PLAN':env.get('GGML_GOOSE_GRAPH_PLAN'),
    'pid': os.getpid(), 'mode': mode}, indent=2))
log = os.open(root / 'results' / (mode + '-server.log'), os.O_CREAT | os.O_WRONLY | os.O_TRUNC, 0o600)
os.dup2(log, 1)
os.dup2(log, 2)
os.execvpe(args[0], args, env)
