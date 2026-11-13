// error: cannot clear outer: the for loop at
// error: whose elements may refer into outer
// An array declared before the loop may be what its sequence's elements
// view, though one the body declares is not (lifetimes/for_body_shrinks).
import std;
fn main() {
    var outer: u8[>..] = [];
    outer.append("xyz");
    for s in [outer[..1], outer[1..]] {
        var mine: u8[>..] = [];
        mine.append(s);
        mine.clear();
        outer.clear();
        print(s);
    }
}
