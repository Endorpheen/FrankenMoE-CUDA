// error: holds variable-size elements, which cannot be indexed
// A sequential array cannot be indexed, so nothing is an index of it. (docs/design/index_types.md)
var names: u8[][>..] = [];
fn main() {
    names.push("a");
    let w = 0 as (u32 in names);
    print(w);
}
