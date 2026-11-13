// error: cannot assign pool while r2 is still used
// Emptying a holder empties no copy of it made before (§5.1).
fn main() {
    var pool: i64[>..] = [1];
    var refs: (i64&)[>..] = [];
    refs.push(pool[0]);
    var r2 = copy(refs);
    refs = [];
    pool = [];
    print(r2.len);
}
