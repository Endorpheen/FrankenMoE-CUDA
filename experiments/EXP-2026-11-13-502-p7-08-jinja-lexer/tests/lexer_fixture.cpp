// Differential fixture for the Jinja lexer (EXP502).
// Oracle: jinja::lexer::tokenize from common/jinja/lexer.cpp.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "lexer-gen.h"
#include "jinja/lexer.h"

// the debug flag the lexer's JJ_DEBUG macro references
bool g_jinja_debug = false;

static int cases = 0, values = 0, fails = 0;
static void bad(const char * what, const std::string & detail) {
    ++fails;
    if (fails <= 12) printf("FAIL %s: %s\n", what, detail.c_str());
}
static sl_u8 sl(const std::string & s) { return sl_u8{ (uint8_t *) s.data(), (int64_t) s.size() }; }
static sl_u8 slv(std::vector<uint8_t> & v) { return sl_u8{ v.data(), (int64_t) v.size() }; }
static sl_i64 sli(std::vector<int64_t> & v) { return sl_i64{ v.data(), (int64_t) v.size() }; }

static std::string visible(const std::string & s) {
    std::string r;
    for (unsigned char c : s) {
        if (c >= 32 && c < 127) r.push_back((char) c);
        else { char b[16]; snprintf(b, sizeof b, "\\x%02X", c); r += b; }
    }
    return r;
}

static const char * type_name(int t) {
    static const char * names[] = {
        "eof", "text", "numeric_literal", "string_literal", "identifier", "equals",
        "open_paren", "close_paren", "open_statement", "close_statement",
        "open_expression", "close_expression", "open_square_bracket",
        "close_square_bracket", "open_curly_bracket", "close_curly_bracket",
        "comma", "dot", "colon", "pipe", "call_operator", "additive_binary_operator",
        "multiplicative_binary_operator", "comparison_binary_operator", "unary_operator", "comment",
    };
    if (t >= 0 && t < 26) return names[t];
    return "unknown";
}

static void check(const std::string & source, const std::string & label) {
    // native
    jinja::lexer_result nres;
    std::string nerr;
    bool nok = true;
    try {
        jinja::lexer lx;
        nres = lx.tokenize(source);
    } catch (const std::exception & e) {
        nok = false;
        nerr = e.what();
    }

    // port
    const int maxn = 1 << 18;
    std::vector<int64_t> tt(maxn, -1), voff(maxn, 0), vlen(maxn, 0), tpos(maxn, 0), st(16, 0);
    std::vector<uint8_t> out(1 << 22, 0), norm(1 << 22, 0), errb(1 << 16, 0);
    ++cases;
    const int64_t g = jinja_lex(sl(source), sli(tt), sli(voff), sli(vlen), sli(tpos),
        slv(out), slv(norm), slv(errb), sli(st));
    const std::string ctx = "source=[" + visible(source) + "] " + label;

    if (!nok) {
        ++values;
        if (g != -1 || st[1] == 0) { bad("expected rejection", ctx); return; }
        ++values;
        const std::string gerr((const char *) errb.data(), (size_t) st[2]);
        if (gerr != nerr) {
            bad("error text", ctx + "\n  got =[" + visible(gerr) + "]\n  want=[" + visible(nerr) + "]");
        }
        return;
    }

    ++values;
    if (g < 0) {
        const std::string gerr((const char *) errb.data(), (size_t) st[2]);
        bad("unexpected rejection", ctx + ": " + visible(gerr));
        return;
    }
    ++values;
    if ((size_t) g != nres.tokens.size()) {
        bad("token count", ctx + " goose=" + std::to_string(g) + " native=" + std::to_string(nres.tokens.size()));
        return;
    }
    for (int64_t i = 0; i < g; ++i) {
        const auto & nt = nres.tokens[(size_t) i];
        const std::string got((const char *) out.data() + voff[i], (size_t) vlen[i]);
        const std::string tc = ctx + " token " + std::to_string(i) + " (" + type_name(nt.t) + ")";
        ++values;
        if (tt[i] != (int64_t) nt.t) {
            bad("type", tc + " goose=" + type_name((int) tt[i]) + " native=" + type_name((int) nt.t));
            return;
        }
        ++values;
        if (got != nt.value) {
            bad("value", tc + " got=[" + visible(got) + "] want=[" + visible(nt.value) + "]");
            return;
        }
        ++values;
        if (tpos[i] != (int64_t) nt.pos) {
            bad("pos", tc + " goose=" + std::to_string(tpos[i]) + " native=" + std::to_string(nt.pos));
            return;
        }
    }
    // the normalized source is part of the native result
    ++values;
    const std::string gnorm((const char *) norm.data(), (size_t) st[8]);
    if (gnorm != nres.source) {
        bad("normalized source", ctx + "\n  got =[" + visible(gnorm) + "]\n  want=[" + visible(nres.source) + "]");
    }
}

int main(void) {
    goose_init(0, 0);

    const std::vector<std::pair<std::string, std::string>> corpus = {
        {"", "empty"},
        {" ", "space"},
        {"\n", "newline only"},
        {"\r\n", "crlf only"},
        {"\r", "cr only"},
        {"\n\n", "two newlines"},
        {"a", "one char"},
        {"abc", "text"},
        {"a\n", "text trailing newline"},
        {"a\n\n", "text two trailing newlines"},
        {"a\r\nb", "crlf inside"},
        {"a\rb", "cr inside"},
        {"a\r\n", "crlf at end"},
        {"a\r", "cr at end"},
        {"a{", "lone brace at end"},
        {"{", "lone brace"},
        {"{{", "open expression only"},
        {"{{-", "trimmed open expression"},
        {"-%}", "trim close at start"},
        {"{{ x }}", "simple expression"},
        {"{{x}}", "tight expression"},
        {"{{ x + 1 }}", "additive"},
        {"{{- x -}}", "trimmed expression"},
        {"{{ x|upper }}", "pipe"},
        {"{{ a.b[0] }}", "member and index"},
        {"{{ fn(1, 2) }}", "call"},
        {"{{ {'a': 1} }}", "object literal"},
        {"{{ {'a': 1} }} }}", "object then close"},
        {"{{ x }}tail", "expression then text"},
        {"head{{ x }}", "text then expression"},
        {"{% if x %}a{% endif %}", "statement"},
        {"{%- if x -%}\na\n{%- endif -%}", "trimmed statement"},
        {"{# c #}", "comment"},
        {"{# multi\nline #}", "multiline comment"},
        {"a{# c #}b", "comment between text"},
        {"{#\n#}", "comment newline"},
        {"a\n   {% if x %}", "lstrip blocks"},
        {"{% if x %}\n  a", "trim blocks"},
        {"a  {{- x }}", "rstrip previous"},
        {"{{ x -}}  b", "lstrip current"},
        {"{{ 1 }}", "integer"},
        {"{{ 1.5 }}", "float"},
        {"{{ 1. }}", "dot without fraction"},
        {"{{ 1..2 }}", "double dot"},
        {"{{ 0 }}", "zero"},
        {"{{ -1 }}", "unary minus number"},
        {"{{ +1 }}", "unary plus number"},
        {"{{ - 1 }}", "unary minus spaced"},
        {"{{ a - 1 }}", "binary minus"},
        {"{{ (1) - 1 }}", "paren then binary minus"},
        {"{{ a[1] - 1 }}", "index then binary minus"},
        {"{{ -a }}", "unary minus identifier"},
        {"{{ not -1 }}", "not unary"},
        {"{{ not not x }}", "double not"},
        {"{{ \"a\\nb\" }}", "string newline escape"},
        {"{{ \"a\\x41b\" }}", "string hex escape"},
        {"{{ \"a\\x80b\" }}", "string high hex escape"},
        {"{{ \"a\\\\b\" }}", "string backslash escape"},
        {"{{ 'a\\'b' }}", "string quote escape"},
        {"{{ \"a\\tb\" }}", "string tab escape"},
        {"{{ a<=b }}", "comparison le"},
        {"{{ a>=b }}", "comparison ge"},
        {"{{ a==b }}", "comparison eq"},
        {"{{ a!=b }}", "comparison ne"},
        {"{{ a<b }}", "comparison lt"},
        {"{{ a>b }}", "comparison gt"},
        {"{{ a~b }}", "tilde"},
        {"{{ a*b }}", "multiply"},
        {"{{ a/b }}", "divide"},
        {"{{ a%b }}", "modulo"},
        {"{{ a=b }}", "equals"},
        {"{{}}", "empty expression"},
        {"{% %}", "empty statement"},
        {"{%%}", "tight empty statement"},
        {"{{ { } }}", "empty object"},
        {"{{ {a: 1} }}", "object with identifier key"},
        {"{{ { {{ x }}", "depth reset on a new expression"},
        {"{{ {}}", "close expression inside a brace"},
        {"{{ {'a': 1}}}", "object immediately before the expression end"},
        {"{%-- x %}", "dash after a statement marker"},
        {"{{-- x }}", "dash after an expression marker"},
        {"{{ a_b }}", "identifier with underscore"},
        {"{{ \"\xc3\xa9\" }}", "utf8 in string"},
        {"{{\xc3\xa9}}", "utf8 identifier byte"},
        {"{{\vx\v}}", "vertical tab whitespace"},
        {"{{ x }}\n", "trailing newline"},
        {"{{ x }}\r\n", "trailing crlf"},
        {"{% raw %}x{% endraw %}", "raw block"},
        {"a{# c #}\n  b", "comment then trim"},
        // rejected
        {"{{ @ }}", "unexpected at"},
        {"{{ \"abc }}", "unterminated string"},
        {"{{ \"a\\", "unterminated escape"},
        {"{{ \"\\x4\" }}", "incomplete hex"},
        {"{{ \"\\xZZ\" }}", "invalid hex"},
        {"{{ \"\\q\" }}", "unknown escape"},
        {"{# unclosed", "unclosed comment"},
        {"{#", "comment opener only"},
        {"a{{ \"\\x\" }}", "incomplete hex after text"},
        {"{{ 1 @ 2 }}", "unexpected between numbers"},
        {"{{ \\ }}", "lone backslash outside string"},
    };
    for (const auto & [src, label] : corpus) check(src, label);

    { // 5000 expressions
        std::string s;
        for (int i = 0; i < 5000; ++i) s += "{{ x }}";
        check(s, "5000 expressions");
    }
    { // 200000 bytes of text
        check(std::string(200000, 'a'), "200k text");
    }
    { // a long identifier
        check("{{ " + std::string(50000, 'a') + " }}", "50k identifier");
    }
    { // a long string literal
        check("{{ \"" + std::string(50000, 'a') + "\" }}", "50k string");
    }
    { // 5000 statement lines with trim
        std::string s;
        for (int i = 0; i < 5000; ++i) s += "  {%- if x -%}\n";
        check(s, "5000 trimmed statements");
    }
    { // 500 nested curly brackets inside an expression
        std::string s = "{{ ";
        s += std::string(500, '{');
        s += std::string(500, '}');
        s += " }}";
        check(s, "500 nested braces");
    }
    { // a deep stack of nested groups in text boundaries
        std::string s;
        for (int i = 0; i < 2000; ++i) s += "a{{ b }}";
        check(s, "2000 text/expression pairs");
    }

    printf("FG502_LEXER cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails ? 1 : 0;
}
