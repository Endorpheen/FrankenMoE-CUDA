// error: return from handler: no enclosing call of handler on this compile-time call path
import exception;
fn fail() { exception::throw("uncaught"); }
fn main() { fail(); }
