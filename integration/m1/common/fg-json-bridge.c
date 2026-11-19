// Bridge for goose/json/json-parser.goose: the one host primitive the parser needs.
//
// The module reads a decimal number as a byte slice and calls gs_json_strtod on it. Only the conversion is
// host code; where the number starts and ends, and what is legal, stays in Goose. The provider is included
// rather than copied so there is exactly one definition of it in the tree
// (integration/goose-standalone/json-strtod-provider.c).
//
// No wrapper functions are declared here on purpose: the generated header carries the real prototype of
// json_parse, and build.py now puts the generated header's directory on this file's include path, so a
// hand-written signature - the fastest way to get an ABI wrong - is not needed.

#include "../../goose-standalone/json-strtod-provider.c"
