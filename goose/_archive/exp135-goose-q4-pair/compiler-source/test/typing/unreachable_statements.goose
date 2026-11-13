// runtime-debug: dead statements/tails are removed before they are checked.
import exception;

fn local_return() -> i64 {
    return 42;
    let bad: NoSuchType = no_such_function();
    return "incompatible dead return";
}
fn dead_tail() -> i64 { return 42; unknown_tail }
fn both_return(ok: bool) -> i64 {
    if ok { return 41; } else { return 42; }
    unknown_after_join();
    unknown_tail
}
fn take(n: i64) { assert(n == 42); }
fn stop() { exception::throw("stopped"); }
fn dead_after_call() -> i64 {
    stop();
    take(if true { 1 } else { 2 });
    unknown_after_call();
    unknown_tail
}
fn invoke<F>() { F(); }
fn dead_in_callback() -> i64 {
    invoke() {
        return 42;
        unknown_in_callback();
        unknown_callback_tail
    };
    unknown_after_callback();
    unknown_outer_tail
}
fn dead_after_abort() {
    abort("stopped");
    take(if true { 1 } else { 2 });
    unknown_after_abort
}
fn dead_after_exit() { exit(0); unknown_after_exit }
fn dead_after_loop() { loop {}; unknown_after_loop }

fn main() {
    assert(local_return() == 42 && dead_tail() == 42);
    assert(both_return(true) == 41 && both_return(false) == 42);
    assert(dead_in_callback() == 42);
    var caught = false;
    exception::try<const u8[:]>(dead_after_call) { error =>
        assert(error == "stopped"); caught = true;
    };
    assert(caught);

    let a = block { break 42; unknown_after_break(); unknown_block_tail };
    let b = loop { break 42; unknown_loop_tail };
    take(a); take(b);
    var n = 0;
    while n < 3 {
        n += 1;
        continue;
        unknown_after_continue();
        unknown_while_tail
    }
    assert(n == 3);
    for i in 3 {
        n += 1;
        continue;
        unknown_for_tail
    }
    assert(n == 6);
    fn branch() {
        let c = if n > 0 {
            stop();
            unknown_branch_tail
        } else { 42 };
        take(c);
    }
    caught = false;
    exception::try<const u8[:]>(branch) { error => caught = true; };
    assert(caught);
    print("unreachable statement tests passed");
}
