#!/usr/bin/env python3
from pathlib import Path
import subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-477-p7-09-notstrings'
SRC = E / 'source/notstrings.goose'
ORIG = SRC.read_text()

M = [
    ('children are appended unsorted',
     '   if before>=0 {\n    enext[ei]=before;\n   } else {\n    enext[ei]=NO_EDGE;\n   }\n   if prev>=0 { enext[prev]=ei; } else { head[node]=ei; }',
     '   enext[ei]=NO_EDGE;\n   if prev>=0 { enext[prev]=ei; } else { head[node]=ei; }'),
    ('three-byte UTF-8 decoding is wrong',
     '  return ((b0-224) as i64)*4096+((s[i+1]-128) as i64)*64+((s[i+2]-128) as i64),i+3;',
     '  return ((b0-224) as i64)*4096+((s[i+1]-128) as i64)*64+((s[i+2]-128) as i64)+1,i+3;'),
    ('two-byte UTF-8 encoding is wrong',
     '   pos=append_c(rbuf,pos,(192+(cpt/64)) as u8);',
     '   pos=append_c(rbuf,pos,(193+(cpt/64)) as u8);'),
    ('the trailing question mark is inverted',
     ' if st[S_ROOTPAT]==0 { p=append_c(out,p,\'?\'); }',
     ' if st[S_ROOTPAT]!=0 { p=append_c(out,p,\'?\'); }'),
    ('an empty word never marks the root',
     ' if s.len==0 { st[S_ROOTPAT]=1; }',
     ' if s.len==99 { st[S_ROOTPAT]=1; }'),
    ('rejects keep only the last child',
     '  q=append_s(out,q,rbuf,base,rlen);',
     '  q=append_s(out,q,rbuf,base+rlen-1,1);'),
    ('the exclusion class loses its quote',
     '  q=append_s(out,q," | [^\\"",0,6);',
     '  q=append_s(out,q," | [^",0,5);'),
    ('leaf repetition uses star',
     "   q=append_c(out,q,'+');",
     "   q=append_c(out,q,'*');"),
    ('nested groups lose their wrapper',
     '   q=append_s(out,q," (",0,2);',
     '   q=append_s(out,q,"",0,0);'),
    ('alternation loses its spaces',
     '  if !first { q=append_s(out,q," | ",0,3); }',
     '  if !first { q=append_s(out,q,"|",0,1); }'),
    ('the root opener loses a space',
     ' var p=append_s(out,0,"[\\"] ( ",0,6);',
     ' var p=append_s(out,0,"[\\"] (",0,5);'),
    ('the closing bracket keeps a quote',
     ' p=append_s(out,p," [\\"]",0,4);',
     ' p=append_s(out,p,"[\\"]",0,3);'),
    ('existing children are never reused',
     '  if found>=0 {\n   node=found;\n  } else {',
     '  if found>=99 {\n   node=found;\n  } else {'),
    ('bytes above 127 are not decoded',
     ' if b0>=194 && b0<=223 && i+1<s.len {',
     ' if b0>=250 && b0<=223 && i+1<s.len {'),
]


def run():
    try:
        r = subprocess.run(['bash', str(E / 'build_fixture.sh')], text=True,
                           capture_output=True, timeout=180, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'timeout (non-terminating mutation)'
    line = [x for x in r.stdout.splitlines() if x.startswith('FG477_NOTSTRINGS')]
    if line:
        return r.returncode, line[-1]
    if 'wrote ' not in r.stdout:
        return None, 'emission failed'
    last = r.stderr.strip().splitlines()[-1] if r.stderr.strip() else 'no stderr'
    return max(r.returncode, 1), 'build failure: ' + last[:140]


caught = 0
try:
    for label, old, new in M:
        if old not in ORIG:
            print('[skip]', label)
            continue
        SRC.write_text(ORIG.replace(old, new, 1))
        rc, line = run()
        if rc is None:
            print('[build-fail]', label)
            continue
        ok = rc != 0
        caught += ok
        print(f'[{"caught" if ok else "MISSED"}] {label}: rc={rc} {line}')
finally:
    SRC.write_text(ORIG)
    run()
print(f'mutations {caught}/{len(M)} caught')
