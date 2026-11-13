// runtime-debug: a regrouped min chain (optimize_reassoc.h) whose operands
// would both index out of bounds aborts at the first one written, as the
// chain does as written.
import std;

fn main() {
    let xs: i64[4] = [5, 3, 8, 1];
    let ys: i64[3] = [9, 9, 9];
    let zs: i64[2] = [7, 7];
    var acc: i64 = 100;
    for i in 4 {
        acc = min(min(xs[i],
                      acc - ys[i + 1]),
                  zs[i]);
        print(acc);
    }
}
