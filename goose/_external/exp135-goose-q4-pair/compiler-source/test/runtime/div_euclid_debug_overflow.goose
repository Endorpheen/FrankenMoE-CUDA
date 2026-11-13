// runtime-define: GS_DEBUG=1
import std;

fn main() {
    var values: i8[>..] = [-128, -1];
    print(div_euclid(values[0], values[1]));
}
