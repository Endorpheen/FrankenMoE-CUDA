// runtime-debug: rechecking arguments must preserve effects before an exit.
import exception;

fn stop() -> i64 { exception::throw("stopped"); }
fn take(a: i64, b: i64) { assert(false); }
fn ref_take(a: i64&, b: i64) { assert(false); }
fn slice_take(a: const u8[:], b: i64) { assert(false); }
fn default_take(a: i64, b: i64 = stop()) { assert(false); }
enum Choice { A, B }
fn dispatch(c: Choice.A, a: i64, b: i64) { assert(false); }
fn dispatch(c: Choice.B, a: i64, b: i64) { assert(false); }
fn print(n: bool) { assert(false); }

fn main() {
    var before = 0;
    var caught = 0;
    var x = 42;
    fn mark() { before += 1; }
    fn catcher(e: const u8[:]) { assert(e == "stopped"); caught += 1; }
    fn direct() { take(if true { mark(); 42 } else { 0 }, stop()); }
    fn receiver() { (if true { mark(); 42 } else { 0 }).take(stop()); }
    fn reference() { ref_take(if true { mark(); x } else { x }, stop()); }
    fn slice() { slice_take(if true { mark(); "yes" } else { "no" }, stop()); }
    fn defaults() { default_take(if true { mark(); 42 } else { 0 }); }
    fn dispatched(c: Choice) {
        dispatch(c, if true { mark(); 42 } else { 0 }, stop());
    }
    fn case_a() { dispatched(Choice.A); }
    fn case_b() { dispatched(Choice.B); }
    fn invoke<F>() { F(if true { mark(); 42 } else { 0 }, stop()); }
    fn callback() { invoke() { a: i64, b: i64 => assert(false); }; }
    fn fallback() { print(if true { mark(); 42 } else { 0 }, stop()); }
    exception::try<const u8[:]>(direct, catcher);
    exception::try<const u8[:]>(receiver, catcher);
    exception::try<const u8[:]>(reference, catcher);
    exception::try<const u8[:]>(slice, catcher);
    exception::try<const u8[:]>(defaults, catcher);
    exception::try<const u8[:]>(case_a, catcher);
    exception::try<const u8[:]>(case_b, catcher);
    exception::try<const u8[:]>(callback, catcher);
    exception::try<const u8[:]>(fallback, catcher);
    assert(before == 9 && caught == 9);
    print("never-return argument tests passed");
}
