// error: cannot assign pool while refs is in scope: a reference into it is stored there at
// A holder emptied on some paths only still holds, on the others, what the
// previous iteration stored there (§5.1).
fn main() {
    var pool: i64[>..] = [];
    var refs: (i64&)[>..] = [];
    for i in 3 {
        if i > 0 { refs = []; }
        pool = [];
        refs.push(pool.push(i));
    }
    print(refs.len);
}
