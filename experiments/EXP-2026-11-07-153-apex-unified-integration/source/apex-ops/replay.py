import argparse, hashlib, json, pathlib, re

here=pathlib.Path(__file__).resolve().parent
parser=argparse.ArgumentParser(description='Replay only Apex operation dispatch hooks onto a chosen GGML core copy.')
parser.add_argument('--input',required=True,type=pathlib.Path)
parser.add_argument('--output',required=True,type=pathlib.Path)
args=parser.parse_args()
original=args.input.read_text();text=original
manifest=json.loads((here/'manifest.json').read_text())
insertions=[]
for item in manifest:
    function=item['function'];mode=item['mode']
    mat=re.search(r'\b'+function+r'\s*\(([^;{}]*?)\)\s*\{',text,re.S)
    if not mat:raise SystemExit('Function definition missing: '+function)
    params=[p.strip() for p in mat[1].split(',')]
    if [re.sub(r'\s+',' ',p) for p in params]!=[re.sub(r'\s+',' ',p) for p in item['parameters']]:
        raise SystemExit('Function signature changed: '+function)
    if 'fg_apex_ops_enabled()' in text[mat.end():mat.end()+600]:
        raise SystemExit('Apex dispatch already present: '+function)
    expressions=[]
    for param in params:
        variable=re.search(r'(\w+)\s*(?:\[[^\]]*\])?$',param)[1]
        if re.search(r'\bfloat\b',param):expressions.append('fg_apex_ops_float_bits('+variable+')')
        elif '*' in param or '[' in param:expressions.append('(intptr_t)'+variable)
        else:expressions.append('(int64_t)'+variable)
    call=f'fg_apex_ops_entry({mode},fg_args)'
    ret=item['return'];statement=f'return (void*)(intptr_t){call};' if ret=='void*' else f'{call};return;' if ret=='void' else f'return {call}!=0;'
    branch='\n    if(fg_apex_ops_enabled()){const int64_t fg_args[16]={'+','.join(expressions)+'};'+statement+'}\n'
    insertions.append((mat.end(),branch))
for position,branch in sorted(insertions,reverse=True):text=text[:position]+branch+text[position:]
header='#include "../apex-ops/apex-ops.h"'
if header not in text:
    marker='#include "bridge.h"' if '#include "bridge.h"' in text else '#include "ggml.h"'
    if marker not in text:raise SystemExit('GGML header inclusion missing')
    text=text.replace(marker,marker+'\n'+header,1)
projection='#include "../apex-ops/projection.inc"'
if projection not in text:text+='\n'+projection+'\n'
args.output.write_text(text)
print(json.dumps({'input':str(args.input),'output':str(args.output),'before_sha256':hashlib.sha256(original.encode()).hexdigest(),'after_sha256':hashlib.sha256(text.encode()).hexdigest(),'dispatch_entries':len(manifest)},indent=2))
