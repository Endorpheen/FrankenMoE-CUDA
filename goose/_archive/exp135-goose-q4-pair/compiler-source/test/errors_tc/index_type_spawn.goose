// error: cannot pass between threads: it holds an index type
// A worker's argument crosses to another thread program. (docs/design/index_types.md)
var nodes: i32[>..] = [];
type NodeId = u32 in nodes;
thread_fn worker(w: NodeId) { print(w); }
fn main() {
    nodes.push(1);
    thread_wait(thread_spawn(worker, 0 as NodeId));
}
