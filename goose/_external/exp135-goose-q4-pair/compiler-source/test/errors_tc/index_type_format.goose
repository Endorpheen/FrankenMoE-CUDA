// error: .format appends text to u8 arrays
// A text sink writes any byte, so an index type of u8 is no byte. (docs/design/index_types.md)
var bytes: u8[>..] = [];
fn main() {
    bytes.push(1);
    var out: (u8 in bytes)[>..] = [];
    out.format("x");
}
