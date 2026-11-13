// error: cannot call drive, which may shrink cfg while view is still used
// A function passing its struct parameter on to a callee replacing an array
// it refers to records that for its own callers, which judge it by what
// their struct refers to (§5.1).
var cfg: u8[>..] = ['c', 'f', 'g'];
struct S { text: u8[>..]&, refs: (i64&)[>..] }
fn prepare(s: S&) { s.text = ['x']; }
fn drive(s: S&) { prepare(s); }
fn main() {
    let view: const u8[:] = cfg[..];
    var s = S { text: cfg, refs: [] };
    drive(s);
    print(view);
}
