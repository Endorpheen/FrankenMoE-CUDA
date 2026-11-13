// error: cannot assign s while s is still used: what it refers to may hold a reference or slice into it
struct S { pool: i64[>..]&, refs: (i64&)[>..] }
// A struct parameter whose references are emptied on some paths only still
// holds the caller's, and its own earlier ones, on the others (§5.1).
fn run(s: S&, i: i64) {
    if i > 0 { s.refs = []; }
    s.pool = [];
    s.refs.push(s.pool.push(i));
}
fn main() {
    var pool: i64[>..] = [];
    var s = S { pool: pool, refs: [] };
    for i in 3 { run(s, i); }
    print(s.refs.len);
}
