// Oracle for the grammar automaton: the native translation unit is compiled into
// this fixture so the static functions can be called directly.
#include "../../../vendor/flint/src/llama-grammar.cpp"

#include <cstdio>
#include <vector>

int main() {
    // grammar: root = "a" (single character rule)
    llama_grammar_rules rules(2);
    rules[0].push_back({LLAMA_GRETYPE_CHAR, 'a'});
    rules[0].push_back({LLAMA_GRETYPE_END, 0});
    rules[1].push_back({LLAMA_GRETYPE_CHAR, 'x'});
    rules[1].push_back({LLAMA_GRETYPE_END, 0});

    llama_grammar_stacks out;
    llama_grammar_stack stack;
    stack.push_back(rules[0].data());
    llama_grammar_advance_stack(rules, stack, out);

    printf("stacks=%zu\n", out.size());
    for (size_t i = 0; i < out.size(); ++i) {
        printf("stack %zu depth=%zu\n", i, out[i].size());
    }
    return 0;
}
