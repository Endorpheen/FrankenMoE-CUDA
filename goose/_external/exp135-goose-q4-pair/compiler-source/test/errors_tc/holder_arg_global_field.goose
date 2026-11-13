// error: cannot call prepare, which may shrink cfg while view is still used
// A callee replacing the array its struct parameter refers to replaces the
// global the caller's struct refers to (§5.1).
var cfg: u8[>..] = ['c', 'f', 'g'];
struct S { text: u8[>..]&, refs: (i64&)[>..] }
fn prepare(s: S&) { s.text = ['x']; }
fn main() {
    let view: const u8[:] = cfg[..];
    var s = S { text: cfg, refs: [] };
    prepare(s);
    print(view);
}
