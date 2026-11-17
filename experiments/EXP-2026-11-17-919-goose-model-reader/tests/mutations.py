#!/usr/bin/env python3
"""Independent semantic mutation gates; one named variant at a time."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess
import sys

E=Path(__file__).resolve().parents[1];ROOT=E.parents[1]


def main():
    p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);p.add_argument('--support',type=Path,required=True)
    p.add_argument('--name',choices=['publish','payload','array-span','constant-hash','hash-only','inventory','live-frame','rope-bytes'],required=True);a=p.parse_args();out=a.out.resolve()
    if not out.is_relative_to(Path('/home/end0/AI/LlamaServer')):raise SystemExit('artifacts only in LlamaServer')
    model=a.name in ['inventory','live-frame','rope-bytes'];source=(out/'unit.goose').read_text();goose=json.loads((ROOT/'runtime/compiler/current.json').read_text())['compiler']
    replacements={
        'publish':('state[0] = 0;','state[0] = 919;'),
        'payload':('total > file_bytes - data_offset','false'),
        'array-span':('pairs[i * 2] = at + 8;','pairs[i * 2] = at + 7;'),
        'inventory':('if scratch[4 + i] == 0 {','if false {'),
        'live-frame':('owner[0] != 9191 || owner[6] != 0 { return -1; }','owner[0] != 9191 || false { return -1; }'),
        'rope-bytes':('(sections[i] >> (8 * j)) & 255','((sections[i] >> (8 * j)) & 255) ^ 1')}
    if a.name in ('constant-hash','hash-only'):
        pattern=r'fn gr_hash\([^{}]+\{.*?\n\}';matches=list(re.finditer(pattern,source,re.S));assert len(matches)==1
        source=source[:matches[0].start()]+'fn gr_hash(data: const u8[:], at: i64, n: i64) -> u64 { return 1; }'+source[matches[0].end():]
        if a.name=='hash-only':
            old='&&\n           gr_same(data, records[prior], records[prior + 1], records[base], records[base + 1])'
            assert source.count(old)==1;source=source.replace(old,'')
    else:
        old,new=replacements[a.name];assert source.count(old)==1,(a.name,source.count(old));source=source.replace(old,new)
    directory=out/('probe-'+a.name);directory.mkdir(exist_ok=False);unit=directory/'unit.goose';unit.write_text(source)
    with (directory/'gate.log').open('x') as log:
        def run(cmd,reject=False):
            cmd=list(map(str,cmd));line='$ '+shlex.join(cmd);log.write(line+'\n');log.flush();print(line,flush=True)
            r=subprocess.run(cmd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=180)
            log.write(r.stdout+'\nexit_code='+str(r.returncode)+'\n');log.flush();print(r.stdout[-2500:],flush=True)
            if reject:
                if r.returncode!=1 or 'AssertionError' not in r.stdout:raise RuntimeError('must fail a semantic assertion, not crash or compiler error')
            elif r.returncode:raise RuntimeError('command failed')
        run([goose,'check',unit]);run([goose,'--standalone','-o',directory/'runtime.c','--header',directory/'runtime.h',unit])
        lib=directory/('model.so' if model else 'reader.so');cmd=['gcc-13','-O0','-g','-fPIC','-shared','-Wl,-z,defs',directory/'runtime.c']
        if model:cmd += [out/'bindings.o',out/'existing-byte-store.o',a.support/'bin/libggml-base.so.0','-Wl,-rpath,'+str(a.support/'bin')]
        run([*cmd,'-lm','-o',lib])
        if model:gate=[sys.executable,E/'tests/model_contracts.py',lib]
        else:
            case='synthetic-extra' if a.name=='payload' else 'synthetic'
            gate=[sys.executable,E/'tests/gate.py',lib,'--native',a.support/'bin/libggml-base.so.0','--case',case]
            if a.name in ('constant-hash','hash-only'):gate+=['--constant-hash']
        run(gate,reject=a.name!='constant-hash')
        log.write(('CONTROL PASS' if a.name=='constant-hash' else 'KILLED '+a.name)+'\nRESULT PASS\n')
    print('PASS',a.name,'positive collision control' if a.name=='constant-hash' else 'semantic mutation rejected')


if __name__=='__main__':main()
