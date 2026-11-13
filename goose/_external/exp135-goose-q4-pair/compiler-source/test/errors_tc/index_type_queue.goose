// error: cannot pass between threads: it holds an index type
// Each thread program's copy of the array grows on its own. (docs/design/index_types.md)
var nodes: i32[>..] = [];
type NodeId = u32 in nodes;
fn main() {
    nodes.push(1);
    qput(0 as NodeId);
}
