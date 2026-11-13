// error: cannot call run, which may shrink pool while r is still used
struct S { pool: i64[>..]&, refs: (i64&)[>..] }
// Emptying the holder leaves a reference the caller took out of it before
// pointing into the pool the callee shrinks (§5.1).
fn run(s: S&, i: i64) {
    s.refs = [];
    s.pool = [];
    s.refs.push(s.pool.push(i));
}
fn main() {
    var pool: i64[>..] = [];
    var s = S { pool: pool, refs: [] };
    run(s, 0);
    let r: i64& = s.refs[0];
    run(s, 1);
    print(r);
}
