// Bridge for goose/json/json-parser.goose: the one host primitive the parser needs, and the runtime start.
//
// The module reads a decimal number as a byte slice and calls gs_json_strtod on it. Only the conversion is
// host code; where the number starts and ends, and what is legal, stays in Goose. The provider is included
// rather than copied so there is exactly one definition of it in the tree
// (integration/goose-standalone/json-strtod-provider.c).
//
// The constructor is not decoration. common/json.cpp reaches the door with dlsym and calls it; the generated
// module refuses to answer before goose_init has run ("goose_init must be called before exported
// functions"), and a caller that only knows the door's name cannot be asked to initialise it. Running the
// init when the library loads is the same guarantee the lazy first-call init gives the bridges that own
// their call, without requiring this path to own it.
//
// No prototype is declared by hand: build.py puts the generated header's directory on this file's include
// path, so an 18-argument export cannot drift from its definition here.

#include "../../goose-standalone/json-strtod-provider.c"

void fg_json_goose_init(int argc, char ** argv);

__attribute__((constructor)) static void fg_json_bridge_start(void) {
    fg_json_goose_init(0, (char **) 0);
}
