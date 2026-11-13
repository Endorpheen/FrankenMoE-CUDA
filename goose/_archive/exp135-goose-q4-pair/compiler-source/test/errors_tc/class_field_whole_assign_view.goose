// error: cannot call fill, which may shrink text while s is still used: a reference into it was stored there
// A holder whose references can point into the array a callee replaces
// through its reference field, and do, keeps the caller from using it
// after the call (§5.1).
struct S { text: u8[>..]&, refs: (u8&)[>..] }

fn fill(s: S&) { s.text = ['a', 'b']; }

fn make(text: u8[>..]&) -> i64 {
    var s = S { text: text, refs: [] };
    s.refs.push(text[0]);
    fill(s);
    return s.refs.len;
}

fn main() {
    var t: u8[>..] = ['x'];
    print(make(t));
}
