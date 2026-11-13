// A holder passed by value to a function value holds what the argument
// held, as a holder variable bound from it would (§9.5): a reference read
// out of it is rooted at the array the caller stored there, not at
// whichever globals of its type the program happens to declare.
struct Cx { scratch: u8[>..]& }

fn mk(scratch: u8[>..]&) -> u8[] { scratch.push('a'); return "x"; }

fn push_one(v: u8[][>..]&, scratch: u8[>..]&) { v.push(mk(scratch)); }

fn with_cx<H>() {
    var scratch: u8[>..] = [];
    var cx = Cx { scratch: scratch };
    H(cx);
    print(scratch.len);
}

var first: u8[>..] = [];
var spill: u8[>..] = [];

fn main() {
    with_cx() { cx =>
        var v: u8[][>..] = [];
        push_one(v, cx.scratch);
        push_one(v, cx.scratch);
        for e in v { print(e); }
    };
    first.push('f');
    spill.push('s');
    print(first.len + spill.len);
}
