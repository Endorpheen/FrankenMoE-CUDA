// error: thread programs may access only flat globals
// error: view holds references (reached from thread_fn worker)
// A `let` global viewing another global would let the worker read the
// spawning program's storage rather than a copy of its own (§11.2).
var buf: u8[4] = "abcd";
let view = buf[1..];
thread_fn worker() { print(view); }
fn main() { thread_wait(thread_spawn(worker)); }
