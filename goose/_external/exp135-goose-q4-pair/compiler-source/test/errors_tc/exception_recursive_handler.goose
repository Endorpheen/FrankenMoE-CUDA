// error: whose non-fixed-size result would land in a temporary here
// An enclosing handler can catch throws from recursive code, but a try
// itself cannot be re-entered recursively with this owned-array transport.
import exception;
recursive fn nested(n: i64) {
    fn body() {
        if n > 0 { nested(n - 1); }
        exception::throw(n);
    }
    exception::try<i64>(body) { error => assert(error == n); };
}
fn main() { nested(3); }
