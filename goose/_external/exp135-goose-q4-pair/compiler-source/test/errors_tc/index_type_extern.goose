// error: C code is not checked against the array an index type names
// C code could return any integer. (docs/design/index_types.md)
var nodes: i32[>..] = [];
type NodeId = u32 in nodes;
extern fn pick() -> NodeId;
fn main() {
    nodes.push(1);
    print(pick());
}
