// error: as! cannot make a u32 in nodes
// Only the checked conversion makes an index. (docs/design/index_types.md)
var nodes: i32[>..] = [];
type NodeId = u32 in nodes;
fn main() {
    nodes.push(1);
    let w = 0 as! NodeId;
    print(w);
}
