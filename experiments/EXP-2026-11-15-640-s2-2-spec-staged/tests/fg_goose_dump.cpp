// Prints the grammar text of every corpus schema through json_schema_to_grammar.
// Used to compare the pristine base build with the staged OFF build: with the
// flag off the staged library must produce the same text as production.

#include <cstdio>

#include "json-schema-to-grammar.h"
#include "json.h"

#include "fg_goose_cases.h"

int main() {
    for (const auto & c : fg_schema_cases) {
        const std::string g = json_schema_to_grammar(common_json::parse(c[1]), true);
        printf("### %s\n%s\n", c[0], g.c_str());
    }
    return 0;
}
