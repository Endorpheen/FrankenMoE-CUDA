import std;

fn main() {
    var values: i64[>..] = [-9223372036854775808, -1];
    print(div_euclid(values[0], values[1]));
}
