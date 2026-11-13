// error: cannot assign cfg (which s.text may point at) while v is still used
// Inside the callee, the array a reference read out of its parameter's
// storage points at may be any its callers stored there, a global among
// them, which a view of the callee's own still used rules out (§5.1).
var cfg: u8[>..] = ['c', 'f', 'g'];
struct S { text: u8[>..]&, refs: (i64&)[>..] }
fn prepare(s: S&) {
    let v: const u8[:] = cfg[..];
    s.text = ['x'];
    print(v);
}
fn main() {
    var text: u8[>..] = [];
    var s = S { text: text, refs: [] };
    prepare(s);
}
