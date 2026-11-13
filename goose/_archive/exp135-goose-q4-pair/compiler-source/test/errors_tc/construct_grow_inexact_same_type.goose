// error: cannot call mk, which grows scratch: the element pushed into v is still under construction
// scratch is rooted inexactly, at first or spill, so it may be any array of
// its type they bound (§5.1): of v's type, it may be the array v pushes
// into, which mk would grow under the element (§1.3(4)). Here it is.
fn mk(scratch: u8[][>..]&) -> u8[] { scratch.push("a"); return "x"; }
fn push_one(v: u8[][>..]&, scratch: u8[][>..]&) { v.push(mk(scratch)); }
var first: u8[][>..] = [];
var spill: u8[][>..] = [];
fn main() {
    let c = first.len == 0;
    push_one(first, if c { first } else { spill });
    print(first.len);
}
