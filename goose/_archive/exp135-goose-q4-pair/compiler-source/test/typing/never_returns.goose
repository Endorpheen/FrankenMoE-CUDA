// runtime-debug: inferred completion across calls, callbacks and dispatch.
import exception;

fn stop() { exception::throw("stopped"); }
fn relay() { stop(); }
fn value() -> i64 { relay(); }
fn tail_value() -> i64 { relay() }
fn invoke<F>() { F(); }
fn callback_value() -> i64 { invoke(stop); }

fn choose(ok: bool) -> i64 { if ok { 42 } else { stop(); } }
fn assigned(ok: bool) -> i64 {
    let n: i64;
    if ok { n = 42; } else { stop(); }
    return n;
}
fn narrowed(n: i64?) -> i64 {
    guard n else { stop(); }
    return n;
}

enum Choice { A, B }
fn visit(c: Choice.A) { stop(); }
fn visit(c: Choice.B) { stop(); }
fn dispatched(c: Choice) -> i64 { visit(c); }
fn mixed(c: Choice.A) { abort("stopped"); }
fn mixed(c: Choice.B) {}
fn mixed_value(c: Choice) -> i64 { mixed(c); return 42; }

// The explicit stop proves noncompletion even with conservative back edges.
recursive fn descend(n: i64) {
    if n > 0 { descend(n - 1); }
    stop();
}
fn recursive_value() -> i64 { descend(4); }

// Returning to this body resumes its caller, including on a cached call.
fn own_exit() -> i64 {
    fn done() { return 42 from own_exit; }
    done();
}
fn lexical_exit() -> i64 { invoke() { return 42; }; }

// Unresolved recursion must keep its continuation live during checking.
recursive fn count(n: i64) -> i64 {
    if n == 0 { return 0; }
    let rest = count(n - 1);
    return rest + (if n > 0 { 1 } else { 2 });
}
recursive fn left(n: i64) -> i64 { right(n) }
recursive fn right(n: i64) -> i64 {
    if n == 0 { return 42; }
    return left(n - 1);
}

// Builtin termination propagates too; these functions are checked standalone.
fn abort_relay() { abort("stopped"); }
fn abort_value() -> i64 { abort_relay(); }
fn exit_relay() { exit(0); }
fn exit_value() -> i64 { exit_relay(); }
fn spin() { loop {} }
fn spin_value() -> i64 { spin(); }

fn main() {
    var caught = 0;
    fn catcher(e: const u8[:]) { assert(e == "stopped"); caught += 1; }
    exception::try<const u8[:]>(value, catcher);
    exception::try<const u8[:]>(tail_value, catcher);
    exception::try<const u8[:]>(callback_value, catcher);
    fn alternatives() {
        assert(choose(true) == 42);
        assert(assigned(true) == 42);
        var n = 42;
        assert(narrowed(n) == 42);
        choose(false);
    }
    exception::try<const u8[:]>(alternatives, catcher);
    fn unassigned() { assigned(false); }
    fn null_value() { narrowed(null); }
    fn case_a() { dispatched(Choice.A); }
    fn case_b() { dispatched(Choice.B); }
    exception::try<const u8[:]>(unassigned, catcher);
    exception::try<const u8[:]>(null_value, catcher);
    exception::try<const u8[:]>(case_a, catcher);
    exception::try<const u8[:]>(case_b, catcher);
    exception::try<const u8[:]>(recursive_value, catcher);
    assert(caught == 9);
    assert(mixed_value(Choice.B) == 42);
    assert(own_exit() == 42 && own_exit() == 42);
    assert(lexical_exit() == 42);
    assert(count(4) == 4 && left(4) == 42);

    // Completion belongs to the specialization, including its callback.
    var ran = 0;
    fn success() { ran += 1; }
    invoke(success);
    invoke(success);
    assert(ran == 2);
    print("never-return tests passed");
}
