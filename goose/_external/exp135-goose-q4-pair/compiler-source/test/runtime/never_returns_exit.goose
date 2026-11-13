// runtime-debug: propagate exit through wrappers without emitting dead code.
fn stop() { exit(0); }
fn relay() { stop(); }
fn value() -> i64 { relay(); }
fn main() {
    print("before exit");
    value();
    no_such_function();
    unknown_tail
}
