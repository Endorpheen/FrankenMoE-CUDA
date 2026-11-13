// error: u32 in nodes has no value every state of nodes allows
// An index type has no default value: not even 0 is one while the array is empty. (docs/design/index_types.md)
var nodes: i32[>..] = [];
type NodeId = u32 in nodes;
fn main() {
    let w = default<NodeId>();
    print(w);
}
