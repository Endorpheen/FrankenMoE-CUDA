// error: is not fixed-size and is not copied implicitly
import exception;
fn fail() {
    let text: u8[] = "owned";
    exception::throw(text);
}
fn main() { exception::try<u8[]>(fail); }
