// Narrow signed quotient overflow wraps in release, as with `/`.
import std;

fn main() {
    print(div_euclid(-128 as i8, -1));
    print(div_euclid(-32768 as i16, -1));
    print(div_euclid(-2147483648 as i32, -1));
}
