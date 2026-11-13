// A whole assignment through a struct's reference field frees the elements
// of the array the field refers to, and nothing else (§5.1): a holder built
// from a parameter keeps the path to that array, and its references of
// another element type cannot point into it, so a callee replacing the
// array may be followed by any use of the holder.
import std;

struct S { text: u8[>..]&, refs: (i64&)[>..] }

fn fill(s: S&) {
    let t = str("abc");
    s.text = copy(t);
}

fn make(text: u8[>..]&, nums: i64[>..]&) -> i64 {
    var s = S { text: text, refs: [] };
    s.refs.push(nums[0]);
    fill(s);
    return s.refs.len + s.refs[0] + s.text.len;
}

fn main() {
    var t: u8[>..] = ['x'];
    var nums: i64[>..] = [40];
    print(make(t, nums));
    print(t);
}
