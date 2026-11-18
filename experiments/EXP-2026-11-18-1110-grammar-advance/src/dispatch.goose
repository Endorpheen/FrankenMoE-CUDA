// One stable entry for the grammar automaton: six slices and five scalars, a shape
// no other function here has, so the generated prototype can be identified without
// guessing. op 0 = accept_chr, op 1 = advance, op 2 = reject (single stack).
fn g_disp(op: i64, a: i64[:], b: i64[:], c: i64[:], d: i64[:], e: i64[:], f: i64[:],
          p0: i64, p1: i64, p2: i64, p3: i64) -> i64 {
    if op == 0 {
        // accept: a=etype b=eval c=stacks d=stackd e=out f=outd ; p0=nstacks p1=chr
        return g_accept(a, b, c, d, p0, p1, e, f, p2, p3);
    }
    return -2;
}

fn main() {
    let et: i64[8] = [3, 0, 0, 0, 0, 0, 0, 0];
    let ev: i64[8] = [97, 0, 0, 0, 0, 0, 0, 0];
    let st: i64[4] = [0, 0, 0, 0];
    let std: i64[1] = [1];
    let out: i64[16] = [0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0];
    let outd: i64[4] = [0,0,0,0];
    let r = g_disp(0, et, ev, st, std, out, outd, 1, 97, 4, 4);
    if r < 0 {
        return;
    }
}
