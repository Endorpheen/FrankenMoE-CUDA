#!/usr/bin/env python3
# Mutation run for P9-03 (dense weights and expert streaming/LRU sources).
#
# Each mutation is one semantic change to a slice of the port. The gate is
# rebuilt at -O0 and run; any difference from the gate's own baseline output (a
# build failure counts too) means the mutation was caught. The source is
# restored after every mutation and once at the end.
#
#   python3 tests/mutate.py 0 6    # half 1
#   python3 tests/mutate.py 6 12   # half 2
from pathlib import Path
import subprocess
import sys

S = Path(__file__).resolve().parent.parent
A = S / 'source' / 'expert_lru.goose'
B = S / 'source' / 'dense_place.goose'
C = S / 'source' / 'stream_plan.goose'
ORIG = {p: p.read_text() for p in (A, B, C)}


def rep(p, old, new):
    t = ORIG[p]
    if t.count(old) != 1:
        raise AssertionError('anchor count %d in %s: %r' % (t.count(old), p.name, old[:70]))
    return (p, t.replace(old, new))


# ---- half 1: the LRU list, the load eviction, the ranges, the avail guard ----
def m_push_back(t):
    # a speculative or committed entry enters the list HOT instead of cold
    return rep(A, '''fn xl_back(prev: i64[:], next: i64[:], st: i64[:], id: i64) {
    next[id] = XL_NIL;
    prev[id] = st[1];
    if st[1] != XL_NIL {
        next[st[1]] = id;
    } else {
        st[0] = id;
    }
    st[1] = id;
}''', '''fn xl_back(prev: i64[:], next: i64[:], st: i64[:], id: i64) {
    xl_front(prev, next, st, id);
}''')


def m_hit_no_promote(t):
    # a hit no longer moves the entry to the MRU end
    return rep(A, '''        if promote != 0 {
            xl_unlink(prev, next, st, id);
            xl_front(prev, next, st, id);
        }''',
               '''        if promote > 1 {
            xl_unlink(prev, next, st, id);
            xl_front(prev, next, st, id);
        }''')


def m_evict_load_no_gen(t):
    # the post-load eviction ignores the generation stamp
    return rep(A, '''    while sacc[0] > cache_max && st[1] != XL_NIL && cstamp[st[1]] != gen {''',
               '''    while sacc[0] > cache_max && st[1] != XL_NIL {''')


def m_worst_no_clamp(t):
    # a top_k wider than the bank is not clamped to n_expert
    return rep(A, '''    var k: i64 = top_k;

    if k > n_expert {
        k = n_expert;
    }''',
               '''    var k: i64 = top_k;''')


def m_ranges_pos(t):
    # an overlapping expert range can move the cursor backwards
    return rep(B, '''        if eb[i] > pos {
            pos = eb[i];
        }''',
               '''        pos = eb[i];''')


def m_classify_avail(t):
    # hold_back runs even when MemAvailable is unmeasured (avail == 0)
    return rep(B, '''        if avail != 0 && size[i] > avail {''',
               '''        if size[i] > avail {''')


# ---- half 2: subtraction, the verdict order, the slab plan, the window ------
def m_sub_head(t):
    # a zero-length left remainder is emitted
    return rep(B, '''            if ra[i] < a {''',
               '''            if ra[i] <= a {''')


def m_verdict_order(t):
    # the pinned capability is checked before the empty resident set
    return rep(B, '''    if mode == DW_ANON || mode == DW_PINNED {
        if n_resident == 0 {
            return 3;
        }
        if mode == DW_PINNED && pinned_ok == 0 {
            return 0;
        }''',
               '''    if mode == DW_ANON || mode == DW_PINNED {
        if mode == DW_PINNED && pinned_ok == 0 {
            return 0;
        }
        if n_resident == 0 {
            return 3;
        }''')


def m_straddle(t):
    # a row's covered span loses its last byte, so a row ending on a slab
    # boundary no longer stamps the next slab
    return rep(C, '''            var b: i64 = a + row_bytes;''',
               '''            var b: i64 = a + row_bytes - 1;''')


def m_evict_stamp(t):
    # a slab needed by the gather in flight becomes the first eviction target
    return rep(C, '''        if stamp[s] == gen {
            keep[k] = s;
            k = k + 1;
        } else {''',
               '''        if stamp[s] != gen {
            keep[k] = s;
            k = k + 1;
        } else {''')


def m_budget_floor(t):
    # a budget below one slab is kept, so every gather evicts what it read
    return rep(C, '''    if b < slab {
        b = slab;
    }''',
               '''    b = b;''')


def m_materialize(t):
    # a table exactly the size of the window is refused
    return rep(C, '''    if size > budget {
        return 0;
    }''',
               '''    if size >= budget {
        return 0;
    }''')


M = [
    # ---- half 1 -----------------------------------------------------------
    ('a speculative entry enters the LRU hot', m_push_back),
    ('a hit no longer promotes its entry', m_hit_no_promote),
    ('the post-load eviction ignores the generation stamp', m_evict_load_no_gen),
    ('worst_cycle is not clamped at n_expert', m_worst_no_clamp),
    ('an overlapping expert range moves the dense cursor backwards', m_ranges_pos),
    ('hold_back runs without a measurable MemAvailable', m_classify_avail),
    # ---- half 2 -----------------------------------------------------------
    ('subtract_range emits a zero-length head remainder', m_sub_head),
    ('the pinned capability is checked before the empty set', m_verdict_order),
    ('a row ending on a slab boundary loses the next slab', m_straddle),
    ('a stamped slab is the first eviction target', m_evict_stamp),
    ('a budget below one slab is accepted', m_budget_floor),
    ('a table the size of the window is refused', m_materialize),
]


def build_and_run():
    r = subprocess.run(['bash', str(S / 'tests' / 'build.sh'), 'mutate'], capture_output=True,
                       text=True, timeout=900, errors='replace')
    total = None
    for line in r.stdout.splitlines():
        if line.startswith('FG_P9_03_STREAM'):
            total = line
    if total is not None:
        return r.stdout, total
    tail = (r.stderr or r.stdout).strip().splitlines()
    return None, 'build failed: ' + (tail[-1][:140] if tail else 'no output')


def main():
    lo = int(sys.argv[1]) if len(sys.argv) > 1 else 0
    hi = int(sys.argv[2]) if len(sys.argv) > 2 else len(M)
    base, base_line = build_and_run()
    if base is None:
        print('baseline does not build: ' + base_line)
        return 2
    caught = 0
    missed = []
    try:
        for idx, (label, fn) in enumerate(M[lo:hi], start=lo):
            try:
                target, text = fn(ORIG)
            except AssertionError as e:
                print('[skip] %d %s: %s' % (idx, label, e))
                continue
            target.write_text(text)
            out, line = build_and_run()
            ok = out is None or out != base
            caught += ok
            if not ok:
                missed.append(idx)
            print('[%s] %d %s: %s' % ('caught' if ok else 'MISSED', idx, label, line))
            target.write_text(ORIG[target])
    finally:
        for p in (A, B, C):
            p.write_text(ORIG[p])
        out, line = build_and_run()
        print('baseline after restore: %s' % line)
    print('mutations %d/%d caught (range %d:%d) missed=%s' % (caught, hi - lo, lo, hi, missed))
    return 0 if caught == hi - lo else 1


if __name__ == '__main__':
    sys.exit(main())
