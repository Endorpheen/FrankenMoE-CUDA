// error: a long-distance return may only carry references to globals or static data
import exception;
fn fail() {
    var scratch: u8[>..] = [];
    scratch.push('x');
    exception::throw(scratch[..]);
}
fn main() { exception::try<u8[:]>(fail); }
