// error: cannot call wrap, which may shrink cfg while view is still used
// A local struct built from a struct parameter's reference fields leads to
// what those lead to: a callee replacing the array one refers to replaces
// what the caller's caller's struct refers to, here a global (§5.1).
var cfg: u8[>..] = ['c', 'f', 'g'];
struct S { text: u8[>..]&, refs: (i64&)[>..] }
fn prepare(s: S&) { s.text = ['x']; }
fn wrap(s: S&) {
    var t = S { text: s.text, refs: [] };
    prepare(t);
}
fn main() {
    let view: const u8[:] = cfg[..];
    var s = S { text: cfg, refs: [] };
    wrap(s);
    print(view);
}
