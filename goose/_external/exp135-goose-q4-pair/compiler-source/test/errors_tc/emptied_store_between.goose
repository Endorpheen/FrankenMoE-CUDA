// error: cannot assign pool while refs is still used: a reference into it was stored there
// A store after the holder was emptied reaches a later shrink (§5.1).
fn main() {
    var pool: i64[>..] = [1];
    var refs: (i64&)[>..] = [];
    refs = [];
    refs.push(pool[0]);
    pool = [];
    print(refs.len);
}
