// error: cannot call run, which shrinks pool while s is still used: a reference into it was stored there
struct S { pool: i64[>..]&, refs: (i64&)[>..] }
// What a callee stores into its emptied struct parameter before it shrinks
// the array the reference points into is still there at the shrink (§5.1).
fn run(s: S&, q: i64[>..]&) {
    s.refs = [];
    s.refs.push(q[0]);
    q.clear();
}
fn main() {
    var pool: i64[>..] = [1];
    var s = S { pool: pool, refs: [] };
    run(s, pool);
    print(s.refs.len);
}
