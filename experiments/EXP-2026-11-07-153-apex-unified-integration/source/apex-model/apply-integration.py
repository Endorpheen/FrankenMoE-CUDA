from pathlib import Path
import hashlib,sys,shutil
source=Path(__file__).resolve().parent
target=Path(sys.argv[1]).resolve()
assert target.is_relative_to(Path('/home/end0/AI/LlamaServer').resolve())
assert target!=Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda').resolve()
file=target/'src/models/qwen35moe.cpp'
assert hashlib.sha256(file.read_bytes()).hexdigest()=='579426f57c8691279ab9231264b5a0bceb214e57b3876d58020a1f40a98647a4', 'native model source hash mismatch'
shutil.copyfile(source/'qwen35moe.cpp',file)
print('Applied copied model integration to',file)
