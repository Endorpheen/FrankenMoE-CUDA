#!/usr/bin/env python3
from pathlib import Path
import subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-473-p7-09-refs'
SRC = E / 'source/refs.goose'
ORIG = SRC.read_text()

M = [
    ('invalid runs are not collapsed',
     '   if !in_run {\n    if dst+n<out.len { out[dst+n]=\'-\'; }\n    n=n+1;\n    in_run=true;\n   }',
     '   if true {\n    if dst+n<out.len { out[dst+n]=\'-\'; }\n    n=n+1;\n    in_run=true;\n   }'),
    ('underscore treated as a valid rule byte',
     " if b == '-' { return true; }",
     " if b == '-' || b == '_' { return true; }"),
    ('invalid byte kept instead of a dash',
     "    if dst+n<out.len { out[dst+n]='-'; }",
     "    if dst+n<out.len { out[dst+n]=s[i]; }"),
    ('last hash instead of the first',
     ' var i:i64=0;\n while i<ref.len {\n  if ref[i]==\'#\' { return i+1; }\n  i=i+1;\n }\n return 0;',
     ' var i:i64=ref.len-1;\n while i>=0 {\n  if ref[i]==\'#\' { return i+1; }\n  i=i-1;\n }\n return 0;'),
    ('ref prefix becomes "rex"',
     "  out[2]='f';",
     "  out[2]='x';"),
    ('date-time-string is no longer reserved',
     '  "date-time-string",\n',
     ''),
    ('reserved name loses its dash',
     "  if n<out.len { out[n]='-'; }\n  return n+1;",
     "  return n;"),
    ('empty name stops becoming root',
     '  return 4;',
     '  return 0;'),
    ('empty sub-name still gets a dash',
     ' if name.len!=0 {\n  if n<out.len { out[n]=\'-\'; }\n  return n+1;\n }\n return n;',
     "  if n<out.len { out[n]='-'; }\n  return n+1;"),
    ('a different rule text reuses the plain name',
     ' if found<0 { return esc_len; }',
     ' if found<0 || found>=0 { return esc_len; }'),
    ('suffix ladder starts at 1',
     ' var i:i64=0;\n while i>=0 {',
     ' var i:i64=1;\n while i>=0 {'),
    ('suffix joined with a dot',
     '  let total=esc_len+append_decimal(out,esc_len,i);',
     "  if esc_len<out.len { out[esc_len]='.'; }\n  let total=esc_len+1+append_decimal(out,esc_len+1,i);"),
    ('already-resolving ref is visited again',
     ' if rule_exists==0 && resolving==0 {',
     ' if rule_exists==0 {'),
    ('missing target is visited, present target errors',
     '  if has_target==0 {\n   action=1;\n   error=1;\n  } else {\n   action=2;\n  }',
     '  if has_target!=0 {\n   action=1;\n   error=1;\n  } else {\n   action=2;\n  }'),
]


def run():
    r = subprocess.run(['bash', str(E / 'build_fixture.sh')], text=True,
                       capture_output=True, timeout=900, errors='replace')
    line = [x for x in r.stdout.splitlines() if x.startswith('FG473_REFS')]
    if line:
        return r.returncode, line[-1]
    if 'wrote ' not in r.stdout:
        return None, 'emission failed'
    last = r.stderr.strip().splitlines()[-1] if r.stderr.strip() else 'no stderr'
    return max(r.returncode, 1), 'build/runtime failure: ' + last[:120]


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
