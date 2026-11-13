// A conversion into an index type checks its value against the array in
// every build, with the error indexing would give (§3.11).

import std;

var nodes: i32[>..] = [];
type NodeId = u32 in nodes;

fn convert(x: i64) -> NodeId { x as NodeId }

fn main() {
    for v in 3 { nodes.push(v as i32); }
    print(convert(2));
    print(convert(5));
    print("not reached");
}
