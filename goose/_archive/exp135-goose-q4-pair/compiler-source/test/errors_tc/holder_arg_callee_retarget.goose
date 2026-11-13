// error: cannot call retarget, which may shrink cfg while view is still used
// What a callee stores into its struct parameter before replacing the array
// a field refers to is among the arrays the caller's struct may lead to
// (§5.1).
var cfg: u8[>..] = ['c', 'f', 'g'];
struct S { text: u8[>..]&, refs: (i64&)[>..] }
fn retarget(s: S&) {
    s.text .= cfg;
    s.text = ['x'];
}
fn main() {
    let view: const u8[:] = cfg[..];
    var text: u8[>..] = [];
    var s = S { text: text, refs: [] };
    retarget(s);
    print(view);
}
