// A callee replacing an array its struct parameter refers to shrinks one of
// the arrays the caller's struct refers to (§5.1): as the struct's record
// says, through each function taking the struct on by reference. A global
// array of the same type, which nothing stored into the struct, stays as
// it is, so a view of it may be used across the calls, and so may the
// references into a pool the struct also refers to, which cannot be the
// array replaced.
import std;

var cfg: u8[>..] = ['c', 'f', 'g'];

struct S { text: u8[>..]&, nodes: i64[>..]&, refs: (i64&)[>..] }

fn prepare(s: S&) {
    let t = str("text");
    s.text = copy(t);
}

fn twice(s: S&) {
    prepare(s);
    prepare(s);
}

fn drive<P, B>(s: B&) { P(s); }

fn main() {
    let view: const u8[:] = cfg[..];
    var text: u8[>..] = [];
    var nodes: i64[>..] = [1, 2];
    var s = S { text: text, nodes: nodes, refs: [] };
    s.refs.push(nodes[1]);
    prepare(s);
    twice(s);
    drive(s, prepare);
    print(view, " ", text, " ", s.refs[0]);
}
