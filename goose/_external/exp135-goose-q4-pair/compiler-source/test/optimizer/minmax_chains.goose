// runtime-debug: chains of integer min and max with an operand carried from
// the previous iteration, which the optimizer regroups to combine that
// operand last (optimize_reassoc.h), compute what they do as written, with
// their operands evaluated once each and in the order written.
import std;

// Wagner-Fischer: the insertion cost reads the element the previous
// iteration wrote.
fn edit_distance(s: u8[:], t: u8[:]) -> u32 {
    var prev_row: u32[>..] = [];
    var curr_row: u32[>..] = [];
    for i in s.len + 1 { prev_row.push(i as u32); }
    push_n(curr_row, 0, s.len + 1);
    var prev: u32[:] = prev_row;
    var curr: u32[:] = curr_row;
    for c, j in t {
        curr[0] = (j + 1) as u32;
        for i in s.len {
            let cost: u32 = if s[i] != c { 1 } else { 0 };
            curr[i + 1] = min(min(prev[i + 1] + 1, curr[i] + 1), prev[i] + cost);
        }
        swap(prev, curr);
    }
    return prev[s.len];
}

// The longest common subsequence's length, a max chain over signed values,
// the carried operand nested on the right.
fn lcs(s: u8[:], t: u8[:]) -> i64 {
    var prev: i64[>..] = [];
    var curr: i64[>..] = [];
    push_n(prev, 0, s.len + 1);
    push_n(curr, 0, s.len + 1);
    for c in t {
        for i in s.len {
            let hit: i64 = if s[i] == c { 1 } else { -1000 };
            curr[i + 1] = max(prev[i + 1], max(curr[i], prev[i] + hit));
        }
        for i in s.len + 1 { prev[i] = curr[i]; }
    }
    return prev[s.len];
}

// A scalar carried in a variable, in a four-operand chain with a literal.
fn running(xs: i32[:]) -> i32 {
    var best: i32 = -50;
    for x in xs {
        best = max(max(max(x * 2, best - 3), -7), x + 3);
    }
    return best;
}

// Operands with effects: each records itself, so the order they ran in shows.
var trail: u8[>..];

fn note(tag: u8, v: i64) -> i64 {
    trail.push(tag);
    return v;
}

fn ordered(n: i64) -> i64 {
    var acc: i64 = 100;
    for i in n {
        if i > 0 { trail.push(' '); }
        acc = min(min(note('a', 50 - i), note('b', acc - 1)), note('c', 60 + i));
    }
    return acc;
}

fn main() {
    print(edit_distance("kitten", "sitting"), " ", edit_distance("flaw", "lawn"), " ",
          edit_distance("", "abc"), " ", edit_distance("intention", "execution"));
    print(lcs("ABCBDAB", "BDCABA"), " ", lcs("goose", "moose"), " ", lcs("", "x"));
    let xs: i32[6] = [4, -9, 12, 1, -30, 7];
    print(running(xs));
    print(ordered(5), " ", trail);
}
