// error: result 1 of the call is a reference to a value that is not fixed-size, which is not copied implicitly
// A reference result's pointee is copied into a target only as copy(...)
// says (§4.1).
fn first(xs: i64[>..]&) -> i64[>..]&, bool { return xs, true; }
fn main() {
    var xs: i64[>..] = [1];
    var ys: i64[>..] = [];
    var ok = false;
    ys, ok = first(xs);
    print(ys, ok);
}
