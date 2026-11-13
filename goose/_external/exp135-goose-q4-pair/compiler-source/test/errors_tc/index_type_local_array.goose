// error: the array of an index type must be a global variable
// A local has no name where a type is declared. (docs/design/index_types.md)
fn main() {
    var local: i32[>..] = [];
    local.push(1);
    let w = 0 as (u32 in local);
    print(w);
}
