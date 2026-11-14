#!/usr/bin/env python3
# Mutation run for P6-11 (speculative decoding decisions).
#
# Each mutation is one semantic change to source/sdp.goose. The gate is rebuilt with
# tests/build.sh mutate (module at -O0) and run; any difference from the gate's own baseline
# output counts as caught, and a build failure counts too. The source is restored after every
# mutation and once at the end.
#
#   python3 tests/mutate.py 0 6    # half 1: the draft source and the accept decision
#   python3 tests/mutate.py 6 12   # half 2: the MTP draft state and the budget
from pathlib import Path
import subprocess
import sys

S = Path(__file__).resolve().parent.parent
SRC = S / 'source' / 'sdp.goose'
ORIG = SRC.read_text()


def rep(t, old, new):
    if t.count(old) != 1:
        raise AssertionError('anchor count %d: %r' % (t.count(old), old[:70]))
    return t.replace(old, new)


# ---- half 1: the n-gram proposal and the accept decision -------------------
def m_tie_recent(t):
    # an equal-length match found earlier replaces the more recent one
    return rep(t, '            if m > best_m {', '            if m >= best_m {')


def m_overlap(t):
    # the self-predicting last token of a run is dropped
    return rep(t, '    let avail: i64 = end - best_j; // tokens that followed the match, up to the corpus end',
               '    let avail: i64 = end - best_j - 1 as i64; // tokens that followed the match')


def m_min_gate(t):
    # the gate now needs strictly more than min_match, so an exact match stops drafting
    return rep(t, '    if best_m < minm {\n        return 0 as i64;\n    }',
               '    if best_m <= minm {\n        return 0 as i64;\n    }')


def m_accept_eog(t):
    # an accepted end-of-generation token is emitted like any other
    return rep(t, '            if eog[i] != 0 as i32 {\n                hit = 1 as i64; // end-of-generation is never emitted\n                stop = i;\n                break;\n            }',
               '            if eog[i] != 0 as i32 {\n                hit = 0 as i64;\n                stop = i;\n                break;\n            }')


def m_accept_mismatch(t):
    # a mismatch no longer stops the loop: every draft is confirmed
    return rep(t, '        if target[i] != drafts[i] {\n            stop = i;\n            break;\n        }',
               '        if target[i] != drafts[i] {\n            if i >= drafts.len {\n                stop = i;\n                break;\n            }\n        }')


def m_emit_bound(t):
    # the emit bound is a plain count of confirmed tokens
    return rep(t, '    let left: i64 = n_predict - n_gen;\n    if confirmed < left {\n        return confirmed;\n    }\n    return left;',
               '    return confirmed;')


# ---- half 2: the MTP draft state and the budget ----------------------------
def m_pmin_boundary(t):
    # a token exactly at the floor is rejected
    return rep(t, '        if conf[i] < p_min {', '        if conf[i] <= p_min {')


def m_nmax_extra(t):
    # one more draft than n_max is collected
    return rep(t, '        if n_max <= n {\n            break; // n_max reached: the loop stops right after the push\n        }',
               '        if n_max + 1 as i64 <= n {\n            break;\n        }')


def m_seed_row(t):
    # the accept seed reads one row past the accepted prefix
    return rep(t, '        if n_rows - 1 as i64 < seed {\n            seed = n_rows - 1 as i64; // std::min(n_accepted, n_rows - 1)\n        }',
               '        if n_rows < seed {\n            seed = n_rows;\n        }')


def m_rewind_rows(t):
    # the rewind leaves the last drafted row behind
    return rep(t, '        removed = seq_pos_max - n_past + 1 as i64;',
               '        removed = seq_pos_max - n_past;')


def m_room(t):
    # the room leaves no space for the confirmed token itself
    return rep(t, '    let room: i64 = n_predict - n_gen - 1 as i64;',
               '    let room: i64 = n_predict - n_gen;')


def m_rollback_keep(t):
    # the kept position forgets the confirmed token
    return rep(t, '    var keep: i64 = n_past + 1 as i64 + n_acc;',
               '    var keep: i64 = n_past + n_acc;')


M = [
    # ---- half 1 -----------------------------------------------------------
    ('the most recent equal-length match loses', m_tie_recent),
    ('a run no longer predicts its own last token', m_overlap),
    ('an exact min_match stops drafting', m_min_gate),
    ('an accepted EOG is emitted', m_accept_eog),
    ('a mismatching draft is confirmed', m_accept_mismatch),
    ('the emit bound ignores the budget', m_emit_bound),
    # ---- half 2 -----------------------------------------------------------
    ('a token exactly at p_min is rejected', m_pmin_boundary),
    ('one draft past n_max is collected', m_nmax_extra),
    ('the accept seed reads past the prefix', m_seed_row),
    ('the rewind keeps the last drafted row', m_rewind_rows),
    ('the room forgets the confirmed token', m_room),
    ('the rollback keep forgets the confirmed token', m_rollback_keep),
]


def build_and_run():
    r = subprocess.run(['bash', str(S / 'tests' / 'build.sh'), 'mutate'], capture_output=True,
                       text=True, timeout=900, errors='replace')
    out = r.stdout
    for line in out.splitlines():
        if line.startswith('FG626_SPC cases='):
            return out, line
    tail = (r.stderr or out).strip().splitlines()
    return None, 'build failed: ' + (tail[-1][:120] if tail else 'no output')


def first_failure(out):
    for line in out.splitlines():
        if line.startswith('[FAIL] '):
            return line.split()[1].rstrip(':')
    return '?'


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
                SRC.write_text(fn(ORIG))
            except AssertionError as e:
                print('[skip] %d %s: %s' % (idx, label, e))
                SRC.write_text(ORIG)
                continue
            out, line = build_and_run()
            ok = out is None or out != base
            caught += ok
            if not ok:
                missed.append(idx)
            print('[%s] %d %s: %s (first: %s)' % ('caught' if ok else 'MISSED', idx, label,
                                                  line, first_failure(out) if out else 'build'))
            SRC.write_text(ORIG)
    finally:
        SRC.write_text(ORIG)
        out, line = build_and_run()
        print('baseline after restore: %s' % line)
    print('mutations %d/%d caught (range %d:%d) missed=%s' % (caught, hi - lo, lo, hi, missed))
    return 0 if caught == hi - lo else 1


if __name__ == '__main__':
    sys.exit(main())
