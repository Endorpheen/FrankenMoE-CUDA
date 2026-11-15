// bmoe::version() in two builds: tests/build.sh compiles this file twice, once
// with -DBMOE_VERSION=\"9.9.9-test\" and once without, so both branches of the
// header are real code and neither is assumed.
#include "bmoe/version.h"

extern "C" const char * VERSION_FN();

extern "C" const char * VERSION_FN() {
    return bmoe::version();
}
