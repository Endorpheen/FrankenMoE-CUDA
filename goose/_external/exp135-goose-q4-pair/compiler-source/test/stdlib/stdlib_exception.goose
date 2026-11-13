// runtime-debug: library exceptions, owned payloads, nested handlers,
// rethrow, recursion, lexical exits, and repeated data-stack restoration.
import std;
import exception;

struct Error { message: u8[], code: i64 }
enum Problem { Text { message: u8[] }, Code { value: i64 } }

fn text_error() {
    var text: u8[>..] = [];
    format(text, "local ", 42);
    exception::throw(copy(text));
    assert(false);
}

fn checked_value(n: i64) -> i64 {
    if n < 0 { exception::throw("negative"); }
    return n * 2;
}

recursive fn deep(n: i64) {
    let scratch: u8[..32] = str("frame ", n);
    if n == 0 { exception::throw(Error { message: copy(scratch), code: 17 }); }
    deep(n - 1);
    assert(false);
}

// A Lobster-style protect can run cleanup after success or an exception.
// It cannot intercept a return aimed at a different function.
fn protect<E, Body, Cleanup>() {
    exception::try<E>(Body) { error =>
        Cleanup();
        exception::throw(copy(error));
    };
    Cleanup();
}

fn lexical_exit(cleaned: i64&) -> i64 {
    fn cleanup() { cleaned += 1; }
    fn body() {
        protect<u8[]>(cleanup_body, cleanup);
    }
    fn cleanup_body() { return 9 from lexical_exit; }
    exception::try<u8[]>(body) { error => assert(false); };
    return 0;
}

fn block_exit(caught: bool&) -> i64 {
    fn on_error(error: i64) { caught = true; }
    // Only a trailing block is a function literal. Make the body the last
    // generic argument in this local adapter to test lexical block returns.
    fn with_body<Body>() { exception::try<i64>(Body, on_error); }
    with_body() { return 23; };
    return 0;
}

fn main() {
    var count = 0;
    fn success() { count += 1; }
    exception::try<Error>(success) { error => assert(false); };
    assert(count == 1);

    exception::try<u8[]>(text_error) { error =>
        assert(error == "local 42");
        count += 1;
    };
    assert(count == 2);

    fn fresh_text() { exception::throw(str("fresh ", 19)); }
    exception::try<u8[]>(fresh_text) { error => assert(error == "fresh 19"); };

    fn record_error() { deep(12); }
    exception::try<Error>(record_error) { error =>
        assert(error.message == "frame 0" && error.code == 17);
        count += 1;
    };
    assert(count == 3);

    // Zero, false and an empty string are errors too, not success sentinels.
    fn zero() { exception::throw(0); }
    fn no() { exception::throw(false); }
    fn empty() { exception::throw(""); }
    exception::try<i64>(zero) { error => assert(error == 0); count += 1; };
    exception::try<bool>(no) { error => assert(!error); count += 1; };
    exception::try<u8[]>(empty) { error => assert(error.len == 0); count += 1; };
    assert(count == 6);

    fn values() {
        assert(checked_value(3) == 6);
        checked_value(-1);
        assert(false);
    }
    var value_caught = false;
    exception::try<u8[]>(values) { error =>
        assert(error == "negative");
        value_caught = true;
    };
    assert(value_caught);

    // Static references are permitted; local borrows are rejected in
    // errors_tc/exception_borrow_escape.goose.
    fn static_error() { exception::throw("static"); }
    exception::try<const u8[:]>(static_error) { error => assert(error == "static"); };

    fn enum_error() { exception::throw(Problem.Text { message: "variant" }); }
    exception::try<Problem..>(enum_error) { error =>
        match error {
            Text e => { assert(e.message == "variant"); count += 1; },
            Code => { assert(false); },
        }
    };
    assert(count == 7);

    // The same throwing function is checked for each target's error type.
    exception::try<i16>(zero) { error => assert(error == 0); count += 1; };
    assert(count == 8);

    fn rethrow_body() {
        exception::try<Error>(record_error) { error =>
            assert(error.code == 17);
            exception::throw(copy(error));
            assert(false);
        };
        assert(false);
    }
    exception::try<Error>(rethrow_body) { error =>
        assert(error.message == "frame 0" && error.code == 17);
        count += 1;
    };
    assert(count == 9);

    fn translate() {
        exception::try<u8[]>(text_error) { error =>
            exception::throw(Error { message: copy(error), code: 81 });
        };
    }
    exception::try<Error>(translate) { error =>
        assert(error.message == "local 42" && error.code == 81);
        count += 1;
    };
    assert(count == 10);

    // Handled inner errors leave the outer body running normally.
    fn inner_handled() {
        exception::try<u8[]>(text_error);
        count += 1;
    }
    exception::try<i64>(inner_handled) { error => assert(false); };
    assert(count == 11);

    var cleaned = 0;
    fn cleanup() { cleaned += 1; }
    fn protected_error() { protect<u8[]>(text_error, cleanup); }
    exception::try<u8[]>(protected_error) { error => assert(error == "local 42"); };
    assert(cleaned == 1);
    fn protected_success() { protect<u8[]>(success, cleanup); }
    exception::try<u8[]>(protected_success);
    assert(cleaned == 2);
    assert(lexical_exit(cleaned) == 9);
    assert(cleaned == 2);  // cleanup was bypassed: protect is NOT finally.
    var caught = false;
    assert(block_exit(caught) == 23 && !caught);

    // More than a small stack reservation over the whole run, but only a
    // bounded amount live at once. Errors must survive scratch destruction.
    fn large_error() {
        var scratch: u8[>..] = [];
        for i in 2048 { scratch.push('x'); }
        exception::throw(copy(scratch));
    }
    for round in 2048 {
        exception::try<u8[]>(large_error) { error =>
            assert(error.len == 2048 && error[0] == 'x' && error[2047] == 'x');
        };
    }
    print("exception tests passed");
}
