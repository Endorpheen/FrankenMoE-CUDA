// error: missing initializer for field to of Edge
// A literal's `..` needs a default for every field it leaves out. (docs/design/index_types.md)
var nodes: i32[>..] = [];
type NodeId = u32 in nodes;
struct Edge { weight: i32, to: NodeId }
fn main() {
    let e = Edge { weight: 1, .. };
    print(e.weight);
}
