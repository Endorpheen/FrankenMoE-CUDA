#include "llama-grammar.h"
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <chrono>

#include "llama-impl.h"
#include "llama-vocab.h"
#include "llama-sampler.h"

#include <cmath>
#include <algorithm>
#include <cstdint>
#include <set>
#include <stdexcept>

#define MAX_REPETITION_THRESHOLD 2000
//
// helpers
//

// NOTE: assumes valid utf8 (but checks for overrun)
static std::pair<uint32_t, const char *> decode_utf8(const char * src) {
    static const int lookup[] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 3, 4 };
    uint8_t  first_byte = static_cast<uint8_t>(*src);
    uint8_t  highbits   = first_byte >> 4;
    int      len        = lookup[highbits];
    uint8_t  mask       = (1 << (8 - len)) - 1;
    uint32_t value      = first_byte & mask;
    const char * end    = src + len; // may overrun!
    const char * pos    = src + 1;
    for ( ; pos < end && *pos; pos++) {
        value = (value << 6) + (static_cast<uint8_t>(*pos) & 0x3F);
    }
    return std::make_pair(value, pos);
}

static std::pair<std::vector<uint32_t>, llama_partial_utf8> decode_utf8(
        const std::string & src,
        llama_partial_utf8 partial_start) {
    static const int      lookup[] = { 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 2, 2, 3, 4 };
    const char          * pos      = src.c_str();
    std::vector<uint32_t> code_points;

    // common english strings have the same number of codepoints and bytes. `+ 1` for the terminating 0.
    code_points.reserve(src.size() + 1);
    uint32_t value    = partial_start.value;
    int      n_remain = partial_start.n_remain;

    // continue previous decode, if applicable
    while (*pos != 0 && n_remain > 0) {
        uint8_t next_byte = static_cast<uint8_t>(*pos);
        if ((next_byte >> 6) != 2) {
            // invalid sequence, abort
            code_points.push_back(0);
            return std::make_pair(std::move(code_points), llama_partial_utf8{ 0, -1 });
        }
        value = (value << 6) + (next_byte & 0x3F);
        ++pos;
        --n_remain;
    }

    if (partial_start.n_remain > 0 && n_remain == 0) {
        code_points.push_back(value);
    }

    // decode any subsequent utf-8 sequences, which may end in an incomplete one
    while (*pos != 0) {
        uint8_t first_byte = static_cast<uint8_t>(*pos);
        uint8_t highbits   = first_byte >> 4;
        n_remain   = lookup[highbits] - 1;

        if (n_remain < 0) {
            // invalid sequence, abort
            code_points.clear();
            code_points.push_back(0);
            return std::make_pair(std::move(code_points), llama_partial_utf8{ 0, n_remain });
        }

        uint8_t mask  = (1 << (7 - n_remain)) - 1;
        value = first_byte & mask;

        ++pos;
        while (*pos != 0 && n_remain > 0) {
            value = (value << 6) + (static_cast<uint8_t>(*pos) & 0x3F);
            ++pos;
            --n_remain;
        }
        if (n_remain == 0) {
            code_points.push_back(value);
        }
    }
    code_points.push_back(0);

    return std::make_pair(std::move(code_points), llama_partial_utf8{ value, n_remain });
}

static bool is_digit_char(char c) {
    return '0' <= c && c <= '9';
}

static bool is_word_char(char c) {
    return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '-' || is_digit_char(c);
}

static std::pair<uint32_t, const char *> parse_hex(const char * src, int size) {
    const char * pos   = src;
    const char * end   = src + size;
    uint32_t     value = 0;
    for ( ; pos < end && *pos; pos++) {
        value <<= 4;
        char c = *pos;
        if ('a' <= c && c <= 'f') {
            value += c - 'a' + 10;
        } else if ('A' <= c && c <= 'F') {
            value += c - 'A' + 10;
        } else if ('0' <= c && c <= '9') {
            value += c - '0';
        } else {
            break;
        }
    }
    if (pos != end) {
        throw std::runtime_error("expecting " + std::to_string(size) + " hex chars at " + src);
    }
    return std::make_pair(value, pos);
}

static const char * parse_space(const char * src, bool newline_ok) {
    const char * pos = src;
    while (*pos == ' ' || *pos == '\t' || *pos == '#' ||
            (newline_ok && (*pos == '\r' || *pos == '\n'))) {
        if (*pos == '#') {
            while (*pos && *pos != '\r' && *pos != '\n') {
                pos++;
            }
        } else {
            pos++;
        }
    }
    return pos;
}

static const char * parse_name(const char * src) {
    const char * pos = src;
    while (is_word_char(*pos)) {
        pos++;
    }
    if (pos == src) {
        throw std::runtime_error(std::string("expecting name at ") + src);
    }
    return pos;
}

static const char * parse_int(const char * src) {
    const char * pos = src;
    while (is_digit_char(*pos)) {
        pos++;
    }
    if (pos == src) {
        throw std::runtime_error(std::string("expecting integer at ") + src);
    }
    return pos;
}

static std::pair<uint32_t, const char *> parse_char(const char * src) {
    if (*src == '\\') {
        switch (src[1]) {
            case 'x': return parse_hex(src + 2, 2);
            case 'u': return parse_hex(src + 2, 4);
            case 'U': return parse_hex(src + 2, 8);
            case 't': return std::make_pair('\t', src + 2);
            case 'r': return std::make_pair('\r', src + 2);
            case 'n': return std::make_pair('\n', src + 2);
            case '\\':
            case '"':
            case '[':
            case ']':
            case '-':
                      return std::make_pair(src[1], src + 2);
            default:
                      throw std::runtime_error(std::string("unknown escape at ") + src);
        }
    } else if (*src) {
        return decode_utf8(src);
    }
    throw std::runtime_error("unexpected end of input");
}

static std::pair<uint32_t, const char *> parse_token(const llama_vocab * vocab, const char * src) {
    const char * pos = src;
    if (*pos != '<') {
        throw std::runtime_error(std::string("expecting '<' at ") + pos);
    }
    pos++;

    // Parse <[id]>
    if (*pos == '[') {
        pos++;
        const char * int_end = parse_int(pos);
        unsigned long id = std::stoul(std::string(pos, int_end - pos));
        if (id > std::numeric_limits<uint32_t>::max()) {
            throw std::runtime_error(std::string("parsed token id is too big at ") + pos);
        }
        uint32_t token_id = static_cast<uint32_t>(id);
        pos = int_end;
        if (*pos != ']') {
            throw std::runtime_error(std::string("expecting ']' at ") + pos);
        }
        pos++;
        if (*pos != '>') {
            throw std::runtime_error(std::string("expecting '>' at ") + pos);
        }
        pos++;
        return std::make_pair(token_id, pos);
    }

    if (vocab == nullptr) {
        throw std::runtime_error(std::string("no vocab to parse token at ") + src);
    }

    // Parse <token> and tokenize to obtain the token id
    while (*pos != 0 && *pos != '>') {
        pos++;
    }
    if (*pos != '>') {
        throw std::runtime_error(std::string("expecting '>' at ") + pos);
    }
    pos++;

    llama_token tokens[2];
    int32_t n_tokens = vocab->tokenize(src, static_cast<int32_t>(pos - src), tokens, 2, false, true);
    if (n_tokens != 1) {
        // must tokenize to exactly 1 token
        throw std::runtime_error("invalid token '" + std::string(src, pos - src) + "'");
    }
    return std::make_pair(tokens[0], pos);
}

static void print_grammar_char(FILE * file, uint32_t c) {
    if (0x20 <= c && c <= 0x7f) {
        fprintf(file, "%c", static_cast<char>(c));
    } else {
        // cop out of encoding UTF-8
        fprintf(file, "<U+%04X>", c);
    }
}

static bool is_char_element(llama_grammar_element elem) {
    switch (elem.type) {
        case LLAMA_GRETYPE_CHAR:           return true;
        case LLAMA_GRETYPE_CHAR_NOT:       return true;
        case LLAMA_GRETYPE_CHAR_ALT:       return true;
        case LLAMA_GRETYPE_CHAR_RNG_UPPER: return true;
        case LLAMA_GRETYPE_CHAR_ANY:       return true;
        default:                           return false;
    }
}

static void print_rule_binary(FILE * file, const llama_grammar_rule & rule) {
    for (auto elem : rule) {
        switch (elem.type) {
            case LLAMA_GRETYPE_END:            fprintf(file, "END");            break;
            case LLAMA_GRETYPE_ALT:            fprintf(file, "ALT");            break;
            case LLAMA_GRETYPE_RULE_REF:       fprintf(file, "RULE_REF");       break;
            case LLAMA_GRETYPE_CHAR:           fprintf(file, "CHAR");           break;
            case LLAMA_GRETYPE_CHAR_NOT:       fprintf(file, "CHAR_NOT");       break;
            case LLAMA_GRETYPE_CHAR_RNG_UPPER: fprintf(file, "CHAR_RNG_UPPER"); break;
            case LLAMA_GRETYPE_CHAR_ALT:       fprintf(file, "CHAR_ALT");       break;
            case LLAMA_GRETYPE_CHAR_ANY:       fprintf(file, "CHAR_ANY");       break;
            case LLAMA_GRETYPE_TOKEN:          fprintf(file, "TOKEN");          break;
            case LLAMA_GRETYPE_TOKEN_NOT:      fprintf(file, "TOKEN_NOT");      break;
        }
        switch (elem.type) {
            case LLAMA_GRETYPE_END:
            case LLAMA_GRETYPE_ALT:
            case LLAMA_GRETYPE_RULE_REF:
                fprintf(file, "(%u) ", elem.value);
                break;
            case LLAMA_GRETYPE_CHAR:
            case LLAMA_GRETYPE_CHAR_NOT:
            case LLAMA_GRETYPE_CHAR_RNG_UPPER:
            case LLAMA_GRETYPE_CHAR_ALT:
            case LLAMA_GRETYPE_CHAR_ANY:
                fprintf(file, "(\"");
                print_grammar_char(file, elem.value);
                fprintf(file, "\") ");
                break;
            case LLAMA_GRETYPE_TOKEN:
                fprintf(file, "<[");
                fprintf(file, "%u", elem.value);
                fprintf(file, "]> ");
                break;
            case LLAMA_GRETYPE_TOKEN_NOT:
                fprintf(file, "!");
                fprintf(file, "<[");
                fprintf(file, "%u", elem.value);
                fprintf(file, "]> ");
                break;
        }
    }
    fprintf(file, "\n");
}

static void print_rule(
        FILE     * file,
        uint32_t   rule_id,
        const llama_grammar_rule & rule,
        const std::map<uint32_t, std::string> & symbol_id_names) {
    if (rule.empty() || rule.back().type != LLAMA_GRETYPE_END) {
        throw std::runtime_error(
            "malformed rule, does not end with LLAMA_GRETYPE_END: " + std::to_string(rule_id));
    }
    fprintf(file, "%s ::= ", symbol_id_names.at(rule_id).c_str());
    for (size_t i = 0, end = rule.size() - 1; i < end; i++) {
        llama_grammar_element elem = rule[i];
        switch (elem.type) {
            case LLAMA_GRETYPE_END:
                throw std::runtime_error(
                    "unexpected end of rule: " + std::to_string(rule_id) + "," +
                    std::to_string(i));
            case LLAMA_GRETYPE_ALT:
                fprintf(file, "| ");
                break;
            case LLAMA_GRETYPE_RULE_REF:
                fprintf(file, "%s ", symbol_id_names.at(elem.value).c_str());
                break;
            case LLAMA_GRETYPE_CHAR:
                fprintf(file, "[");
                print_grammar_char(file, elem.value);
                break;
            case LLAMA_GRETYPE_CHAR_NOT:
                fprintf(file, "[^");
                print_grammar_char(file, elem.value);
                break;
            case LLAMA_GRETYPE_CHAR_RNG_UPPER:
                if (i == 0 || !is_char_element(rule[i - 1])) {
                    throw std::runtime_error(
                        "LLAMA_GRETYPE_CHAR_RNG_UPPER without preceding char: " +
                        std::to_string(rule_id) + "," + std::to_string(i));
                }
                fprintf(file, "-");
                print_grammar_char(file, elem.value);
                break;
            case LLAMA_GRETYPE_CHAR_ALT:
                if (i == 0 || !is_char_element(rule[i - 1])) {
                    throw std::runtime_error(
                        "LLAMA_GRETYPE_CHAR_ALT without preceding char: " +
                        std::to_string(rule_id) + "," + std::to_string(i));
                }
                print_grammar_char(file, elem.value);
                break;
            case LLAMA_GRETYPE_CHAR_ANY:
                fprintf(file, ".");
                break;
            case LLAMA_GRETYPE_TOKEN:
                fprintf(file, "<[");
                fprintf(file, "%u", elem.value);
                fprintf(file, "]> ");
                break;
            case LLAMA_GRETYPE_TOKEN_NOT:
                fprintf(file, "!");
                fprintf(file, "<[");
                fprintf(file, "%u", elem.value);
                fprintf(file, "]> ");
                break;
        }
        if (is_char_element(elem)) {
            switch (rule[i + 1].type) {
                case LLAMA_GRETYPE_CHAR_ALT:
                case LLAMA_GRETYPE_CHAR_RNG_UPPER:
                case LLAMA_GRETYPE_CHAR_ANY:
                    break;
                default:
                    fprintf(file, "] ");
            }
        }
    }
    fprintf(file, "\n");
}

//
// Regex utilities
//

size_t llama_grammar_trigger_pattern::find(const std::string & input) const {
    auto find_start_pos = [](const std::smatch & match) {
        // get from the first matched capturing group to the end of the string
        size_t start = std::string::npos;
        for (auto i = 1u; i < match.size(); i++) {
            if (match.length(i) > 0) {
                start = match.position(i);
                break;
            }
        }
        if (start == std::string::npos) {
            start = match.position(0);
        }
        return start;
    };

    if (!pattern.empty() && pattern.front() == '^' && pattern.back() == '$') {
        // match against the entire input
        std::smatch match;
        if (std::regex_match(input, match, regex)) {
            return find_start_pos(match);
        }
    }

    // search anywhere
    std::smatch match;
    if (std::regex_search(input, match, regex)) {
        return find_start_pos(match);
    }

    return std::string::npos;
}


//
// implementation
//

uint32_t llama_grammar_parser::get_symbol_id(const char * src, size_t len) {
    uint32_t next_id = static_cast<uint32_t>(symbol_ids.size());
    auto result = symbol_ids.emplace(std::string(src, len), next_id);
    return result.first->second;
}

uint32_t llama_grammar_parser::generate_symbol_id(const std::string & base_name) {
    uint32_t next_id = static_cast<uint32_t>(symbol_ids.size());
    symbol_ids[base_name + '_' + std::to_string(next_id)] = next_id;
    return next_id;
}

void llama_grammar_parser::add_rule(uint32_t rule_id, const llama_grammar_rule & rule) {
    if (rules.size() <= rule_id) {
        rules.resize(rule_id + 1);
    }
    rules[rule_id] = rule;
}

const char * llama_grammar_parser::parse_alternates(
        const char        * src,
        const std::string & rule_name,
        uint32_t            rule_id,
        bool                is_nested) {
    llama_grammar_rule rule;
    const char * pos = parse_sequence(src, rule_name, rule, is_nested);
    while (*pos == '|') {
        rule.push_back({LLAMA_GRETYPE_ALT, 0});
        pos = parse_space(pos + 1, true);
        pos = parse_sequence(pos, rule_name, rule, is_nested);
    }
    rule.push_back({LLAMA_GRETYPE_END, 0});
    add_rule(rule_id, rule);
    return pos;
}

const char * llama_grammar_parser::parse_sequence(
        const char         * src,
        const std::string  & rule_name,
        llama_grammar_rule & rule,
        bool               is_nested) {
    size_t last_sym_start = rule.size();
    const char * pos = src;
    uint64_t n_prev_rules = 1;

    // use UINT64_MAX as the empty value because we aligned to the proper uint64_t type so -1 can't be used
    // (though it's technically the same as -1 now)
    auto handle_repetitions = [&](uint64_t min_times, uint64_t max_times) {
        bool no_max = max_times == UINT64_MAX;
        if (last_sym_start == rule.size()) {
            throw std::runtime_error(std::string("expecting preceding item to */+/?/{ at ") + pos);
        }

        // apply transformation to previous symbol (last_sym_start to end) according to
        // the following rewrite rules:
        // S{m,n} --> S S S (m times) S'(n-m)
        //            S'(x)   ::= S S'(x-1) |
        //            (... n-m definitions of these S' rules ...)
        //            S'(1)   ::= S |
        // S{m,} -->  S S S (m times) S'
        //            S'     ::= S S' |
        // S*     --> S{0,}
        //        --> S'     ::= S S' |
        // S+     --> S{1,}
        //        --> S S'
        //            S'     ::= S S' |
        // S?     --> S{0,1}
        //        --> S'
        //            S'     ::= S |

        llama_grammar_rule prev_rule(rule.begin() + last_sym_start, rule.end());
        // Calculate the total number of rules that will be generated by this repetition
        uint64_t total_rules = 1; // Start with 1 for the original rule
        if (!no_max && max_times > 0) {
            total_rules = max_times;
        } else if (min_times > 0) {
            total_rules = min_times;
        }

        if (n_prev_rules * total_rules > MAX_REPETITION_THRESHOLD) {
            throw std::runtime_error("number of rules that are going to be repeated multiplied by the new repetition exceeds sane defaults, please reduce the number of repetitions or rule complexity");
        }

        if (min_times == 0) {
            rule.resize(last_sym_start);
        } else {
            // Repeat the previous elements (min_times - 1) times
            for (uint64_t i = 1; i < min_times; i++) {
                rule.insert(rule.end(), prev_rule.begin(), prev_rule.end());
            }
        }

        uint32_t last_rec_rule_id = 0;
        auto n_opt = no_max ? 1 : max_times - min_times;

        llama_grammar_rule rec_rule(prev_rule);
        for (uint64_t i = 0; i < n_opt; i++) {
            rec_rule.resize(prev_rule.size());
            uint32_t rec_rule_id = generate_symbol_id( rule_name);
            if (i > 0 || no_max) {
                rec_rule.push_back({LLAMA_GRETYPE_RULE_REF, no_max ? rec_rule_id : last_rec_rule_id});
            }
            rec_rule.push_back({LLAMA_GRETYPE_ALT, 0});
            rec_rule.push_back({LLAMA_GRETYPE_END, 0});
            add_rule( rec_rule_id, rec_rule);
            last_rec_rule_id = rec_rule_id;
        }
        if (n_opt > 0) {
            rule.push_back({LLAMA_GRETYPE_RULE_REF, last_rec_rule_id});
        }
        n_prev_rules *= total_rules;
        GGML_ASSERT(n_prev_rules >= 1);
    };

    while (*pos) {
        if (*pos == '"') { // literal string
            pos++;
            last_sym_start = rule.size();
            n_prev_rules = 1;
            while (*pos != '"') {
                if (!*pos) {
                    throw std::runtime_error("unexpected end of input");
                }
                auto char_pair = parse_char(pos);
                     pos       = char_pair.second;
                rule.push_back({LLAMA_GRETYPE_CHAR, char_pair.first});
            }
            pos = parse_space(pos + 1, is_nested);
        } else if (*pos == '[') { // char range(s)
            pos++;
            enum llama_gretype start_type = LLAMA_GRETYPE_CHAR;
            if (*pos == '^') {
                pos++;
                start_type = LLAMA_GRETYPE_CHAR_NOT;
            }
            last_sym_start = rule.size();
            n_prev_rules = 1;
            while (*pos != ']') {
                if (!*pos) {
                    throw std::runtime_error("unexpected end of input");
                }
                auto char_pair = parse_char(pos);
                     pos       = char_pair.second;
                enum llama_gretype type = last_sym_start < rule.size()
                    ? LLAMA_GRETYPE_CHAR_ALT
                    : start_type;

                rule.push_back({type, char_pair.first});
                if (pos[0] == '-' && pos[1] != ']') {
                    if (!pos[1]) {
                        throw std::runtime_error("unexpected end of input");
                    }
                    auto endchar_pair = parse_char(pos + 1);
                         pos          = endchar_pair.second;
                    rule.push_back({LLAMA_GRETYPE_CHAR_RNG_UPPER, endchar_pair.first});
                }
            }
            pos = parse_space(pos + 1, is_nested);
        } else if (*pos == '<' || *pos == '!') { // token
            auto type = LLAMA_GRETYPE_TOKEN;
            if (*pos == '!') { // token inverse
                type = LLAMA_GRETYPE_TOKEN_NOT;
                pos++;
            }
            auto token_pair = parse_token(vocab, pos);
            const char * token_end  = token_pair.second;
            last_sym_start = rule.size();
            n_prev_rules = 1;
            rule.push_back({type, token_pair.first});
            pos = parse_space(token_end, is_nested);
        } else if (is_word_char(*pos)) { // rule reference
            const char * name_end    = parse_name(pos);
            uint32_t ref_rule_id = get_symbol_id(pos, name_end - pos);
            pos = parse_space(name_end, is_nested);
            last_sym_start = rule.size();
            n_prev_rules = 1;
            rule.push_back({LLAMA_GRETYPE_RULE_REF, ref_rule_id});
        } else if (*pos == '(') { // grouping
            // parse nested alternates into synthesized rule
            pos = parse_space(pos + 1, true);
            uint32_t n_rules_before = symbol_ids.size();
            uint32_t sub_rule_id = generate_symbol_id(rule_name);
            pos = parse_alternates(pos, rule_name, sub_rule_id, true);
            n_prev_rules = std::max(1u, (uint32_t)symbol_ids.size() - n_rules_before);
            last_sym_start = rule.size();
            // output reference to synthesized rule
            rule.push_back({LLAMA_GRETYPE_RULE_REF, sub_rule_id});
            if (*pos != ')') {
                throw std::runtime_error(std::string("expecting ')' at ") + pos);
            }
            pos = parse_space(pos + 1, is_nested);
        } else if (*pos == '.') { // any char
            last_sym_start = rule.size();
            n_prev_rules = 1;
            rule.push_back({LLAMA_GRETYPE_CHAR_ANY, 0});
            pos = parse_space(pos + 1, is_nested);
        } else if (*pos == '*') {
            pos = parse_space(pos + 1, is_nested);
            handle_repetitions(0, -1);
        } else if (*pos == '+') {
            pos = parse_space(pos + 1, is_nested);
            handle_repetitions(1, -1);
        } else if (*pos == '?') {
            pos = parse_space(pos + 1, is_nested);
            handle_repetitions(0, 1);
        } else if (*pos == '{') {
            pos = parse_space(pos + 1, is_nested);

            if (!is_digit_char(*pos)) {
                throw std::runtime_error(std::string("expecting an int at ") + pos);
            }
            const char * int_end = parse_int(pos);
            uint64_t min_times = std::stoull(std::string(pos, int_end - pos));
            pos = parse_space(int_end, is_nested);

            uint64_t max_times = UINT64_MAX; // default: no max limit

            if (*pos == '}') {
                max_times = min_times;
                pos = parse_space(pos + 1, is_nested);
            } else if (*pos == ',') {
                pos = parse_space(pos + 1, is_nested);

                if (is_digit_char(*pos)) {
                    const char * int_end = parse_int(pos);
                    max_times = std::stoull(std::string(pos, int_end - pos));
                    pos = parse_space(int_end, is_nested);
                }

                if (*pos != '}') {
                    throw std::runtime_error(std::string("expecting '}' at ") + pos);
                }
                pos = parse_space(pos + 1, is_nested);
            } else {
                throw std::runtime_error(std::string("expecting ',' at ") + pos);
            }
            if (min_times > MAX_REPETITION_THRESHOLD) {
                throw std::runtime_error(std::string("number of repetitions exceeds sane defaults, please reduce the number of repetitions"));
            }
            if (max_times != UINT64_MAX && max_times > MAX_REPETITION_THRESHOLD) {
                max_times = UINT64_MAX;
            }
            handle_repetitions(min_times, max_times);
        } else {
            break;
        }
    }
    return pos;
}

const char * llama_grammar_parser::parse_rule(const char * src) {
    const char * name_end = parse_name(src);
    const char * pos      = parse_space(name_end, false);
    size_t       name_len = name_end - src;
    uint32_t     rule_id  = get_symbol_id(src, name_len);
    const std::string name(src, name_len);

    if (!(pos[0] == ':' && pos[1] == ':' && pos[2] == '=')) {
        throw std::runtime_error(std::string("expecting ::= at ") + pos);
    }
    pos = parse_space(pos + 3, true);

    pos = parse_alternates(pos, name, rule_id, false);

    if (*pos == '\r') {
        pos += pos[1] == '\n' ? 2 : 1;
    } else if (*pos == '\n') {
        pos++;
    } else if (*pos) {
        throw std::runtime_error(std::string("expecting newline or end at ") + pos);
    }
    return parse_space(pos, true);
}

bool llama_grammar_parser::parse(const char * src) {
    try {
        const char * pos = parse_space(src, true);
        while (*pos) {
            pos = parse_rule(pos);
        }
        // Validate the state to ensure that all rules are defined
        for (const auto & rule : rules) {
            if (rule.empty()) {
                throw std::runtime_error("Undefined rule");
            }
            for (const auto & elem : rule) {
                if (elem.type == LLAMA_GRETYPE_RULE_REF) {
                    // Ensure that the rule at that location exists
                    if (elem.value >= rules.size() || rules[elem.value].empty()) {
                        // Get the name of the rule that is missing
                        for (const auto & kv : symbol_ids) {
                            if (kv.second == elem.value) {
                                throw std::runtime_error("Undefined rule identifier '" + kv.first + "'");
                            }
                        }
                    }
                }
            }
        }
    } catch (const std::exception & err) {
        fprintf(stderr, "%s: error parsing grammar: %s\n\n%s\n", __func__, err.what(), src);
        rules.clear();
        return false;
    }

    return true;
}

void llama_grammar_parser::print(FILE * file) {
    try {
        std::map<uint32_t, std::string> symbol_id_names;
        for (const auto & kv : symbol_ids) {
            symbol_id_names[kv.second] = kv.first;
        }
        for (size_t i = 0, end = rules.size(); i < end; i++) {
            // fprintf(file, "%zu: ", i);
            // print_rule_binary(file, rules[i]);
            print_rule(file, uint32_t(i), rules[i], symbol_id_names);
            // fprintf(file, "\n");
        }
    } catch (const std::exception & err) {
        fprintf(stderr, "\n%s: error printing grammar: %s\n", __func__, err.what());
    }
}

llama_grammar_stack llama_grammar_parser::c_rules() const {
    llama_grammar_stack ret;
    ret.reserve(rules.size());
    for (const auto & rule : rules) {
        ret.push_back(rule.data());
    }
    return ret;
}

// returns true iff pos points to the end of one of the definitions of a rule
static bool llama_grammar_is_end_of_sequence(const llama_grammar_element * pos) {
    switch (pos->type) {
        case LLAMA_GRETYPE_END: return true;  // NOLINT
        case LLAMA_GRETYPE_ALT: return true;  // NOLINT
        default:                return false;
    }
}

// returns true iff chr satisfies the char range at pos (regular or inverse range)
// asserts that pos is pointing to a char range element
static std::pair<bool, const llama_grammar_element *> llama_grammar_match_char(
        const llama_grammar_element * pos,
        const uint32_t                chr) {
    bool found            = false;
    bool is_positive_char = pos->type == LLAMA_GRETYPE_CHAR || pos->type == LLAMA_GRETYPE_CHAR_ANY;

    GGML_ASSERT(is_positive_char || pos->type == LLAMA_GRETYPE_CHAR_NOT); // NOLINT

    do {
        if (pos[1].type == LLAMA_GRETYPE_CHAR_RNG_UPPER) {
            // inclusive range, e.g. [a-z]
            found = found || (pos->value <= chr && chr <= pos[1].value);
            pos += 2;
        } else if (pos->type == LLAMA_GRETYPE_CHAR_ANY) {
            // Any character matches "."
            found = true;
            pos += 1;
        } else {
            // exact char match, e.g. [a] or "a"
            found = found || pos->value == chr;
            pos += 1;
        }
    } while (pos->type == LLAMA_GRETYPE_CHAR_ALT);

    return std::make_pair(found == is_positive_char, pos);
}

// returns true iff some continuation of the given partial UTF-8 sequence could satisfy the char
// range at pos (regular or inverse range)
// asserts that pos is pointing to a char range element
static bool llama_grammar_match_partial_char(
        const llama_grammar_element * pos,
        const llama_partial_utf8      partial_utf8) {
    bool is_positive_char = pos->type == LLAMA_GRETYPE_CHAR || pos->type == LLAMA_GRETYPE_CHAR_ANY;
    GGML_ASSERT(is_positive_char || pos->type == LLAMA_GRETYPE_CHAR_NOT);

    uint32_t partial_value = partial_utf8.value;
    int      n_remain      = partial_utf8.n_remain;

    // invalid sequence or 7-bit char split across 2 bytes (overlong)
    if (n_remain < 0 || (n_remain == 1 && partial_value < 2)) {
        return false;
    }

    // range of possible code points this partial UTF-8 sequence could complete to
    uint32_t low  = partial_value << (n_remain * 6);
    uint32_t high = low | ((1 << (n_remain * 6)) - 1);

    if (low == 0) {
        if (n_remain == 2) {
            low = 1 << 11;
        } else if (n_remain == 3) {
            low = 1 << 16;
        }
    }

    do {
        if (pos[1].type == LLAMA_GRETYPE_CHAR_RNG_UPPER) {
            // inclusive range, e.g. [a-z]
            if (pos->value <= high && low <= pos[1].value) {
                return is_positive_char;
            }
            pos += 2;
        } else if (pos->type == LLAMA_GRETYPE_CHAR_ANY) {
            // Any character matches "."
            return true;
        } else {
            // exact char match, e.g. [a] or "a"
            if (low <= pos->value && pos->value <= high) {
                return is_positive_char;
            }
            pos += 1;
        }
    } while (pos->type == LLAMA_GRETYPE_CHAR_ALT);

    return !is_positive_char;
}

// returns true iff token matches the rule at pos (regular or inverse)
// asserts that pos is pointing to a token element
static bool llama_grammar_match_token(
    const llama_grammar_element * pos,
    const llama_token             token) {
    GGML_ASSERT(pos->type == LLAMA_GRETYPE_TOKEN || pos->type == LLAMA_GRETYPE_TOKEN_NOT);
    if (pos->type == LLAMA_GRETYPE_TOKEN) {
        return pos->value == static_cast<uint32_t>(token);
    }
    if (pos->type == LLAMA_GRETYPE_TOKEN_NOT) {
        return pos->value != static_cast<uint32_t>(token);
    }
    return false;
}

// transforms a grammar pushdown stack into N possible stacks, all ending
// at a character range (terminal element)
static void llama_grammar_advance_stack(
        const llama_grammar_rules  & rules,
        const llama_grammar_stack  & stack,
        llama_grammar_stacks & new_stacks) {
    std::vector<llama_grammar_stack> todo;
    todo.push_back(stack);

    auto stack_cmp = [](const llama_grammar_stack & a, const llama_grammar_stack & b) {
        return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(),
            [](const llama_grammar_element * pa, const llama_grammar_element * pb) {
                return pa < pb;  // Compare pointer addresses
            }
        );
    };

    std::set<llama_grammar_stack, decltype(stack_cmp)> seen(stack_cmp);

    while (!todo.empty()) {
        llama_grammar_stack curr_stack_candidate = std::move(todo.back());
        todo.pop_back();

        auto [curr_stack_it, inserted] = seen.insert(std::move(curr_stack_candidate));
        if (!inserted) {
            continue;
        }
        const llama_grammar_stack & curr_stack = *curr_stack_it;

        if (curr_stack.empty()) {
            if (std::find(new_stacks.begin(), new_stacks.end(), curr_stack) == new_stacks.end()) {
                new_stacks.emplace_back(curr_stack);
            }
            continue;
        }

        const llama_grammar_element * pos = curr_stack.back();

        switch (pos->type) {
        case LLAMA_GRETYPE_RULE_REF: {
            const size_t                  rule_id = static_cast<size_t>(pos->value);
            const llama_grammar_element * subpos  = rules[rule_id].data();
            do {
                // init new stack without the top (pos)
                llama_grammar_stack next_stack(curr_stack.begin(), curr_stack.end() - 1);
                if (!llama_grammar_is_end_of_sequence(pos + 1)) {
                    // if this rule ref is followed by another element, add that to stack
                    next_stack.push_back(pos + 1);
                }
                if (!llama_grammar_is_end_of_sequence(subpos)) {
                    // if alternate is nonempty, add to stack
                    next_stack.push_back(subpos);
                }
                todo.push_back(std::move(next_stack));
                while (!llama_grammar_is_end_of_sequence(subpos)) {
                    // scan to end of alternate def
                    subpos++;
                }
                if (subpos->type == LLAMA_GRETYPE_ALT) {
                    // there's another alternate def of this rule to process
                    subpos++;
                } else {
                    break;
                }
            } while (true);
            break;
        }
        case LLAMA_GRETYPE_CHAR:
        case LLAMA_GRETYPE_CHAR_NOT:
        case LLAMA_GRETYPE_CHAR_ANY:
        case LLAMA_GRETYPE_TOKEN:
        case LLAMA_GRETYPE_TOKEN_NOT:
            if (std::find(new_stacks.begin(), new_stacks.end(), curr_stack) == new_stacks.end()) {
                // only add the stack if it's not a duplicate of one we already have
                new_stacks.emplace_back(curr_stack);
            }
            break;
        default:
            // end of alternate (LLAMA_GRETYPE_END, LLAMA_GRETYPE_ALT) or middle of char range
            // (LLAMA_GRETYPE_CHAR_ALT, LLAMA_GRETYPE_CHAR_RNG_UPPER); stack should never be left on
            // those
            GGML_ABORT("fatal error");
        }
    }
}

static llama_grammar_candidates llama_grammar_reject_candidates(
        const llama_grammar_rules      & rules,
        const llama_grammar_stacks     & stacks,
        const llama_grammar_candidates & candidates) {
    GGML_ASSERT(!stacks.empty()); // REVIEW

    if (candidates.empty()) {
        return {};
    }

    auto rejects = llama_grammar_reject_candidates_for_stack(rules, stacks.front(), candidates);

    for (size_t i = 1, size = stacks.size(); i < size; ++i) {
        rejects = llama_grammar_reject_candidates_for_stack(rules, stacks[i], rejects);
    }

    return rejects;
}

static bool llama_grammar_detect_left_recursion(
        const llama_grammar_rules & rules,
        size_t rule_index,
        std::vector<bool> * rules_visited,
        std::vector<bool> * rules_in_progress,
        std::vector<bool> * rules_may_be_empty) {
    if ((*rules_in_progress)[rule_index]) {
        return true;
    }

    (*rules_in_progress)[rule_index] = true;

    const llama_grammar_rule & rule = rules[rule_index];

    // First check if the rule might produce the empty string. This could be done combined with the second
    // step but it's more readable as two steps.
    bool at_rule_start = true;
    for (size_t i = 0; i < rule.size(); i++) {
        if (llama_grammar_is_end_of_sequence(&rule[i])) {
            if (at_rule_start) {
                (*rules_may_be_empty)[rule_index] = true;
                break;
            }
            at_rule_start = true;
        } else {
            at_rule_start = false;
        }
    }

    // Second, recurse into leftmost nonterminals (or next-leftmost as long as the previous nonterminal may
    // be empty)
    bool recurse_into_nonterminal = true;
    for (size_t i = 0; i < rule.size(); i++) {
        if (rule[i].type == LLAMA_GRETYPE_RULE_REF && recurse_into_nonterminal) {
            if (llama_grammar_detect_left_recursion(rules, (size_t)rule[i].value, rules_visited, rules_in_progress, rules_may_be_empty)) {
                return true;
            }
            if (!((*rules_may_be_empty)[(size_t)rule[i].value])) {
                recurse_into_nonterminal = false;
            }
        } else if (llama_grammar_is_end_of_sequence(&rule[i])) {
            recurse_into_nonterminal = true;
        } else {
            recurse_into_nonterminal = false;
        }
    }

    (*rules_in_progress)[rule_index] = false;
    (*rules_visited)[rule_index] = true;

    return false;
}

const llama_grammar_rules & llama_grammar_get_rules(const struct llama_grammar * grammar) {
    return grammar->rules;
}

llama_grammar_stacks & llama_grammar_get_stacks(struct llama_grammar * grammar) {
    return grammar->stacks;
}

static void llama_grammar_accept_chr(
        struct llama_grammar       & grammar,
        const llama_grammar_stack  & stack,
              uint32_t               chr,
              llama_grammar_stacks & new_stacks) {
    if (stack.empty()) {
        return;
    }

    const llama_grammar_element * pos = stack.back();

    // ignore if this turns into a token
    if (pos->type == LLAMA_GRETYPE_TOKEN || pos->type == LLAMA_GRETYPE_TOKEN_NOT) {
        return;
    }

    auto match = llama_grammar_match_char(pos, chr);
    if (match.first) {
        llama_grammar_stack new_stack(stack.begin(), stack.end() - 1);
        if (!llama_grammar_is_end_of_sequence(match.second)) {
            new_stack.push_back(match.second);
        }
        llama_grammar_advance_stack(grammar.rules, new_stack, new_stacks);
    }
}

void llama_grammar_accept(struct llama_grammar * grammar, uint32_t chr) {
    llama_grammar_stacks stacks_new;
    stacks_new.reserve(grammar->stacks.size());

    for (const auto & stack : grammar->stacks) {
        llama_grammar_accept_chr(*grammar, stack, chr, stacks_new);
    }

    grammar->stacks = std::move(stacks_new);
}

llama_grammar_candidates llama_grammar_reject_candidates_for_stack(
        const llama_grammar_rules      & rules,
        const llama_grammar_stack      & stack,
        const llama_grammar_candidates & candidates) {

    llama_grammar_candidates rejects;
    rejects.reserve(candidates.size());

    if (stack.empty()) {
        for (const auto & tok : candidates) {
            if (*tok.code_points != 0 || tok.partial_utf8.n_remain != 0) {
                rejects.push_back(tok);
            }
        }
        return rejects;
    }

    const llama_grammar_element * stack_pos = stack.back();

    // if the top of the stack is a token rule, then we only need to check the token id
    if (stack_pos->type == LLAMA_GRETYPE_TOKEN || stack_pos->type == LLAMA_GRETYPE_TOKEN_NOT) {
        for (const auto & tok : candidates) {
            if (*tok.code_points == 0) {
                // reached the end of a token consumed by char rules, reject iff it ended
                // in a partial response
                if (tok.partial_utf8.n_remain != 0) {
                    rejects.push_back(tok);
                }
            } else if (!llama_grammar_match_token(stack_pos, tok.id)) {
                rejects.push_back(tok);
            }
        }
        return rejects;
    }

    llama_grammar_candidates next_candidates;
    next_candidates.reserve(candidates.size());

    for (const auto & tok : candidates) {
        if (*tok.code_points == 0) {
            // reached end of full codepoints in token, reject iff it ended in a partial sequence
            // that cannot satisfy this position in grammar
            if (tok.partial_utf8.n_remain != 0 &&
                    !llama_grammar_match_partial_char(stack_pos, tok.partial_utf8)) {
                rejects.push_back(tok);
            }
        } else if (llama_grammar_match_char(stack_pos, *tok.code_points).first) {
            next_candidates.push_back({ tok.index, tok.code_points + 1, tok.partial_utf8, tok.id });
        } else {
            rejects.push_back(tok);
        }
    }

    const auto * stack_pos_after = llama_grammar_match_char(stack_pos, 0).second;

    // update top of stack to next element, if any
    llama_grammar_stack stack_after(stack.begin(), stack.end() - 1);
    if (!llama_grammar_is_end_of_sequence(stack_pos_after)) {
        stack_after.push_back(stack_pos_after);
    }
    llama_grammar_stacks next_stacks;
    llama_grammar_advance_stack(rules, stack_after, next_stacks);

    auto next_rejects = llama_grammar_reject_candidates(rules, next_stacks, next_candidates);
    for (const auto & tok : next_rejects) {
        rejects.push_back({ tok.index, tok.code_points - 1, tok.partial_utf8, tok.id });
    }

    return rejects;
}

////////////////////

struct llama_grammar * llama_grammar_init_impl(
        const struct llama_vocab * vocab,
        const llama_grammar_element ** rules,
        size_t n_rules,
        size_t start_rule_index) {
    const llama_grammar_element * pos;

    // copy rule definitions into vectors
    llama_grammar_rules vec_rules(n_rules);
    for (size_t i = 0; i < n_rules; i++) {
        for (pos = rules[i]; pos->type != LLAMA_GRETYPE_END; pos++) {
            vec_rules[i].push_back(*pos);
        }
        vec_rules[i].push_back({LLAMA_GRETYPE_END, 0});
    }

    // Validate that all rule references point to valid rules
    for (size_t i = 0; i < n_rules; i++) {
        for (const auto & elem : vec_rules[i]) {
            if (elem.type == LLAMA_GRETYPE_RULE_REF) {
                if (elem.value >= n_rules || vec_rules[elem.value].empty()) {
                    LLAMA_LOG_ERROR("invalid grammar: rule %zu references undefined rule %u\n", i, elem.value);
                    return nullptr;
                }
            }
        }
    }

    // Check for left recursion
    std::vector<bool> rules_visited(n_rules);
    std::vector<bool> rules_in_progress(n_rules);
    std::vector<bool> rules_may_be_empty(n_rules);
    for (size_t i = 0; i < n_rules; i++) {
        if (rules_visited[i]) {
            continue;
        }
        if (llama_grammar_detect_left_recursion(vec_rules, i, &rules_visited, &rules_in_progress, &rules_may_be_empty)) {
            LLAMA_LOG_ERROR("unsupported grammar, left recursion detected for nonterminal at index %zu", i);
            return nullptr;
        }
    }

    // loop over alternates of start rule to build initial stacks
    llama_grammar_stacks stacks;
    pos = vec_rules[start_rule_index].data();
    do {
        llama_grammar_stack stack;
        if (!llama_grammar_is_end_of_sequence(pos)) {
            // if alternate is nonempty, add to stack
            stack.push_back(pos);
        }
        llama_grammar_advance_stack(vec_rules, stack, stacks);
        while (!llama_grammar_is_end_of_sequence(pos)) {
            // scan to end of alternate def
            pos++;
        }
        if (pos->type == LLAMA_GRETYPE_ALT) {
            // there's another alternate def of this rule to process
            pos++;
        } else {
            break;
        }
    } while (true);

    // Important: vec_rules has to be moved here, not copied, because stacks contains
    // pointers to elements of vec_rules. If vec_rules were copied into llama_grammar
    // then the pointers would be invalidated when the local vec_rules goes out of scope.
    return new llama_grammar {
        vocab,
        std::move(vec_rules),
        std::move(stacks),
        /* .partial_utf8 = */             {},
        /* .lazy = */                     false,
        /* .awaiting_trigger = */         false,
        /* .trigger_buffer = */           "",
        /* .trigger_buffer_positions = */ {},
        /* .trigger_tokens = */           {},
        /* .trigger_patterns = */         {},
    };
}

struct llama_grammar * llama_grammar_init_impl(
        const struct llama_vocab * vocab,
                      const char * grammar_str,
                      const char * grammar_root,
                              bool lazy,
                     const char ** trigger_patterns,
                            size_t num_trigger_patterns,
               const llama_token * trigger_tokens,
                            size_t num_trigger_tokens) {
    llama_grammar_parser parser(vocab);

    // if there is a grammar, parse it
    // rules will be empty (default) if there are parse errors
    if (!parser.parse(grammar_str) || parser.rules.empty()) {
        LLAMA_LOG_ERROR("failed to parse grammar\n");
        return nullptr;
    }

    // Ensure that the grammar contains the start symbol
    if (parser.symbol_ids.find(grammar_root) == parser.symbol_ids.end()) {
        LLAMA_LOG_ERROR("grammar does not contain a '%s' symbol\n", grammar_root);
        return nullptr;
    }

    std::vector<const llama_grammar_element *> grammar_rules(parser.c_rules());

    const size_t n_rules = grammar_rules.size();
    const size_t start_rule_index = parser.symbol_ids.at(grammar_root);

    const llama_grammar_element * pos;

    // copy rule definitions into vectors
    llama_grammar_rules vec_rules(n_rules);
    for (size_t i = 0; i < n_rules; i++) {
        for (pos = grammar_rules[i]; pos->type != LLAMA_GRETYPE_END; pos++) {
            vec_rules[i].push_back(*pos);
        }
        vec_rules[i].push_back({LLAMA_GRETYPE_END, 0});
    }

    // Check for left recursion
    std::vector<bool> rules_visited(n_rules);
    std::vector<bool> rules_in_progress(n_rules);
    std::vector<bool> rules_may_be_empty(n_rules);
    for (size_t i = 0; i < n_rules; i++) {
        if (rules_visited[i]) {
            continue;
        }
        if (llama_grammar_detect_left_recursion(vec_rules, i, &rules_visited, &rules_in_progress, &rules_may_be_empty)) {
            LLAMA_LOG_ERROR("unsupported grammar, left recursion detected for nonterminal at index %zu\n", i);
            return nullptr;
        }
    }

    // loop over alternates of start rule to build initial stacks
    llama_grammar_stacks stacks;
    pos = vec_rules[start_rule_index].data();
    do {
        llama_grammar_stack stack;
        if (!llama_grammar_is_end_of_sequence(pos)) {
            // if alternate is nonempty, add to stack
            stack.push_back(pos);
        }
        llama_grammar_advance_stack(vec_rules, stack, stacks);
        while (!llama_grammar_is_end_of_sequence(pos)) {
            // scan to end of alternate def
            pos++;
        }
        if (pos->type == LLAMA_GRETYPE_ALT) {
            // there's another alternate def of this rule to process
            pos++;
        } else {
            break;
        }
    } while (true);

    std::vector<llama_token>    vec_trigger_tokens;
    std::vector<llama_grammar_trigger_pattern> vec_trigger_patterns;
    for (size_t i = 0; i < num_trigger_tokens; i++) {
        GGML_ASSERT(trigger_tokens != nullptr);
        vec_trigger_tokens.push_back(trigger_tokens[i]);
    }
    for (size_t i = 0; i < num_trigger_patterns; i++) {
        GGML_ASSERT(trigger_patterns != nullptr);
        auto & trigger = vec_trigger_patterns.emplace_back();
        trigger.pattern = trigger_patterns[i];
        trigger.regex = std::regex(trigger.pattern);
    }

    // Important: vec_rules has to be moved here, not copied, because stacks contains
    // pointers to elements of vec_rules. If vec_rules were copied into llama_grammar
    // then the pointers would be invalidated when the local vec_rules goes out of scope.
    return new llama_grammar {
        vocab,
        std::move(vec_rules),
        std::move(stacks),
        /* .partial_utf8 = */             {},
        /* .lazy = */                     lazy,
        /* .awaiting_trigger = */         lazy,
        /* .trigger_buffer = */           "",
        /* .trigger_buffer_positions = */ {},
        std::move(vec_trigger_tokens),
        std::move(vec_trigger_patterns),
    };
}

void llama_grammar_free_impl(struct llama_grammar * grammar) {
    if (grammar == nullptr) {
        return;
    }

    delete grammar;
}

struct llama_grammar * llama_grammar_clone_impl(const struct llama_grammar & grammar) {
    auto * result = new llama_grammar {
        grammar.vocab,
        grammar.rules,
        grammar.stacks,
        grammar.partial_utf8,
        grammar.lazy,
        grammar.awaiting_trigger,
        grammar.trigger_buffer,
        grammar.trigger_buffer_positions,
        grammar.trigger_tokens,
        grammar.trigger_patterns,
    };

    // redirect elements in stacks to point to new rules
    for (size_t is = 0; is < result->stacks.size(); is++) {
        for (size_t ie = 0; ie < result->stacks[is].size(); ie++) {
            for (size_t ir0 = 0; ir0 < grammar.rules.size(); ir0++) {
                for (size_t ir1 = 0; ir1 < grammar.rules[ir0].size(); ir1++) {
                    if (grammar.stacks[is][ie] == &grammar.rules[ir0][ir1]) {
                        result->stacks[is][ie] =  &result->rules[ir0][ir1];
                    }
                }
            }
        }
    }

    return result;
}


// ---- Goose grammar decision hook -------------------------------------------
// The reject decision is computed by the Goose automaton; the host keeps the
// vocabulary-side work (piece decoding) and the -inf marking.
// same layout as the generated code's slice descriptor
typedef struct { void * data; int64_t len; } fg_slice_rec;

extern "C" int64_t fg_grammar_reject_call(
        fg_slice_rec s_etype, fg_slice_rec s_eval, fg_slice_rec s_roff, fg_slice_rec s_rlen, fg_slice_rec s_seed,
        int64_t v_seedd,
        fg_slice_rec s_cand_cp, fg_slice_rec s_cp_len, fg_slice_rec s_pval, fg_slice_rec s_pnr, fg_slice_rec s_cand_id,
        int64_t v_ncand,
        fg_slice_rec s_wp, fg_slice_rec s_wpd, fg_slice_rec s_stacks, fg_slice_rec s_stackd, fg_slice_rec s_scratch,
        fg_slice_rec s_tmp_rej, fg_slice_rec s_tmp_cur, fg_slice_rec s_rowbuf, fg_slice_rec s_rej,
        int64_t v_maxs, int64_t v_maxd, int64_t v_maxcp) __attribute__((weak));

extern "C" int64_t fg_grammar_first_set_call(fg_slice_rec s_etype, fg_slice_rec s_eval,
                                            fg_slice_rec s_stacks, fg_slice_rec s_stackd,
                                            int64_t v_nstacks, fg_slice_rec s_lo,
                                            fg_slice_rec s_hi, int64_t v_maxr)
                                            __attribute__((weak));

static bool fg_grammar_enabled() {
    // Opt-in, and it must stay that way until the decision is proven to be equal to the
    // automaton's. With it on, a constrained request produced a corrupted byte
    // (`nam\xef\xbf\xbd e`) and, with the pre-filter, an empty-stack exception - both mean the
    // hook accepted a token the native stack would have rejected. Rejecting or accepting too
    // much is a correctness defect, never a performance trade. See EXP-2026-11-18-1194.
    static const bool on = []() {
        const char * f = getenv("GGML_GOOSE_GRAMMAR");
        return f != nullptr && strcmp(f, "0") != 0;
    }();
    return on;
}

// The rule table is immutable, so it is flattened once per grammar object.
struct fg_flat_rules {
    std::vector<int64_t> etype, eval, roff, rlen;
    // every rule is its own vector, so a pointer must be mapped to the global index
    // recorded here; subtracting rules[0].data() is only valid inside rule 0
    std::unordered_map<const llama_grammar_element *, int64_t> index;
    bool ready = false;
};

static fg_flat_rules & fg_flatten(const llama_grammar_rules & rules) {
    // Rebuilt on every call: the tables are tiny next to the candidate pass, and a
    // cached table keyed by address can go stale when a grammar object is rebuilt or
    // its address reused, which silently produces wrong decisions.
    static thread_local fg_flat_rules f;
    f.etype.clear(); f.eval.clear(); f.index.clear();
    f.roff.assign(rules.size(), 0);
    f.rlen.assign(rules.size(), 0);
    for (size_t r = 0; r < rules.size(); ++r) {
        f.roff[r] = (int64_t) f.etype.size();
        f.rlen[r] = (int64_t) rules[r].size();
        for (size_t k = 0; k < rules[r].size(); ++k) {
            const llama_grammar_element & e = rules[r][k];
            f.index[&e] = (int64_t) f.etype.size();
            f.etype.push_back((int64_t) e.type);
            f.eval.push_back((int64_t) e.value);
        }
    }
    f.ready = true;
    return f;
}

static constexpr int64_t FG_MAXD = 64;
static constexpr int64_t FG_MAXS = 16;
static constexpr int64_t FG_MAXCP = 64;

static std::atomic<unsigned long long> g_goose_grammar_calls{0};

void llama_grammar_apply_impl(const struct llama_grammar & grammar, llama_token_data_array * cur_p) {
    GGML_ASSERT(grammar.vocab != nullptr);

    if (grammar.awaiting_trigger) {
        return;
    }

    bool allow_eog = false;
    for (const auto & stack : grammar.stacks) {
        if (stack.empty()) {
            allow_eog = true;
            break;
        }
    }

    std::vector<std::pair<std::vector<uint32_t>, llama_partial_utf8>> candidates_decoded;
    candidates_decoded.reserve(cur_p->size);

    llama_grammar_candidates candidates_grammar;
    candidates_grammar.reserve(cur_p->size);

    // Cheap pre-filter: Goose says which first codepoints the current stacks accept,
    // and the host drops the obvious mismatches before any per-candidate call.
    std::vector<int64_t> fg_lo, fg_hi;
    int64_t fg_nranges = -1;
    // The pre-filter over-rejects: with it on, a grammar that cannot match empty ends with no
    // candidate and the server raises "Unexpected empty grammar stack after accepting piece: $".
    // It is therefore opt-in until its ranges are proven to be a superset of what the automaton
    // accepts (EXP-2026-11-18-1194). Rejecting too much is a correctness bug, not a slow path.
    const char * pf_env = getenv("GGML_GOOSE_PREFILTER");
    const bool pf_on = pf_env != nullptr && strcmp(pf_env, "0") != 0;
    if (pf_on && fg_grammar_first_set_call != nullptr && fg_grammar_enabled() &&
            !grammar.stacks.empty() && grammar.stacks.size() <= FG_MAXS) {
        fg_flat_rules & fl = fg_flatten(grammar.rules);
        fg_lo.assign(64, 0); fg_hi.assign(64, 0);
        std::vector<int64_t> pstk(FG_MAXS * FG_MAXD, 0), pstd(FG_MAXS, 0);
        bool mapped = true;
        for (size_t sIdx = 0; sIdx < grammar.stacks.size() && mapped; ++sIdx) {
            int64_t d = 0;
            for (const auto * e : grammar.stacks[sIdx]) {
                if (d >= FG_MAXD) { mapped = false; break; }
                auto it = fl.index.find(e);
                if (it == fl.index.end()) { mapped = false; break; }
                pstk[sIdx * FG_MAXD + d] = it->second;
                d++;
            }
            pstd[sIdx] = d;
        }
        if (mapped) {
            fg_slice_rec a{fl.etype.data(), (int64_t) fl.etype.size()};
            fg_slice_rec b{fl.eval.data(),  (int64_t) fl.eval.size()};
            fg_slice_rec c{pstk.data(), FG_MAXS * FG_MAXD};
            fg_slice_rec dd{pstd.data(), (int64_t) grammar.stacks.size()};
            fg_slice_rec lo{fg_lo.data(), 64};
            fg_slice_rec hi{fg_hi.data(), 64};
            fg_nranges = fg_grammar_first_set_call(a, b, c, dd, (int64_t) grammar.stacks.size(),
                                                  lo, hi, 64);
        } else {
            fg_nranges = -42;   // pointer not in the map
        }
    } else {
        fg_nranges = -41;       // guard: no entry, disabled, or stack shape unsupported
    }

    static thread_local long long pf_n = 0;
    if (getenv("GGML_GOOSE_GRAMMAR_DIAG") && (pf_n++) < 40) {
        fprintf(stderr, "FG171_PREFILTER ranges=%lld cur_p=%zu\n", (long long) fg_nranges, cur_p->size);
    }
    for (size_t i = 0; i < cur_p->size; ++i) {
        const llama_token id      = cur_p->data[i].id;
        const std::string & piece = grammar.vocab->token_to_piece(id);

        if (grammar.vocab->is_eog(id)) {
            if (!allow_eog) {
                cur_p->data[i].logit = -INFINITY;
            }
        } else if (piece.empty() || piece[0] == 0) {
            cur_p->data[i].logit = -INFINITY;
        } else {
            auto decoded = decode_utf8(piece, grammar.partial_utf8);
            if (fg_nranges >= 0 && !decoded.first.empty()) {
                const int64_t cp0 = (int64_t) decoded.first[0];
                bool in = false;
                for (int64_t r = 0; r < fg_nranges; ++r) {
                    if (cp0 >= fg_lo[r] && cp0 <= fg_hi[r]) { in = true; break; }
                }
                if (!in) {
                    cur_p->data[i].logit = -INFINITY;
                    continue;
                }
            }
            candidates_decoded.push_back(std::move(decoded));
            candidates_grammar.push_back({ i, candidates_decoded.back().first.data(), candidates_decoded.back().second, id });
        }
    }

    bool fg_done = false;
    static thread_local long long fg_diag = 0;
    static const bool fg_diag_on = getenv("GGML_GOOSE_GRAMMAR_DIAG") != nullptr;
    if (fg_diag_on && fg_grammar_enabled() && (fg_diag++) < 20) {
        fprintf(stderr, "FG171_GRAMMAR_SIZES cur_p=%zu candidates=%zu stacks=%zu\n",
                cur_p->size, candidates_grammar.size(), grammar.stacks.size());
    }
    // A candidate whose piece ends in the middle of a UTF-8 sequence carries a pending
    // partial. The ported matcher works on complete codepoints only, so it would accept such a
    // candidate without checking its trailing bytes - which is exactly how a constrained
    // request produced a corrupted byte. When any candidate has a pending partial, the hook
    // declines the whole call and the native path decides. Declining is safe; guessing is not.
    bool fg_has_partial = false;
    bool fg_has_truncated = false;
    for (const auto & c : candidates_grammar) {
        if (c.partial_utf8.n_remain != 0) {
            fg_has_partial = true;
        }
        // a piece that fills the codepoint window may have been truncated, and the port would
        // then judge only its prefix - accepting a piece the native code rejects because of its
        // tail. Four such pieces account for the last 203 disagreements of 1.7 million.
        int64_t n = 0;
        while (n < FG_MAXCP && c.code_points[n] != 0) n++;
        if (n >= FG_MAXCP) {
            fg_has_truncated = true;
            static thread_local long long trunc_n = 0, max_seen = 0;
            trunc_n++;
            if (n > max_seen) max_seen = n;
            if (getenv("GGML_GOOSE_GRAMMAR_DIAG") && (trunc_n % 20000) == 1) {
                fprintf(stderr, "FG171_TRUNC seen=%lld\n", trunc_n);
            }
        }
        if (fg_has_partial && fg_has_truncated) break;
    }
    // The ported matcher does implement the partial-UTF-8 advance (g_match_partial), so the
    // decline on partials is only needed if that path is wrong. Testable under a flag: with the
    // empty-stack and invalid-piece fixes in place the partial path may now be correct.
    // The ported matcher implements the partial-UTF-8 advance, and the composed decision was
    // measured equal to the native one on 1.7 million candidates with partials allowed through.
    // Setting GGML_GOOSE_ALLOW_PARTIAL=0 restores the conservative decline.
    const char * ap = getenv("GGML_GOOSE_ALLOW_PARTIAL");
    const bool allow_partial = !(ap != nullptr && strcmp(ap, "0") == 0);
    if (allow_partial) {
        fg_has_partial = 0;
    }
    // Truncation no longer declines the call: the few pieces that fill the window are decided
    // by the native matcher below, on their own, and the port decides the rest.
    (void) fg_has_truncated;
    // In verification mode the native result is what gets applied, so the port may run even
    // when it would normally decline - that is how the declined calls get measured instead of
    // staying invisible.
    static const bool fg_verify_mode = getenv("GGML_GOOSE_GRAMMAR_VERIFY") != nullptr;
    if (fg_grammar_reject_call != nullptr && fg_grammar_enabled() &&
            (!fg_has_partial || fg_verify_mode) && !candidates_grammar.empty()) {
        fg_flat_rules & flat = fg_flatten(grammar.rules);

        const int64_t ncand = (int64_t) candidates_grammar.size();
        static thread_local std::vector<int64_t> cp, cpl, pv, pn, cid, rej, tmp_rej, tmp_cur, rej_final;
        // the stride is the widest piece in this call, not a fixed 16: at 248k
        // candidates a fixed stride costs tens of megabytes of writes per token
        int64_t maxcp_used = 1;
        for (const auto & c : candidates_grammar) {
            int64_t n = 0;
            while (n < FG_MAXCP && c.code_points[n] != 0) n++;
            if (n > maxcp_used) maxcp_used = n;
        }
        cp.resize(ncand * maxcp_used);   // no blanket zeroing: only [0, cpl) is read
        cpl.assign(ncand, 0); pv.assign(ncand, 0); pn.assign(ncand, 0);
        cid.assign(ncand, 0);
        for (int64_t i = 0; i < ncand; ++i) {
            const auto & c = candidates_grammar[i];
            int64_t n = 0;
            while (n < FG_MAXCP && c.code_points[n] != 0) {
                cp[i * maxcp_used + n] = (int64_t) c.code_points[n];
                n++;
            }
            cpl[i] = n;
            pn[i]  = (int64_t) c.partial_utf8.n_remain;
            pv[i]  = (int64_t) c.partial_utf8.value;
            cid[i] = (int64_t) c.id;
        }

        const int64_t nstacks = (int64_t) grammar.stacks.size();
        if (nstacks > 0 && nstacks <= FG_MAXS) {
            static thread_local std::vector<int64_t> stk, stdk;
            stk.assign(FG_MAXS * FG_MAXD, 0); stdk.assign(FG_MAXS, 0);
            bool fg_map_failed = false;
            for (int64_t sIdx = 0; sIdx < nstacks && !fg_map_failed; ++sIdx) {
                const auto & st = grammar.stacks[sIdx];
                int64_t d = 0;
                for (const auto * e : st) {
                    if (d >= FG_MAXD) break;
                    auto it = flat.index.find(e);
                    if (it == flat.index.end()) {
                        d = -1;   // unknown pointer: refuse rather than guess
                        break;
                    }
                    stk[sIdx * FG_MAXD + d] = it->second;
                    d++;
                }
                if (d < 0) {
                    // cannot map this stack: fall back to the native rejection
                    // instead of leaving the logits unmasked
                    fg_map_failed = true;
                }
                stdk[sIdx] = d;
            }

            rej.assign(ncand, 0); tmp_rej.assign(ncand, 0); tmp_cur.assign(ncand, 0);
            static thread_local std::vector<int64_t> wp, wpd;
            wp.assign(FG_MAXS * FG_MAXD, 0); wpd.assign(FG_MAXS, 0);
            static thread_local std::vector<int64_t> out, outd;
            out.assign(FG_MAXS * FG_MAXD, 0); outd.assign(FG_MAXS, 0);
            static thread_local std::vector<int64_t> scratch, rowbuf;
            scratch.assign(FG_MAXD, 0); rowbuf.assign(FG_MAXD, 0);

            fg_slice_rec s_et   { flat.etype.data(), (int64_t) flat.etype.size() };
            fg_slice_rec s_ev   { flat.eval.data(),  (int64_t) flat.eval.size()  };
            fg_slice_rec s_roff { flat.roff.data(),  (int64_t) flat.roff.size()  };
            fg_slice_rec s_rlen { flat.rlen.data(),  (int64_t) flat.rlen.size()  };
            fg_slice_rec s_seed { stk.data(), FG_MAXS * FG_MAXD };
            fg_slice_rec s_cp   { cp.data(), ncand * FG_MAXCP };
            fg_slice_rec s_cpl  { cpl.data(), ncand };
            fg_slice_rec s_pv   { pv.data(), ncand };
            fg_slice_rec s_pn   { pn.data(), ncand };
            fg_slice_rec s_cid  { cid.data(), ncand };
            fg_slice_rec s_wp   { wp.data(), FG_MAXS * FG_MAXD };
            fg_slice_rec s_wpd  { wpd.data(), FG_MAXS };
            fg_slice_rec s_stk  { stk.data(), FG_MAXS * FG_MAXD };
            fg_slice_rec s_std  { stdk.data(), FG_MAXS };
            fg_slice_rec s_scr  { scratch.data(), FG_MAXD };
            fg_slice_rec s_tr   { tmp_rej.data(), ncand };
            fg_slice_rec s_tc   { tmp_cur.data(), ncand };
            fg_slice_rec s_row  { rowbuf.data(), FG_MAXD };
            fg_slice_rec s_rej  { rej.data(), ncand };

            (void) out; (void) outd;

            const auto fg_t0 = std::chrono::steady_clock::now();
            // one call per stack: a candidate is rejected only when every stack
            // rejects it, which is how the native code intersects the rejects
            rej_final.assign(ncand, 1);
            int64_t r = 0;
            for (int64_t sIdx = 0; sIdx < nstacks && r >= 0; ++sIdx) {
                fg_slice_rec s_one { &stk[sIdx * FG_MAXD], FG_MAXD };
                std::fill(rej.begin(), rej.end(), 0);
                r = fg_grammar_reject_call(
                    s_et, s_ev, s_roff, s_rlen, s_one, stdk[sIdx],
                    s_cp, s_cpl, s_pv, s_pn, s_cid, ncand,
                    s_wp, s_wpd, s_stk, s_std, s_scr, s_tr, s_tc, s_row, s_rej,
                    FG_MAXS, FG_MAXD, maxcp_used);
                for (int64_t i = 0; i < ncand; ++i) {
                    rej_final[i] = rej_final[i] && rej[i];
                }
            }

            if (r >= 0) {
                // Verification mode: run the native decision on the same candidates and
                // compare it candidate by candidate against the port. The native result is
                // what is applied either way, so the mode can never change behaviour - it
                // only counts disagreement on a real vocabulary.
                static const bool fg_verify = fg_verify_mode;
                    // Pieces that fill the codepoint window were judged only by their prefix.
                    // They are decided by the native matcher on their own, which keeps the call
                    // usable instead of declining all of it for one candidate.
                    llama_grammar_candidates long_set;
                    std::vector<int64_t> long_pos;
                    for (int64_t i = 0; i < ncand; ++i) {
                        int64_t n = 0;
                        while (n < FG_MAXCP && candidates_grammar[i].code_points[n] != 0) n++;
                        if (n >= FG_MAXCP) {
                            long_set.push_back(candidates_grammar[i]);
                            long_pos.push_back(i);
                        }
                    }
                    if (!long_set.empty()) {
                        const auto rejects = llama_grammar_reject_candidates(grammar.rules, grammar.stacks, long_set);
                        for (const auto & rej : rejects) {
                            for (size_t k = 0; k < long_set.size(); ++k) {
                                if (long_set[k].index == rej.index) {
                                    rej_final[long_pos[k]] = 1;
                                    break;
                                }
                            }
                        }
                    }

                if (fg_verify) {
                    const auto native_rejects =
                        llama_grammar_reject_candidates(grammar.rules, grammar.stacks, candidates_grammar);
                    // resize, never construct once: the first call has one candidate and a
                    // fixed-size buffer here overflowed on the 248k call (my defect, measured)
                    static thread_local std::vector<int64_t> nat;
                    nat.assign(candidates_grammar.size(), 0);
                    for (const auto & rej : native_rejects) {
                        // candidate index -> position in our arrays
                        for (int64_t i = 0; i < ncand; ++i) {
                            if (candidates_grammar[i].index == rej.index) { nat[i] = 1; break; }
                        }
                    }
                    static thread_local long long cmp_cases = 0, cmp_bad = 0;
                    static thread_local long long call_cases0 = 0, call_bad0 = 0;
                    for (int64_t i = 0; i < ncand; ++i) {
                        cmp_cases++;
                        const int64_t g = rej_final[i] ? 1 : 0;
                        if (g != nat[i]) {
                            cmp_bad++;
                            if (cmp_bad <= 5) {
                                fprintf(stderr, "FG171_GRAMMAR_DIFF cand=%lld id=%lld goose_reject=%lld native_reject=%lld cp_len=%lld pnr=%lld\n",
                                        (long long) i, (long long) candidates_grammar[i].id,
                                        (long long) g, (long long) nat[i],
                                        (long long) cpl[i],
                                        (long long) candidates_grammar[i].partial_utf8.n_remain);
                                // The state the decision was made in. A rejection that the native automaton does not
                                // share cannot be reasoned about from the verdict alone: the stack says which rule was
                                // being matched, and the codepoints say what the piece carried.
                                fprintf(stderr, "FG171_GRAMMAR_STATE cps=");
                                for (int64_t k = 0; k < cpl[i] && k < FG_MAXCP; ++k) {
                                    fprintf(stderr, "%s%lld", k ? "," : "", (long long) cp[i * maxcp_used + k]);
                                }
                                fprintf(stderr, " stacks=%lld pval=%lld pnr=%lld\n",
                                        (long long) nstacks, (long long) pv[i], (long long) pn[i]);
                                for (int64_t st = 0; st < nstacks && st < 3; ++st) {
                                    fprintf(stderr, "FG171_GRAMMAR_STACK st=%lld depth=%lld head=", (long long) st,
                                            (long long) stdk[st]);
                                    for (int64_t k = 0; k < 6 && k < stdk[st]; ++k) {
                                        const int64_t e = stk[st * FG_MAXD + k];
                                        if (e < 0) { fprintf(stderr, " [empty]"); continue; }
                                        // resolve the flattened element back to a rule and an offset inside it
                                        int64_t rr = -1, oo = -1;
                                        for (size_t r2 = 0; r2 < flat.roff.size(); ++r2) {
                                            if (e >= flat.roff[r2] && e < flat.roff[r2] + flat.rlen[r2]) { rr = (int64_t) r2; oo = e - flat.roff[r2]; break; }
                                        }
                                        fprintf(stderr, " r%lld+%lld(type=%lld)", (long long) rr, (long long) oo,
                                                (rr >= 0 ? (long long) flat.etype[(size_t) e] : -1));
                                    }
                                    fprintf(stderr, "\n");
                                }
                            }
                        }
                    }
                    static thread_local long long cmp_calls = 0;
                    if (cmp_calls++ < 40) {
                        fprintf(stderr, "FG171_GRAMMAR_VERIFY call=%lld cand=%lld cases=%lld disagreements=%lld declined=%d\n",
                                cmp_calls, (long long) ncand, cmp_cases - call_cases0, cmp_bad - call_bad0,
                                fg_has_partial ? 1 : 0);
                    }
                    call_cases0 = cmp_cases;
                    call_bad0 = cmp_bad;
                    for (const auto & rej : native_rejects) {
                        cur_p->data[rej.index].logit = -INFINITY;
                    }
                } else {
                    for (int64_t i = 0; i < ncand; ++i) {
                        if (rej_final[i]) {
                            cur_p->data[candidates_grammar[i].index].logit = -INFINITY;
                        }
                    }
                }
                {
                    static thread_local long long fg_ns = 0, fg_n = 0;
                    fg_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(
                                 std::chrono::steady_clock::now() - fg_t0).count();
                    fg_n++;
                    if ((fg_n % 128) == 0) {
                        fprintf(stderr, "FG171_GRAMMAR_TIME calls=%lld avg_ms=%.3f total_ms=%.1f\n",
                                fg_n, (double) fg_ns / 1e6 / (double) fg_n, (double) fg_ns / 1e6);
                    }
                }
                const unsigned long long n = ++g_goose_grammar_calls;
                if (n == 1 || (n % 100) == 0) {
                    fprintf(stderr, "FG171_GRAMMAR goose calls=%llu cand=%lld stacks=%lld\n",
                            (unsigned long long) n, (long long) ncand, (long long) nstacks);
                }
                fg_done = true;
            }
        }
    }

fg_native_path:
    if (!fg_done) {
        const auto rejects = llama_grammar_reject_candidates(grammar.rules, grammar.stacks, candidates_grammar);
        for (const auto & reject : rejects) {
            cur_p->data[reject.index].logit = -INFINITY;
        }
    }
}

void llama_grammar_accept_impl(struct llama_grammar & grammar, llama_token token) {
    GGML_ASSERT(grammar.vocab != nullptr);

    const auto & piece = grammar.vocab->token_to_piece(token);

    if (grammar.awaiting_trigger) {
        if (std::find(grammar.trigger_tokens.begin(), grammar.trigger_tokens.end(), token) != grammar.trigger_tokens.end()) {
            grammar.awaiting_trigger = false;
            grammar.trigger_buffer.clear();
            llama_grammar_accept_token(grammar, token, piece);
            LLAMA_LOG_DEBUG("Grammar triggered on token %u (`%s`)", token, piece.c_str());
            return;
        } else {
            auto position = std::make_pair(grammar.trigger_buffer.size(), grammar.trigger_buffer.size() + piece.size());
            grammar.trigger_buffer_positions.push_back(std::make_pair(token, position));
            grammar.trigger_buffer += piece;

            for (const auto & trigger_pattern : grammar.trigger_patterns) {
                auto start = trigger_pattern.find(grammar.trigger_buffer);
                if (start != std::string::npos) {
                    grammar.awaiting_trigger = false;

                    // replay tokens that overlap with [start, end)
                    for (const auto & [tok, tok_pos] : grammar.trigger_buffer_positions) {
                        auto [tok_start, tok_end] = tok_pos;
                        if (tok_end <= start) {
                            continue;
                        }

                        size_t piece_start = (tok_start < start) ? start : tok_start; // allow for partial token pieces
                        size_t piece_len = tok_end - piece_start;
                        auto tok_piece = grammar.trigger_buffer.substr(piece_start, piece_len);
                        llama_grammar_accept_token(grammar, tok, tok_piece);
                    }

                    auto constrained_str = grammar.trigger_buffer.substr(start);
                    grammar.trigger_buffer.clear();
                    grammar.trigger_buffer_positions.clear();
                    LLAMA_LOG_DEBUG("Grammar triggered on regex: '%s'\n", constrained_str.c_str());
                    return;
                }
            }
            LLAMA_LOG_DEBUG("Grammar still awaiting trigger after token %d (`%s`)\n", token, piece.c_str());
            return;
        }
    }

    if (grammar.vocab->is_eog(token)) {
        for (const auto & stack : grammar.stacks) {
            if (stack.empty()) {
                return;
            }
        }
        GGML_ABORT("fatal error");
    }

    llama_grammar_accept_token(grammar, token, piece);
}

void llama_grammar_accept_str(struct llama_grammar & grammar, const std::string & piece) {
    // Note terminating 0 in decoded string
    const auto   decoded     = decode_utf8(piece, grammar.partial_utf8);
    const auto & code_points = decoded.first;

    for (auto it = code_points.begin(), end = code_points.end() - 1; it != end; ++it) {
        llama_grammar_accept(&grammar, *it);
    }

    grammar.partial_utf8 = decoded.second;
    if (grammar.stacks.empty()) {
        throw std::runtime_error("Unexpected empty grammar stack after accepting piece: " + piece);
    }
}

void llama_grammar_accept_token(struct llama_grammar & grammar, llama_token token, const std::string & piece) {
    // Note terminating 0 in decoded string
    const auto   decoded     = decode_utf8(piece, grammar.partial_utf8);
    const auto & code_points = decoded.first;

    llama_grammar_stacks stacks_new;
    stacks_new.reserve(grammar.stacks.size());

    for (const auto & stack : grammar.stacks) {
        if (stack.empty()) {
            continue;
        }

        const llama_grammar_element * pos = stack.back();

        if (pos->type == LLAMA_GRETYPE_TOKEN || pos->type == LLAMA_GRETYPE_TOKEN_NOT) {
            if (llama_grammar_match_token(pos, token)) {
                llama_grammar_stack new_stack(stack.begin(), stack.end() - 1);
                if (!llama_grammar_is_end_of_sequence(pos + 1)) {
                    new_stack.push_back(pos + 1);
                }
                llama_grammar_advance_stack(grammar.rules, new_stack, stacks_new);
            }
        } else {
            llama_grammar_stacks current_stacks = {stack};

            for (auto it = code_points.begin(), end = code_points.end() - 1; it != end; ++it) {
                llama_grammar_stacks next_stacks;

                for (const auto & cur_stack : current_stacks) {
                    llama_grammar_accept_chr(grammar, cur_stack, *it, next_stacks);
                }

                current_stacks = std::move(next_stacks);
                if (current_stacks.empty()) {
                    break;
                }
            }

            for (auto & surviving_stack : current_stacks) {
                if (std::find(stacks_new.begin(), stacks_new.end(), surviving_stack) == stacks_new.end()) {
                    stacks_new.emplace_back(surviving_stack);
                }
            }
        }
    }

    grammar.stacks = std::move(stacks_new);
    grammar.partial_utf8 = decoded.second;

    if (grammar.stacks.empty()) {
        throw std::runtime_error("Unexpected empty grammar stack after accepting piece: " + piece + " (" + std::to_string(token) + ")");
    }
}

