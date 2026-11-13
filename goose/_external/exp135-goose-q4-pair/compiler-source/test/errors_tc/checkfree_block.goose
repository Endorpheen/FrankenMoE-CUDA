// error: checkfree fn each calls a block written in fn main, which is not checkfree
// A block runs inside the function calling it, as text of the one it was written in. (docs/design/index_types.md)
checkfree fn each<F>(xs: i32[:]) { for x in xs { F(x); } }
fn main() {
    var a: i32[>..] = [];
    a.push(1);
    var t = 0;
    each(a) { x => t += x; };
    print(t);
}
