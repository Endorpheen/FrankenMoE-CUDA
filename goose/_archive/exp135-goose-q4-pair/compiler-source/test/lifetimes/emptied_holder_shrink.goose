// A holder emptied by a whole assignment of a value holding no references
// holds nothing until the next store (§5.1): a pool may shrink after that
// even while the holder is still used, every iteration of a loop, through a
// struct reference parameter whose tail holds the references and whose
// reference field reaches the pool, and on every call of a function doing
// so, however often it runs.
struct S { pool: i64[>..]&, refs: (i64&)[>..] }
struct H { r: (i64&)? }
struct T { h: H, refs: (i64&)[>..] }

fn run(s: S&, i: i64) {
    s.refs = [];
    s.pool = [];
    s.refs.push(s.pool.push(i));
}

fn rounds(s: S&, n: i64) {
    for i in n {
        s.refs = [];
        s.pool = [];
        s.refs.push(s.pool.push(i * 10));
        s.refs.push(s.pool.push(i * 10 + 1));
    }
}

fn outer(s: S&, n: i64) { for i in n { run(s, i + 100); } }

fn sum(refs: (i64&)[>..]&) -> i64 {
    var total = 0;
    for r in refs { total += r; }
    return total;
}

fn main() {
    // A local holder, emptied at the top of each iteration.
    var pool: i64[>..] = [];
    var refs: (i64&)[>..] = [];
    for i in 3 {
        refs = [];
        pool = [];
        refs.push(pool.push(i + 1));
    }
    print(refs.len, " ", refs[0], " ", pool.len);

    // Each part that can hold a view emptied, or the whole holder.
    var nums: i64[>..] = [5];
    var t = T { h: H {}, refs: [] };
    t.refs.push(nums[0]);
    t.h = H { r: nums[0] };
    t.h = H {};
    t.refs = [];
    nums = [6, 7];
    t.refs.push(nums[1]);
    t = T { h: H {}, refs: [] };
    nums = [8];
    print(t.refs.len, " ", nums[0]);

    // Through a struct reference parameter, called again and again, through
    // a function taking the struct by reference too, and through a `let`
    // reference to it.
    var p: i64[>..] = [];
    var s = S { pool: p, refs: [] };
    for i in 3 { run(s, i); }
    print(sum(s.refs), " ", p.len);
    rounds(s, 3);
    rounds(s, 2);
    print(sum(s.refs), " ", p.len);
    outer(s, 3);
    outer(s, 2);
    print(sum(s.refs), " ", p.len);
    let r = s;
    for i in 2 { run(r, i + 7); }
    print(sum(s.refs), " ", s.refs.len);
}
