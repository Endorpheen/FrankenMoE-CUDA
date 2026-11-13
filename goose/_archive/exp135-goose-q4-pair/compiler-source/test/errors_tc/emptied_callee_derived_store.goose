// error: cannot call run, which may shrink pool while s is still used: a reference into it was stored there
struct S { pool: i64[>..]&, refs: (i64&)[>..] }
// A view a callee takes through its struct parameter's reference field and
// stores back into the emptied part, a store no event records, fills it
// again before the shrink (§5.1).
fn run(s: S&) {
    s.refs = [];
    s.refs.push(s.pool[0]);
    s.pool = [];
}
fn main() {
    var pool: i64[>..] = [1];
    var s = S { pool: pool, refs: [] };
    run(s);
    print(s.refs.len);
}
