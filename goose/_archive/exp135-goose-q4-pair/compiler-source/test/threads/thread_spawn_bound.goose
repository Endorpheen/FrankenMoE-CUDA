// thread_spawn names its worker through a function parameter bound to a
// thread_fn (§11.2, §7.6), as a library's spawn helper does, through any
// number of generic callers.
thread_fn worker(n: i32) { print("worker ", n); }
thread_fn other(n: i32) { print("other ", n); }

fn spawn<W>(n: i32) -> i64 { thread_spawn(W, n) }

fn spawn_each<W>(count: i32) {
    var ids: i64[>..] = [];
    for i in count { ids.push(spawn(i as i32, W)); }
    for id in ids { thread_wait(id); }
}

fn main() {
    thread_wait(spawn(1, worker));
    thread_wait(spawn(2, other));
    spawn_each(1, worker);
}
