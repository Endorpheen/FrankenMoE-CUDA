// error: expected a value of type i64, got const u8[:]
import exception;
fn fail() { exception::throw("wrong type"); }
fn main() { exception::try<i64>(fail); }
