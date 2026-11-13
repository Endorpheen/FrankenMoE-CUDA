// error: return from handler: no enclosing call of handler on this compile-time call path
// Catch runs outside its handler, so rethrow requires an outer try.
import exception;
fn work() { exception::throw("error"); }
fn main() {
    exception::try<u8[]>(work) { error => exception::throw(copy(error)); };
}
