// error: cannot pop nodes: an index type names nodes, so its length never goes down
// The array an index type names is never shrunk. (docs/design/index_types.md)
var nodes: i32[>..] = [];
type NodeId = u32 in nodes;
fn main() {
    nodes.push(1);
    let w = 0 as NodeId;
    print(w);
    nodes.pop();
}
