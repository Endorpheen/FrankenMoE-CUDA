// The right operand of && and || runs only when the left one lets it, and so
// do the restores of the temporaries it builds on data stacks: a call's
// variable-size result compared with a slice, a str() result, a literal of
// variable-size elements. Each is compared with the operand running and
// skipped, nested, and in a loop condition; the scratch array grown after
// each call lands where those temporaries were. As the condition of an if,
// a guard, an assert or a loop, either operand's restores run on every path
// out of the chain of branches it compiles to, the loop's exit included.
struct Txt { body: u8[] }

fn word(long: bool) -> u8[] {
    if long { return "abcdef"; }
    return "ab";
}

fn scratch() -> i64 {
    var s: i64[>..] = [];
    for i in 100 { s.push(i); }
    var sum = 0;
    for x in s { sum += x; }
    return sum;
}

fn either(xs: u8[:], long: bool) -> bool { return xs.len > 3 || word(long) == xs; }
fn both(xs: u8[:], long: bool) -> bool { return xs.len < 3 && word(long) == xs; }
fn named(n: i64) -> bool { return n > 5 || str("n", n) == "n3"; }
fn texts(ts: Txt[], n: i64) -> bool {
    return n == 0 || ts == [Txt { body: "a" }, Txt { body: "bc" }];
}
fn nested(xs: u8[:], n: i64) -> bool {
    return n > 9 || (xs.len > 0 && (word(n > 4) == xs || str(n) == xs));
}

fn branches(xs: u8[:], n: i64) -> i64 {
    var r = 0;
    if xs.len > 0 && word(n > 4) == xs { r += 1; } else { r += 10; }
    if n > 9 || str(n) == xs { r += 100; }
    assert(str(n) != xs || xs.len > 0 && word(n > 4) != "xyz");
    if !(n == 3 && str(n) == xs) { r += 1000; }
    guard word(n > 4) != xs || xs.len > 5 && str(n) != "6" else { return r + 10000; }
    return r;
}

// Inlined into a condition, its returns and its end branch: the restores of
// what it builds run on the way out of it, taken or not.
fn named_word(xs: u8[:], n: i64) -> bool {
    let w = str("w", n);
    if w == xs { return true; }
    return word(n > 4) == xs || str(n) == xs;
}

fn calls(xs: u8[:], n: i64) -> i64 {
    var r = 0;
    if named_word(xs, n) { r += 1; } else { r += 10; }
    if !named_word(xs, n + 1) && n > 0 { r += 100; }
    var i = 0;
    while i < n && !named_word(xs, i) { i++; }
    return r + i * 1000;
}

fn loops(n: i64) -> i64 {
    var v: u8[>..] = [];
    while v.len < 2 || str(v.len) != "5" { v.push('x'); }
    var i = 0;
    while i < n && (word(i > 3) != "abcdef" || str(i) != "6") { i++; }
    return v.len * 100 + i;
}

fn main() {
    print(either("ab", false), " ", either("ab", true), " ", either("abcd", true), " ",
          scratch());
    print(both("ab", false), " ", both("ab", true), " ", both("abcd", false), " ", scratch());
    print(named(3), " ", named(4), " ", named(6), " ", scratch());
    print(texts([Txt { body: "a" }, Txt { body: "bc" }], 1), " ",
          texts([Txt { body: "a" }], 1), " ", texts([Txt { body: "a" }], 0), " ", scratch());
    print(nested("abcdef", 5), " ", nested("7", 7), " ", nested("", 3), " ",
          nested("x", 10), " ", nested("x", 3), " ", scratch());
    var w: u8[>..] = [];
    while w.len < 9 && word(true) != w { w.push(('a' + w.len) as u8); }
    print(w, " ", scratch());
    print(branches("", 5), " ", branches("ab", 2), " ", branches("abcdef", 6), " ",
          branches("3", 3), " ", branches("10", 10), " ", scratch());
    print(loops(9), " ", loops(4), " ", loops(0), " ", scratch());
    print(calls("w3", 3), " ", calls("ab", 2), " ", calls("abcdef", 7), " ", calls("5", 5), " ",
          calls("w2", 6), " ", calls("x", 0), " ", scratch());
}
