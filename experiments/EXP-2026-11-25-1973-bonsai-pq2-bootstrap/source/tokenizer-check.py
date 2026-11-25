"""Check Goose tokenizer IDs against an independent local llama.cpp CLI."""
from hashlib import sha256
import json
from pathlib import Path
import subprocess
import sys

goose, llama, vocab = map(Path, sys.argv[1:4])
inputs = (
    'Привет, Goose! print(42) 🪿',
    '答えは42です。Hello, 世界!',
    '  first\n\tsecond   01234\r\n',
    '<|im_start|>user\nCall the weather tool.<|im_end|>\n<|im_start|>assistant\n',
    'if x is not None:\n    print(x)\n',
)
rows = []
for value in inputs:
    commands = (
        [str(goose.resolve()), str(vocab.resolve()), '--stdin', '--ids', '--no-bos'],
        [str(llama.resolve()), '-m', str(vocab.resolve()), '--stdin', '--ids', '--no-bos', '--no-escape'],
    )
    results = [json.loads(subprocess.run(command,input=value,text=True,
               capture_output=True,check=True).stdout) for command in commands]
    if results[0] != results[1]:
        raise AssertionError((value,*results))
    rows.append({'text':value,'ids':results[0]})
print(json.dumps({'vocabulary_sha256':sha256(vocab.read_bytes()).hexdigest(),
     'goose_sha256':sha256(goose.read_bytes()).hexdigest(),'llama_sha256':sha256(llama.read_bytes()).hexdigest(),
     'matched':len(rows),'fixtures':rows},indent=2,ensure_ascii=False))
