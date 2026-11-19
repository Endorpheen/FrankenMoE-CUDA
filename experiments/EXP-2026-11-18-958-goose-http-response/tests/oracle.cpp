// The oracle for EXP958: call cpp-httplib's real status_message and rebuild the
// two lines exactly as its own writers do.
//
// The difference from a pure transcription: the reason phrases come from
// httplib::status_message itself, linked from the vendored httplib.cpp, so a
// table mismatch is caught by the library rather than by my copy of it.
// write_response_line and write_headers are internal statics, and their bodies
// are three concatenations; those three concatenations are reproduced here and
// the strings they use come from the library.
#include "httplib.h"

#include <string>

namespace httplib {

const char * status_message_ref(int status) {
    return status_message(status);
}

// httplib.cpp:4049-4056 write_response_line, verbatim in effect.
std::string response_line_ref(int status) {
    std::string s = "HTTP/1.1 ";
    s += std::to_string(status);
    s += ' ';
    s += status_message(status);
    s += "\r\n";
    return s;
}

// httplib.cpp:4063-4070 write_headers, verbatim in effect.
std::string header_line_ref(const std::string & name, const std::string & value) {
    std::string s;
    s = name;
    s += ": ";
    s += value;
    s += "\r\n";
    return s;
}

} // namespace httplib
