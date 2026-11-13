#!/usr/bin/env python3
"""Mutation gate for the GCP AIP and /props decisions (EXP456)."""
from pathlib import Path
import subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-456-p8-02-gcp-props'
GCP = E / 'source' / 'gcp.goose'
PROPS = E / 'source' / 'props.goose'
G_ORIG = GCP.read_text()
P_ORIG = PROPS.read_text()

MUTATIONS = [
    (GCP, G_ORIG, 'AIP_MODE length check becomes a minimum',
     ' if n==10 as i64 {', ' if n>=9 as i64 {'),
    (GCP, G_ORIG, 'route leading-slash test inverted',
     "  if (ld_u8(src,0 as u64) as i64)!=47 as i64 {", "  if (ld_u8(src,0 as u64) as i64)==47 as i64 {"),
    (GCP, G_ORIG, 'port override flag inverted', '  if server_port!=aip_port { overridden=1 as i64; }',
     '  if server_port==aip_port { overridden=1 as i64; }'),
    (GCP, G_ORIG, 'alias keeps the /v1 prefix', '    if (ld_u8(p,2 as u64) as i64)==49 as i64 { start=3 as i64; }',
     '    if (ld_u8(p,2 as u64) as i64)==49 as i64 { start=0 as i64; }'),
    (GCP, G_ORIG, 'alias stops upper-casing after a separator',
     '    if cap_next!=0 as i64 && c>=97 as i64 && c<=122 as i64 { e=c-32 as i64; }',
     '    if cap_next==2 as i64 && c>=97 as i64 && c<=122 as i64 { e=c-32 as i64; }'),
    (GCP, G_ORIG, 'predict limit off by one', '   if count>128 as i64 { status=3 as i64; }',
     '   if count>127 as i64 { status=3 as i64; }'),
    (GCP, G_ORIG, 'instance format check inverted', '  if has_format_string==0 as i64 { status=5 as i64; }',
     '  if has_format_string!=0 as i64 { status=5 as i64; }'),
    (PROPS, P_ORIG, 'connect_code key is never emitted',
     '  put(out,19 as i64,20 as i64);\n  n=20 as i64;', '  n=19 as i64;'),
    (PROPS, P_ORIG, 'props mode inverted', ' if is_sleeping!=0 as i64 { mode=1 as i64; }',
     ' if is_sleeping==0 as i64 { mode=1 as i64; }'),
    (PROPS, P_ORIG, 'POST /props decision inverted', ' if endpoint_props==0 as i64 { status=1 as i64; }',
     ' if endpoint_props!=0 as i64 { status=1 as i64; }'),
]


def run():
    r = subprocess.run(['bash', str(E / 'build_fixture.sh')], text=True,
                       capture_output=True, timeout=900)
    line = [x for x in r.stdout.splitlines() if x.startswith('FG456_GCP_PROPS')]
    if not line and r.returncode != 0:
        return None, 'build failed'
    return r.returncode, (line[-1] if line else '')


if __name__ == '__main__':
    caught = 0
    try:
        for src, orig, label, old, new in MUTATIONS:
            if old not in orig:
                print(f'[skip] {label}: anchor missing')
                continue
            src.write_text(orig.replace(old, new, 1))
            rc, line = run()
            src.write_text(orig)
            if rc is None:
                print(f'[build-fail] {label}')
                continue
            ok = rc != 0
            caught += ok
            print(f'[{"caught" if ok else "MISSED"}] {label}: rc={rc} {line}')
    finally:
        GCP.write_text(G_ORIG)
        PROPS.write_text(P_ORIG)
        run()
    print(f'mutations {caught}/{len(MUTATIONS)} caught')
