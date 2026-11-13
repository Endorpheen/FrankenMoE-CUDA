// error: returning a reference rooted in arr, which dies with this function
// The block binds r to what each hands it, a reference into each's
// parameter, which each's call makes a reference into arr: r points into
// arr, which get's return outlives (§9.2).
fn each<F>(xs: i64[:]) { F(&xs[0]); }
var K: i64 = 7;
fn get() -> i64& {
    var arr: i64[>..] = [];
    arr.push(1);
    var r: i64&? = null;
    each(arr[..]) { p => r .= p; };
    guard r else { return K; }
    return r;
}
fn main() { print(get()); }
