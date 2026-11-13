// `pos += k` with a k that is not a literal is `pos = pos + k`: a write the
// analysis reads at that point, which keeps every check on pos it proves
// elsewhere, the loops before the step included. Here k > 0 keeps pos
// nonnegative, and each loop's condition bounds it above.
import std;

fn scan(buf: const u8[:]) -> i64 {
    var pos = 0;
    while pos < buf.len && buf[pos] == 'a' { pos++; } // bce:elide
    let bs = byte_set("b");
    let k = find_any(buf[pos..], bs);
    if k > 0 && k <= buf.len - pos { pos += k; }
    while pos < buf.len && buf[pos] == 'b' { pos++; } // bce:elide
    return pos;
}

// A step whose operand writes the variable it steps is no such sum: the
// write leaves pos unknown.
fn bump(p: i64&) -> i64 {
    p = 0;
    return 2;
}

fn scan_back(buf: const u8[:]) -> i64 {
    var pos = 0;
    while pos < buf.len && buf[pos] == 'a' { pos++; }
    pos -= bump(pos);
    return buf[pos]; // bce:keep
}

fn main() {
    print(scan("aaxxbbc"));    // 6
    print(scan("aab"));        // 3
    print(scan_back("aaab"));  // 97
}
