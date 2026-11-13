// error: thread_spawn's first argument names a thread_fn: W is bound to show, which is not one
fn show(n: i32) { print(n); }
fn spawn<W>(n: i32) -> i64 { thread_spawn(W, n) }
fn main() { thread_wait(spawn(1, show)); }
