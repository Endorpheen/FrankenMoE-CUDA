// An array the loop's body declares is made after the loop's sequence and
// gone before it, so nothing the sequence holds can refer into it: the body
// may shrink it, whatever the sequence's elements refer into (§5.1, §9.2).
import std;

fn refill(buf: u8[>..]&, text: const u8[:]) {
    buf.clear();
    buf.append(text);
}

struct Pair { a: const u8[:], b: const u8[:] }

fn main() {
    // A literal of static strings.
    for s in ["a", "b"] {
        var buf: u8[>..] = [];
        refill(buf, s);
        print(buf);
    }
    // Slices into an outer array.
    var outer: u8[>..] = [];
    outer.append("xyz");
    for s in [outer[..1], outer[1..]] {
        var mine: u8[>..] = [];
        mine.append(s);
        mine.append(s);
        mine.pop();
        refill(mine, s);
        print(mine, " ", s);
    }
    // A shrink in a nested block of the body.
    for p in [Pair { a: "p", b: "q" }] {
        var acc: u8[>..] = [];
        if p.a.len > 0 {
            refill(acc, p.a);
            refill(acc, p.b);
        }
        print(acc);
    }
}
