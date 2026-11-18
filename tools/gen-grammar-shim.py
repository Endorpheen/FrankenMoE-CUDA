#!/usr/bin/env python3
"""Emit the grammar wrapper after build-sampler-chain.sh has produced the C.

Kept separate from the chain generator so that a failure here cannot disturb the
entries that already work. Selection is by the complete ordered type list, and the
script refuses to act unless exactly one prototype matches.
"""
import re, sys, os

SHAPE = (['gg_slice'] * 5 + ['int64_t'] + ['gg_slice'] * 5 + ['int64_t'] +
         ['gg_slice'] * 9 + ['int64_t'] * 4)

NAMES = ['s_etype', 's_eval', 's_roff', 's_rlen', 's_seed',
         'v_seedd',
         's_cand_cp', 's_cp_len', 's_pval', 's_pnr', 's_cand_id',
         'v_ncand',
         's_wp', 's_wpd', 's_stacks', 's_stackd', 's_scratch', 's_tmp_rej', 's_tmp_cur',
         's_rowbuf', 's_rej',
         'v_maxs', 'v_maxd', 'v_maxcp']

SHAPE_FIRST = ['gg_slice'] * 4 + ['int64_t'] + ['gg_slice'] * 2 + ['int64_t', 'int64_t']

def main(out):
    src = open(os.path.join(out, 'sampler_chain.c')).read()
    hits = []
    for m in re.finditer(r'static int64_t (gg_function_\d+)\(([^)]*)\);', src):
        params = [q.strip().rsplit(' ', 1)[0].replace('const ', '') for q in m.group(2).split(',')]
        if params[:-1] == SHAPE:
            hits.append((m.group(1), params))
    if len(hits) != 1:
        sys.exit('grammar shape matched %d prototypes, expected exactly one' % len(hits))
    func, _ = hits[0]

    first_hits = []
    for m in re.finditer(r'static int64_t (gg_function_\d+)\(([^)]*)\);', src):
        params = [q.strip().rsplit(' ', 1)[0].replace('const ', '') for q in m.group(2).split(',')]
        if params[:-1] == SHAPE_FIRST:
            first_hits.append(m.group(1))
    if len(first_hits) != 1:
        sys.exit('first-set shape matched %d prototypes' % len(first_hits))
    first = first_hits[0]

    ids = os.path.join(out, 'sampler_chain_ids.h')
    text = open(ids).read()
    if 'FG_GRAMMAR_REJECT' not in text:
        open(ids, 'a').write('#define FG_GRAMMAR_REJECT %s\n' % func)
    if 'FG_GRAMMAR_FIRST' not in text:
        open(ids, 'a').write('#define FG_GRAMMAR_FIRST %s\n' % first)

    proto = ('int64_t fg_grammar_reject_call(gg_slice s_etype, gg_slice s_eval, gg_slice s_roff,\n'
             '                                   gg_slice s_rlen, gg_slice s_seed, int64_t v_seedd,\n'
             '                                   gg_slice s_cand_cp, gg_slice s_cp_len, gg_slice s_pval,\n'
             '                                   gg_slice s_pnr, gg_slice s_cand_id, int64_t v_ncand,\n'
             '                                   gg_slice s_wp, gg_slice s_wpd, gg_slice s_stacks,\n'
             '                                   gg_slice s_stackd, gg_slice s_scratch, gg_slice s_tmp_rej,\n'
             '                                   gg_slice s_tmp_cur, gg_slice s_rowbuf, gg_slice s_rej,\n'
             '                                   int64_t v_maxs, int64_t v_maxd, int64_t v_maxcp)')
    body = ('{\n    return FG_GRAMMAR_REJECT(%s, 180, 0);\n}\n' % ', '.join(NAMES))
    # the wrapper joins the existing shim, because a second translation unit that
    # includes sampler_chain.c would duplicate every symbol
    shim = os.path.join(out, 'sampler_chain_shim.c')
    text = open(shim).read()
    if 'fg_grammar_first_set_call' not in text:
        open(shim, 'a').write('\nint64_t fg_grammar_first_set_call(gg_slice s_etype, gg_slice s_eval,'
                              ' gg_slice s_stacks, gg_slice s_stackd, int64_t v_nstacks,'
                              ' gg_slice s_lo, gg_slice s_hi, int64_t v_maxr) {\n'
                              '    return FG_GRAMMAR_FIRST(s_etype, s_eval, s_stacks, s_stackd, v_nstacks,'
                              ' s_lo, s_hi, v_maxr, 181, 0);\n}\n')
    if 'fg_grammar_reject_call' not in text:
        open(shim, 'a').write('\n' + proto + '\n' + body)
    print('grammar entry:', func)

if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else 'build/sampler-chain')
