// The oracle for EXP977: httplib's own field validators, so the gate compares
// against the library rather than against a second copy of my reading of it.
#include "httplib.h"

#include <string>

namespace httplib {

bool oracle_is_token(const std::string & s) {
    return detail::fields::is_token(s);
}

bool oracle_is_field_valid(const std::string & name, const std::string & value) {
    return detail::fields::is_field_valid(name, value);
}

bool oracle_is_request_target(const std::string & s) {
    return detail::fields::is_request_target(s);
}

} // namespace httplib
