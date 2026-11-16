#!/usr/bin/env python3
"""Resolve which source provides each Goose module, and freeze the answer.

The rule reproduced here is build.py's, not an approximation:
  1. the prefixes are the ones OUR compiled objects reference as undefined symbols (nm -u);
  2. an explicit list of 25 prefixes is skipped, because those are the bridges linked separately;
  3. among candidates, the highest experiment number wins, and files named *generated* are skipped.

Approximations of this rule produced 27 modules, then 0 - both wrong. The working build has 41 needed prefixes and
36 resolved modules.

With RESOLUTION set, nothing is decided here at all: the file says module -> experiment directory and that answer
is used as given. That is what makes a regeneration reproducible instead of depending on which experiment
directories happen to exist.

Usage:
  resolve-modules.py <tree-dir> <base-experiments-root> <objects-dir> [resolution-file]
Writes <tree-dir>/module-objects.txt and <tree-dir>/module-dirs.txt.
"""
import os
import re
import shutil
import subprocess
import sys

BRIDGE_PREFIXES = {
    'fg345w', 'fg367s', 'fg373s', 'fg375s', 'fg376s', 'fg377s', 'fg380s', 'fg381s', 'fg382s', 'fg383s',
    'fg384s', 'fg385s', 'fg386s', 'fg387s', 'fg388s', 'fg389s', 'fg390s', 'fg391s', 'fg392s', 'fg393s',
    'fg394s', 'fg395s', 'fg396s', 'fg397s', 'fg398s', 'fg401s',
}


def prefixes_from_objects(objs_dir):
    prefixes = []
    for o in sorted(list(objs_dir.glob('our-*.o')) + list(objs_dir.glob('*.cpp.o'))):
        out = subprocess.run(['nm', '-u', str(o)], capture_output=True, text=True).stdout
        for line in out.splitlines():
            if not line.strip():
                continue
            m = re.match(r'^(fg\d{3}[a-z])', line.split()[-1])
            if m and m.group(1) not in BRIDGE_PREFIXES and m.group(1) not in prefixes:
                prefixes.append(m.group(1))
    return prefixes


def main():
    tree = os.path.abspath(sys.argv[1])
    base = os.path.abspath(sys.argv[2])
    objs_dir = os.path.abspath(sys.argv[3]) if len(sys.argv) > 3 else None
    resolution = os.path.abspath(sys.argv[4]) if len(sys.argv) > 4 and sys.argv[4] else None

    dst = __import__('pathlib').Path(tree)
    base_p = __import__('pathlib').Path(base)
    (dst / 'modules').mkdir(parents=True, exist_ok=True)
    (dst / 'module-src').mkdir(parents=True, exist_ok=True)

    fixed = {}
    if resolution:
        for line in open(resolution):
            parts = line.split()
            if len(parts) == 2:
                fixed[parts[0]] = parts[1]
        print(f'resolution pinned from {resolution}: {len(fixed)} modules')

    prefixes = prefixes_from_objects(__import__('pathlib').Path(objs_dir)) if objs_dir else []
    print('modules needed:', len(prefixes))

    chosen = []
    for p in prefixes:
        cands = []
        for hu in list(base_p.glob('experiments/*/source/*.c')) + list(base_p.glob('experiments/*/source/*.cpp')):
            if 'generated' in hu.name:
                continue
            if (p + '_enabled').encode() in hu.read_bytes():
                mm = re.search(r'EXP-\d{4}-\d{2}-\d{2}-(\d+)', str(hu))
                if mm:
                    cands.append((int(mm.group(1)), hu))

        want = fixed.get(p + '.c') or fixed.get(p + '.cpp')
        if want:
            narrowed = [c for c in cands if c[1].parent.parent.name == want]
            if not narrowed:
                print(f'  {p}: pinned to {want} but no candidate there - FAILING rather than choosing another')
                sys.exit(1)
            cands = narrowed

        if not cands:
            continue
        exp, hu = max(cands)
        shutil.copy2(hu, dst / 'modules' / (p + hu.suffix))
        d = dst / 'module-src' / hu.parent.parent.name
        d.mkdir(parents=True, exist_ok=True)
        for f in hu.parent.iterdir():
            if f.is_file():
                shutil.copy2(f, d / f.name)
        chosen.append((p, exp, hu.parent.parent.name, hu.suffix.lstrip('.')))
        if want and hu.parent.parent.name != want:
            print(f'  {p}: resolved {hu.parent.parent.name}, expected {want} - FAILING')
            sys.exit(1)

    (dst / 'module-objects.txt').write_text(''.join(p + '.o\n' for p, _, _, _ in chosen))
    (dst / 'module-dirs.txt').write_text(''.join(f'{p}.{ext} {d}\n' for p, _, d, ext in chosen))
    print(f'resolved {len(chosen)} modules')
    return 0


if __name__ == '__main__':
    sys.exit(main())
