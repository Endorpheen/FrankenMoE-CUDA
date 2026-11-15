#!/usr/bin/env python3
# Mutation run for P9-04 (router hook, I/O-compute overlap, row source).
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
A = S / 'source' / 'hook_rules.goose'
B = S / 'source' / 'overlap_rules.goose'
C = S / 'source' / 'row_select.goose'
ORIG = {p: p.read_text() for p in (A, B, C)}


def rep(p, old, new):
    t = ORIG[p]
    if t.count(old) != 1:
        raise AssertionError('anchor count %d in %s: %r' % (t.count(old), p.name, old[:70]))
    return (p, t.replace(old, new))


# ---- half 1: name matching, isolation, the call order, the wave decisions ----
def m_tail_ceiling(t):
    # a 7-digit layer suffix past the ceiling is accepted instead of refused
    return rep(A, '''        if il > RH_TAIL_MAX {
            return -1;
        }''', '''        if il > RH_TAIL_MAX * 10 {
            return -1;
        }''')


def m_weights_dash(t):
    # the '-' after a weight-chain name is no longer required, so any name that
    # merely PREFIXES the chain matches it
    return rep(A, '''        if rh_prefix(name, lit) != 0 && rh_at(name, lit.len) == '-' {
            let il: i64 = rh_tail(name, lit.len + 1);''',
               '''        if rh_prefix(name, lit) != 0 {
            let il: i64 = rh_tail(name, lit.len);''')


def m_weights_iso_always(t):
    # every weight node of every layer is isolated, learned terminal or not
    return rep(A, '''    if term[wl] == variant {
        return 1; // the known terminal: the one worth a barrier
    }
    return 0;''', '''    if term[wl] == variant {
        return 1; // the known terminal: the one worth a barrier
    }
    return 1;''')


def m_plan_second_collect(t):
    # route-ahead's collect after the load is dropped: a result the previous
    # token never collected is committed by this one
    return rep(A, '''        if f[RH_F_RA] != 0 && f[RH_F_PHASE] == 1 {
            k = rh_put(out, k, RH_OP_RA_COLLECT);
        }
    }
    if f[RH_F_PREDLOG] != 0 && f[RH_F_PHASE] == 1 {''',
               '''    }
    if f[RH_F_PREDLOG] != 0 && f[RH_F_PHASE] == 1 {''')


def m_worker_growth(t):
    # a lane parked after wave one is not woken for a batch that grew in the
    # generation it already drained
    return rep(B, '''    if batch_gen > seen || next_idx < batch_njobs || spec_next < spec_n {''',
               '''    if batch_gen > seen || spec_next < spec_n {''')


def m_two_wave_load_all(t):
    # two-wave publishing is used for load_all too, where nothing is waited on
    return rep(B, '''    if cfg_two_wave != 0 && load_all == 0 && p0 >= 0 {''',
               '''    if cfg_two_wave != 0 && p0 >= 0 {''')


# ---- half 2: publication, the stall union, the gate, the row-source verdict ----
def m_publish_always_notify(t):
    # every finished slice takes the mutex and wakes every blocked compute thread
    return rep(B, '''    if waiters != 0 {
        return 2; // store, then lock + notify
    }
    return 1; // store only''', '''    if waiters == 0 {
        return 2; // store, then lock + notify
    }
    return 1; // store only''')


def m_stall_every_exit(t):
    # the union accumulates on every exit instead of only on the 1 -> 0 boundary,
    # so overlapping waits are counted more than once
    return rep(B, '''    st[0] = st[0] - 1;
    if st[0] == 0 {
        st[2] = st[2] + (now_ns - st[1]);
        st[1] = 0;
        return 1; // closed
    }''', '''    st[0] = st[0] - 1;
    if st[0] >= 0 {
        st[2] = st[2] + (now_ns - st[1]);
        st[1] = now_ns;
        return 1; // closed
    }''')


def m_gate_order(t):
    # the scope fault is checked after the readiness test, so a layer that was
    # never staged is waited for instead of aborting
    return rep(B, '''    if ours == 0 {
        return 0;
    }
    if fault != 0 {
        return 1;
    }
    if flag_gen == want {
        return 2;
    }''', '''    if ours == 0 {
        return 0;
    }
    if flag_gen == want {
        return 2;
    }
    if fault != 0 {
        return 1;
    }''')
    # the two orders differ only when the flag already matches AND the layer is
    # wrong; the gate's grid covers that pair.


def m_index_nine_hops(t):
    # the index walk follows nine hops instead of eight
    return rep(C, '''    var i: i64 = 0;
    while i < 8 {''', '''    var i: i64 = 0;
    while i < 9 {''')


def m_ref_any_slot(t):
    # a row gather at ANY source slot qualifies the table, not only the table slot
    return rep(C, '''    if get_rows != 0 && slot == 0 && idx_ok != 0 {''',
               '''    if get_rows != 0 && idx_ok != 0 {''')


def m_shard_silent(t):
    # a table pointing at a shard that does not exist is skipped instead of
    # aborting the whole policy
    return rep(C, '''    if file_ok == 0 {
        return RS_FATAL_SHARD;
    }''', '''    if file_ok == 0 {
        return RS_SKIP_SHAPE;
    }''')


MUTS = [
    ('rh_tail_ceiling', m_tail_ceiling),
    ('rh_weights_dash', m_weights_dash),
    ('rh_weights_iso_always', m_weights_iso_always),
    ('rh_plan_second_collect', m_plan_second_collect),
    ('ov_worker_growth', m_worker_growth),
    ('ov_two_wave_load_all', m_two_wave_load_all),
    ('ov_publish_always_notify', m_publish_always_notify),
    ('ov_stall_every_exit', m_stall_every_exit),
    ('ov_gate_order', m_gate_order),
    ('rs_index_nine_hops', m_index_nine_hops),
    ('rs_ref_any_slot', m_ref_any_slot),
    ('rs_shard_silent', m_shard_silent),
]


def restore():
    for p, t in ORIG.items():
        p.write_text(t)


def gate_lines(out):
    return [l for l in out.splitlines() if l.startswith('FG_P9_04')]


def run_gate():
    r = subprocess.run(['bash', str(S / 'tests' / 'build.sh'), 'mutate'],
                       capture_output=True, text=True)
    return r.returncode, gate_lines(r.stdout + r.stderr)


def main():
    lo = int(sys.argv[1]) if len(sys.argv) > 1 else 0
    hi = int(sys.argv[2]) if len(sys.argv) > 2 else len(MUTS)
    restore()
    _, base = run_gate()
    print('baseline rc=0 lines=%s' % base[-1])
    caught = 0
    try:
        for i in range(lo, hi):
            name, fn = MUTS[i]
            p, text = fn(ORIG)
            p.write_text(text)
            rc, out = run_gate()
            ok = (rc != 0) or (out != base)
            caught += 1 if ok else 0
            last = out[-1] if out else '<no gate line: build failure>'
            print('%2d %-28s %s rc=%d %s' % (i + 1, name, 'CAUGHT' if ok else 'MISSED', rc, last))
            restore()
    finally:
        restore()
    print('mutations %d/%d caught' % (caught, hi - lo))
    return 0 if caught == hi - lo else 1


if __name__ == '__main__':
    sys.exit(main())
