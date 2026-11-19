// reject across several stacks: a candidate is rejected only when every stack
// rejects it, which is the native intersection of per-stack reject sets.
// stacks is nstacks rows of maxd element indexes; stackd holds each depth.
fn g_reject_multi(etype: i64[:], eval: i64[:], roff: i64[:], rlen: i64[:],
                  stacks: i64[:], stackd: i64[:], nstacks: i64,
                  cand_cp: i64[:], cp_len: i64[:], pval: i64[:], pnr: i64[:], cand_id: i64[:],
                  rej: i64[:], scratch: i64[:], tmp_rej: i64[:], tmp_cur: i64[:],
                  rowbuf: i64[:],
                  ncand: i64, maxd: i64, maxcp: i64) -> i64 {
    var i: i64 = 0;
    while i < ncand {
        rej[i] = 1;
        i = i + 1;
    }
    var s: i64 = 0;
    while s < nstacks {
        // reset the per-candidate cursors and evaluate one stack
        i = 0;
        while i < ncand {
            tmp_cur[i] = 0;
            tmp_rej[i] = 0;
            i = i + 1;
        }
        var d: i64 = 0;
        while d < stackd[s] {
            rowbuf[d] = stacks[s * maxd + d];
            d = d + 1;
        }
        let ignored = g_reject(etype, eval, roff, rlen,
                               rowbuf, stackd[s],
                               cand_cp, cp_len, tmp_cur, tmp_rej, ncand,
                               pval, pnr, cand_id, scratch, maxd, maxcp);
        i = 0;
        while i < ncand {
            if tmp_rej[i] == 0 {
                rej[i] = 0;
            }
            i = i + 1;
        }
        s = s + 1;
    }
    var nrej: i64 = 0;
    i = 0;
    while i < ncand {
        nrej = nrej + rej[i];
        i = i + 1;
    }
    return nrej;
}

fn main() {
    let et: i64[8] = [3, 0, 3, 0, 0, 0, 0, 0];
    let ev: i64[8] = [97, 0, 98, 0, 0, 0, 0, 0];
    let ro: i64[2] = [0, 0];
    let rl: i64[2] = [4, 0];
    let stk: i64[16] = [0,0,0,0,0,0,0,0, 2,0,0,0,0,0,0,0];
    let std: i64[2] = [1, 1];
    let cp: i64[6] = [97, 98, 122, 0, 0, 0];
    let cpl: i64[3] = [1, 1, 1];
    let pv: i64[3] = [0, 0, 0];
    let pn: i64[3] = [0, 0, 0];
    let rej: i64[3] = [0, 0, 0];
    let sc: i64[8] = [0,0,0,0,0,0,0,0];
    let tr: i64[3] = [0, 0, 0];
    let tc: i64[3] = [0, 0, 0];
    let rb: i64[8] = [0,0,0,0,0,0,0,0];
    let cid: i64[3] = [0, 0, 0];
    let r = g_reject_multi(et, ev, ro, rl, stk, std, 2, cp, cpl, pv, pn, cid, rej, sc, tr, tc, rb, 3, 8, 2);
    if r < 0 {
        return;
    }
}
