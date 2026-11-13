// error: expected ';' in variable declaration, found 'print'
// error: cannot find the module not.there
// error: the package faraway's source ./no/such/dir is not a directory
// error: expected ')' in function declaration, found 'x'
// error: cannot find the module also_not_there
// An import that fails is reported at the import, and the program goes
// on without what it would have loaded (docs/design/language_server.md
// §5 D): the rest of the file is parsed, and so is every module the other
// imports load, the errors of each file in order and then its imports'.
// The module imported here imports this file back, and a module that is
// not there either.
import not.there;
import imported.helper;
import faraway from "./no/such/dir";
fn main() {
    let x = twice(2)
    print(x);
}
