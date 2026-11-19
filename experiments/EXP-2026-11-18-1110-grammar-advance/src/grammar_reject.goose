// Composed entry: expand the input stack with the automaton, then reject across the
// resulting stacks. This is the shape the sampler filter needs.
fn g_grammar_reject(etype: i64[:], eval: i64[:], roff: i64[:], rlen: i64[:],
                    seed: i64[:], seedd: i64,
                    cand_cp: i64[:], cp_len: i64[:], pval: i64[:], pnr: i64[:],
                    cand_id: i64[:], ncand: i64,
                    wp: i64[:], wpd: i64[:], stacks: i64[:], stackd: i64[:],
                    scratch: i64[:], tmp_rej: i64[:], tmp_cur: i64[:], rowbuf: i64[:],
                    rej: i64[:], maxs: i64, maxd: i64, maxcp: i64) -> i64 {
    let n = g_advance(etype, eval, roff, rlen, seed, seedd, wp, wpd, stacks, stackd, 0, maxs, maxd);
    if n <= 0 {
        return -1;
    }
    return g_reject_multi(etype, eval, roff, rlen, stacks, stackd, n,
                          cand_cp, cp_len, pval, pnr, cand_id, rej, scratch, tmp_rej, tmp_cur,
                          rowbuf, ncand, maxd, maxcp);
}

fn main() {
    let et: i64[8] = [3, 0, 0, 0, 0, 0, 0, 0];
    let ev: i64[8] = [97, 0, 0, 0, 0, 0, 0, 0];
    let ro: i64[1] = [0];
    let rl: i64[1] = [2];
    let seed: i64[4] = [0, 0, 0, 0];
    let cp: i64[2] = [97, 0];
    let cpl: i64[1] = [1];
    let pv: i64[1] = [0];
    let pn: i64[1] = [0];
    let cid: i64[1] = [0];
    let wp: i64[16] = [0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0];
    let wpd: i64[4] = [0,0,0,0];
    let st: i64[16] = [0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0];
    let std: i64[4] = [0,0,0,0];
    let sc: i64[8] = [0,0,0,0,0,0,0,0];
    let tr: i64[1] = [0];
    let tc: i64[1] = [0];
    let rb: i64[4] = [0,0,0,0];
    let rej: i64[1] = [0];
    let r = g_grammar_reject(et, ev, ro, rl, seed, 1, cp, cpl, pv, pn, cid, 1,
                             wp, wpd, st, std, sc, tr, tc, rb, rej, 4, 4, 2);
    if r < 0 {
        return;
    }
}
