// Loops over arrays and slices whose bodies call a recursive function walk a
// cursor through the elements (ForLoop::CgStmt). An empty sequence is never
// walked; continue and break leave the walk as they leave a counted loop; an
// element bound with & is the one in the array, written in place; elements
// behind a resizable's byte-addressed base step by their size like a
// slice's typed ones.

enum Tree { Leaf { v: i64 }, Node { kids: Tree[:] } }

recursive fn total(t: Tree) -> i64 {
    match t {
        Leaf l => { return l.v; },
        Node n => {
            var s = 0;
            for k in n.kids { s += total(k); }
            return s;
        }
    }
}

// The even leaves up to the first one over 40.
recursive fn evens(t: Tree) -> i64 {
    match t {
        Leaf l => { return l.v; },
        Node n => {
            var s = 0;
            for k in n.kids {
                let v = evens(k);
                if v % 2 == 1 { continue; }
                if v > 40 { break; }
                s += v;
            }
            return s;
        }
    }
}

recursive fn fib(n: i64) -> i64 {
    if n < 2 { return n; }
    return fib(n - 1) + fib(n - 2);
}

fn fibs(xs: i64[>..]&) {
    for &x in xs { x = fib(x); }
}

fn digits(s: const u8[:]) -> i64 {
    var n = 0;
    for c in s { n = n * 10 + fib(c as i64 - 48); }
    return n;
}

fn main() {
    var forest: Tree[>..] = [];
    for i in 9 { forest.push(Tree.Leaf { v: i * 7 }); }
    var none: Tree[>..] = [];
    print(total(Tree.Node { kids: forest[..] }), " ", total(Tree.Node { kids: none[..] }));
    print(evens(Tree.Node { kids: forest[..] }));
    var nested: Tree[>..] = [];
    nested.push(Tree.Node { kids: forest[2..4] });
    nested.push(Tree.Leaf { v: 1000 });
    nested.push(Tree.Node { kids: none[..] });
    print(total(Tree.Node { kids: nested[..] }));

    var xs: i64[>..] = [5, 10, 1];
    fibs(xs);
    print(xs[0], " ", xs[1], " ", xs[2]);
    var empty: i64[>..] = [];
    fibs(empty);
    print(empty.len);
    print(digits("123"), " ", digits(""));
}
