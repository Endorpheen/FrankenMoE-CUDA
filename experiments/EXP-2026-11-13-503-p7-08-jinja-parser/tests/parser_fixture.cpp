// Differential fixture for the Jinja parser (EXP503).
// Oracle: jinja::parse_from_tokens from common/jinja/parser.cpp.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <map>

#include "parser-gen.h"
#include "jinja/lexer.h"
#include "jinja/parser.h"
#include "jinja/runtime.h"

// the number primitive the port borrows, as in EXP493
extern "C" double gs_json_strtod(sl_u8 s) {
    std::string tmp((const char *) s.data, (size_t) s.len);
    return std::strtod(tmp.c_str(), nullptr);
}

static int cases = 0, values = 0, fails = 0;
static void bad(const char * what, const std::string & detail) {
    ++fails;
    if (fails <= 8) printf("FAIL %s: %s\n", what, detail.c_str());
}
static sl_u8 sl(const std::string & s) { return sl_u8{ (uint8_t *) s.data(), (int64_t) s.size() }; }
static sl_u8 slv(std::vector<uint8_t> & v) { return sl_u8{ v.data(), (int64_t) v.size() }; }
static sl_i64 sli(std::vector<int64_t> & v) { return sl_i64{ v.data(), (int64_t) v.size() }; }
static sl_f64 slf(std::vector<double> & v) { return sl_f64{ v.data(), (int64_t) v.size() }; }

static std::string visible(const std::string & s, size_t max = 200) {
    std::string r;
    size_t n = 0;
    for (unsigned char c : s) {
        if (n++ > max) { r += "..."; break; }
        if (c == '"') r += "\\\"";
        else if (c == '\\') r += "\\\\";
        else if (c == '\n') r += "\\n";
        else if (c == '\r') r += "\\r";
        else if (c == '\t') r += "\\t";
        else if (c >= 32 && c < 127) r.push_back((char) c);
        else { char b[8]; snprintf(b, sizeof b, "\\x%02X", c); r += b; }
    }
    return r;
}
static std::string q(const std::string & s) { return "\"" + visible(s) + "\""; }

struct Out {
    std::string s;
    void line(int ind, const std::string & t) { s.append((size_t) ind * 2, ' '); s += t; s += '\n'; }
    void fld(int ind, const char * n) { line(ind, std::string(".") + n + ":"); }
};

// ---- native dump -----------------------------------------------------------
static void dn(const jinja::statement * p, int ind, Out & o) {
    if (!p) { o.line(ind, "null"); return; }
    std::string head = p->type() + " pos=" + std::to_string(p->pos);
    if (auto x = dynamic_cast<const jinja::comment_statement *>(p)) head += " val=" + q(x->val);
    else if (auto x = dynamic_cast<const jinja::identifier *>(p)) head += " val=" + q(x->val);
    else if (auto x = dynamic_cast<const jinja::string_literal *>(p)) head += " val=" + q(x->val);
    else if (auto x = dynamic_cast<const jinja::integer_literal *>(p)) head += " val=" + std::to_string(x->val);
    else if (auto x = dynamic_cast<const jinja::float_literal *>(p)) {
        char b[64]; snprintf(b, sizeof b, "%.17g", x->val); head += std::string(" val=") + b;
    }
    else if (auto x = dynamic_cast<const jinja::binary_expression *>(p)) head += " op=" + q(x->op.value);
    else if (auto x = dynamic_cast<const jinja::unary_expression *>(p)) head += " op=" + q(x->op.value);
    else if (auto x = dynamic_cast<const jinja::test_expression *>(p)) head += std::string(" negate=") + (x->negate ? "1" : "0");
    else if (auto x = dynamic_cast<const jinja::member_expression *>(p)) head += std::string(" computed=") + (x->computed ? "1" : "0");
    o.line(ind, head);

    auto dlist = [&](const jinja::statements & v, int i2) { for (const auto & e : v) dn(e.get(), i2, o); };

    if (auto x = dynamic_cast<const jinja::program *>(p)) { o.fld(ind + 1, "body"); dlist(x->body, ind + 2); }
    else if (auto x = dynamic_cast<const jinja::if_statement *>(p)) {
        o.fld(ind + 1, "test"); dn(x->test.get(), ind + 2, o);
        o.fld(ind + 1, "body"); dlist(x->body, ind + 2);
        o.fld(ind + 1, "alternate"); dlist(x->alternate, ind + 2);
    }
    else if (auto x = dynamic_cast<const jinja::for_statement *>(p)) {
        o.fld(ind + 1, "loopvar"); dn(x->loopvar.get(), ind + 2, o);
        o.fld(ind + 1, "iterable"); dn(x->iterable.get(), ind + 2, o);
        o.fld(ind + 1, "body"); dlist(x->body, ind + 2);
        o.fld(ind + 1, "default_block"); dlist(x->default_block, ind + 2);
    }
    else if (auto x = dynamic_cast<const jinja::set_statement *>(p)) {
        o.fld(ind + 1, "assignee"); dn(x->assignee.get(), ind + 2, o);
        o.fld(ind + 1, "value"); dn(x->val.get(), ind + 2, o);
        o.fld(ind + 1, "body"); dlist(x->body, ind + 2);
    }
    else if (auto x = dynamic_cast<const jinja::macro_statement *>(p)) {
        o.fld(ind + 1, "name"); dn(x->name.get(), ind + 2, o);
        o.fld(ind + 1, "args"); dlist(x->args, ind + 2);
        o.fld(ind + 1, "body"); dlist(x->body, ind + 2);
    }
    else if (auto x = dynamic_cast<const jinja::member_expression *>(p)) {
        o.fld(ind + 1, "object"); dn(x->object.get(), ind + 2, o);
        o.fld(ind + 1, "property"); dn(x->property.get(), ind + 2, o);
    }
    else if (auto x = dynamic_cast<const jinja::call_expression *>(p)) {
        o.fld(ind + 1, "callee"); dn(x->callee.get(), ind + 2, o);
        o.fld(ind + 1, "args"); dlist(x->args, ind + 2);
    }
    else if (auto x = dynamic_cast<const jinja::array_literal *>(p)) { o.fld(ind + 1, "val"); dlist(x->val, ind + 2); }
    else if (auto x = dynamic_cast<const jinja::tuple_literal *>(p)) { o.fld(ind + 1, "val"); dlist(x->val, ind + 2); }
    else if (auto x = dynamic_cast<const jinja::object_literal *>(p)) {
        o.fld(ind + 1, "pairs");
        for (const auto & pr : x->val) {
            o.fld(ind + 2, "key"); dn(pr.first.get(), ind + 3, o);
            o.fld(ind + 2, "value"); dn(pr.second.get(), ind + 3, o);
        }
    }
    else if (auto x = dynamic_cast<const jinja::binary_expression *>(p)) {
        o.fld(ind + 1, "left"); dn(x->left.get(), ind + 2, o);
        o.fld(ind + 1, "right"); dn(x->right.get(), ind + 2, o);
    }
    else if (auto x = dynamic_cast<const jinja::filter_expression *>(p)) {
        o.fld(ind + 1, "operand"); dn(x->operand.get(), ind + 2, o);
        o.fld(ind + 1, "filter"); dn(x->filter.get(), ind + 2, o);
    }
    else if (auto x = dynamic_cast<const jinja::filter_statement *>(p)) {
        o.fld(ind + 1, "filter"); dn(x->filter.get(), ind + 2, o);
        o.fld(ind + 1, "body"); dlist(x->body, ind + 2);
    }
    else if (auto x = dynamic_cast<const jinja::select_expression *>(p)) {
        o.fld(ind + 1, "lhs"); dn(x->lhs.get(), ind + 2, o);
        o.fld(ind + 1, "test"); dn(x->test.get(), ind + 2, o);
    }
    else if (auto x = dynamic_cast<const jinja::test_expression *>(p)) {
        o.fld(ind + 1, "operand"); dn(x->operand.get(), ind + 2, o);
        o.fld(ind + 1, "test"); dn(x->test.get(), ind + 2, o);
    }
    else if (auto x = dynamic_cast<const jinja::unary_expression *>(p)) {
        o.fld(ind + 1, "argument"); dn(x->argument.get(), ind + 2, o);
    }
    else if (auto x = dynamic_cast<const jinja::slice_expression *>(p)) {
        o.fld(ind + 1, "start_expr"); dn(x->start_expr.get(), ind + 2, o);
        o.fld(ind + 1, "stop_expr"); dn(x->stop_expr.get(), ind + 2, o);
        o.fld(ind + 1, "step_expr"); dn(x->step_expr.get(), ind + 2, o);
    }
    else if (auto x = dynamic_cast<const jinja::keyword_argument_expression *>(p)) {
        o.fld(ind + 1, "key"); dn(x->key.get(), ind + 2, o);
        o.fld(ind + 1, "val"); dn(x->val.get(), ind + 2, o);
    }
    else if (auto x = dynamic_cast<const jinja::spread_expression *>(p)) {
        o.fld(ind + 1, "argument"); dn(x->argument.get(), ind + 2, o);
    }
    else if (auto x = dynamic_cast<const jinja::call_statement *>(p)) {
        o.fld(ind + 1, "call"); dn(x->call.get(), ind + 2, o);
        o.fld(ind + 1, "caller_args"); dlist(x->caller_args, ind + 2);
        o.fld(ind + 1, "body"); dlist(x->body, ind + 2);
    }
    else if (auto x = dynamic_cast<const jinja::ternary_expression *>(p)) {
        o.fld(ind + 1, "condition"); dn(x->condition.get(), ind + 2, o);
        o.fld(ind + 1, "true_expr"); dn(x->true_expr.get(), ind + 2, o);
        o.fld(ind + 1, "false_expr"); dn(x->false_expr.get(), ind + 2, o);
    }
}

// ---- flat dump -------------------------------------------------------------
static const char * KIND_NAMES[] = {
    "Program", "If", "For", "Break", "Continue", "Noop", "Set", "Macro", "Comment",
    "BlankExpression", "MemberExpression", "CallExpression", "Identifier",
    "IntegerLiteral", "FloatLiteral", "StringLiteral", "ArrayLiteral", "TupleLiteral",
    "ObjectLiteral", "BinaryExpression", "FilterExpression", "FilterStatement",
    "SelectExpression", "TestExpression", "UnaryExpression", "SliceExpression",
    "KeywordArgumentExpression", "SpreadExpression", "CallStatement", "Ternary",
};

struct Flat {
    std::vector<int64_t> & A;
    std::vector<double> & F;
    std::vector<uint8_t> & B;
    std::vector<int64_t> & st;
    int64_t pk, pp, pi, pso, psl, pc0, pc1, pc2, pl0s, pl0c, pl1s, pl1c, pl0t, pl1t, pool;
    explicit Flat(std::vector<int64_t> & a, std::vector<double> & f, std::vector<uint8_t> & b, std::vector<int64_t> & s)
        : A(a), F(f), B(b), st(s) {
        pk = st[20]; pp = st[21]; pi = st[22]; pso = st[23]; psl = st[24];
        pc0 = st[25]; pc1 = st[26]; pc2 = st[27];
        pl0s = st[28]; pl0c = st[29]; pl1s = st[30]; pl1c = st[31]; pl0t = st[32]; pl1t = st[33]; pool = st[34];
    }
    std::string str(int64_t id) const {
        return std::string((const char *) B.data() + A[pso + id], (size_t) A[psl + id]);
    }
};

static void Flat_node(const Flat & f, int64_t id, int ind, Out & o) {
    if (id < 0) { o.line(ind, "null"); return; }
    const int64_t k = f.A[f.pk + id];
    std::string head = std::string(KIND_NAMES[k]) + " pos=" + std::to_string(f.A[f.pp + id]);
    if (k == 8 || k == 12 || k == 15) head += " val=" + q(f.str(id));
    else if (k == 13) head += " val=" + std::to_string(f.A[f.pi + id]);
    else if (k == 14) { char b[64]; snprintf(b, sizeof b, "%.17g", f.F[id]); head += std::string(" val=") + b; }
    else if (k == 19 || k == 24) head += " op=" + q(f.str(id));
    else if (k == 23) head += std::string(" negate=") + (f.A[f.pi + id] ? "1" : "0");
    else if (k == 10) head += std::string(" computed=") + (f.A[f.pi + id] ? "1" : "0");
    o.line(ind, head);

    auto c0 = [&](int i2) { Flat_node(f, f.A[f.pc0 + id], i2, o); };
    auto c1 = [&](int i2) { Flat_node(f, f.A[f.pc1 + id], i2, o); };
    auto c2 = [&](int i2) { Flat_node(f, f.A[f.pc2 + id], i2, o); };
    auto l0 = [&](int i2) {
        int64_t e = f.A[f.pl0s + id];
        while (e >= 0) { Flat_node(f, f.A[f.pool + 2 * e + 1], i2, o); e = f.A[f.pool + 2 * e]; }
    };
    auto l1 = [&](int i2) {
        int64_t e = f.A[f.pl1s + id];
        while (e >= 0) { Flat_node(f, f.A[f.pool + 2 * e + 1], i2, o); e = f.A[f.pool + 2 * e]; }
    };

    if (k == 0) { o.fld(ind + 1, "body"); l0(ind + 2); }
    else if (k == 1) { o.fld(ind + 1, "test"); c0(ind + 2); o.fld(ind + 1, "body"); l0(ind + 2); o.fld(ind + 1, "alternate"); l1(ind + 2); }
    else if (k == 2) { o.fld(ind + 1, "loopvar"); c0(ind + 2); o.fld(ind + 1, "iterable"); c1(ind + 2); o.fld(ind + 1, "body"); l0(ind + 2); o.fld(ind + 1, "default_block"); l1(ind + 2); }
    else if (k == 6) { o.fld(ind + 1, "assignee"); c0(ind + 2); o.fld(ind + 1, "value"); c1(ind + 2); o.fld(ind + 1, "body"); l0(ind + 2); }
    else if (k == 7) { o.fld(ind + 1, "name"); c0(ind + 2); o.fld(ind + 1, "args"); l0(ind + 2); o.fld(ind + 1, "body"); l1(ind + 2); }
    else if (k == 10) { o.fld(ind + 1, "object"); c0(ind + 2); o.fld(ind + 1, "property"); c1(ind + 2); }
    else if (k == 11) { o.fld(ind + 1, "callee"); c0(ind + 2); o.fld(ind + 1, "args"); l0(ind + 2); }
    else if (k == 16 || k == 17) { o.fld(ind + 1, "val"); l0(ind + 2); }
    else if (k == 18) {
        o.fld(ind + 1, "pairs");
        int64_t ek = f.A[f.pl0s + id], ev = f.A[f.pl1s + id];
        while (ek >= 0 && ev >= 0) {
            o.fld(ind + 2, "key"); Flat_node(f, f.A[f.pool + 2 * ek + 1], ind + 3, o);
            o.fld(ind + 2, "value"); Flat_node(f, f.A[f.pool + 2 * ev + 1], ind + 3, o);
            ek = f.A[f.pool + 2 * ek];
            ev = f.A[f.pool + 2 * ev];
        }
    }
    else if (k == 19) { o.fld(ind + 1, "left"); c0(ind + 2); o.fld(ind + 1, "right"); c1(ind + 2); }
    else if (k == 20) { o.fld(ind + 1, "operand"); c0(ind + 2); o.fld(ind + 1, "filter"); c1(ind + 2); }
    else if (k == 21) { o.fld(ind + 1, "filter"); c0(ind + 2); o.fld(ind + 1, "body"); l0(ind + 2); }
    else if (k == 22) { o.fld(ind + 1, "lhs"); c0(ind + 2); o.fld(ind + 1, "test"); c1(ind + 2); }
    else if (k == 23) { o.fld(ind + 1, "operand"); c0(ind + 2); o.fld(ind + 1, "test"); c1(ind + 2); }
    else if (k == 24) { o.fld(ind + 1, "argument"); c0(ind + 2); }
    else if (k == 25) { o.fld(ind + 1, "start_expr"); c0(ind + 2); o.fld(ind + 1, "stop_expr"); c1(ind + 2); o.fld(ind + 1, "step_expr"); c2(ind + 2); }
    else if (k == 26) { o.fld(ind + 1, "key"); c0(ind + 2); o.fld(ind + 1, "val"); c1(ind + 2); }
    else if (k == 27) { o.fld(ind + 1, "argument"); c0(ind + 2); }
    else if (k == 28) { o.fld(ind + 1, "call"); c0(ind + 2); o.fld(ind + 1, "caller_args"); l0(ind + 2); o.fld(ind + 1, "body"); l1(ind + 2); }
    else if (k == 29) { o.fld(ind + 1, "condition"); c0(ind + 2); o.fld(ind + 1, "true_expr"); c1(ind + 2); o.fld(ind + 1, "false_expr"); c2(ind + 2); }
}


static void check(const std::string & source, const std::string & label) {
    jinja::lexer_result lr;
    try {
        jinja::lexer lx;
        lr = lx.tokenize(source);
    } catch (const std::exception & e) {
        bad("lexer", label + ": " + e.what());
        return;
    }

    const int64_t nt = (int64_t) lr.tokens.size();
    int64_t tlen = 0;
    for (const auto & t : lr.tokens) tlen += (int64_t) t.value.size();

    const int64_t MAXN = 65536;
    std::vector<int64_t> A((size_t) (16 + 4 * nt + 14 * MAXN + 2 * 262144), 0);
    std::vector<double> F((size_t) MAXN, 0.0);
    std::vector<uint8_t> B((size_t) (1 << 24), 0);
    std::vector<uint8_t> E((size_t) (1 << 16), 0);
    std::vector<int64_t> st(64, 0);

    int64_t off = 0;
    for (int64_t i = 0; i < nt; ++i) {
        const auto & t = lr.tokens[(size_t) i];
        A[(size_t) (16 + i)] = (int64_t) t.t;
        A[(size_t) (16 + nt + i)] = off;
        A[(size_t) (16 + 2 * nt + i)] = (int64_t) t.value.size();
        A[(size_t) (16 + 3 * nt + i)] = (int64_t) t.pos;
        memcpy(B.data() + off, t.value.data(), t.value.size());
        off += (int64_t) t.value.size();
    }

    // native
    bool nok = true;
    std::string nerr;
    jinja::program prog;
    try {
        prog = jinja::parse_from_tokens(lr);
    } catch (const std::exception & e) {
        nok = false;
        nerr = e.what();
    }

    ++cases;
    const int64_t root = jinja_parse(nt, tlen, sl(lr.source), sli(A), slf(F), slv(B), slv(E), sli(st));
    const std::string ctx = "template=[" + visible(source) + "] " + label;

    if (!nok) {
        ++values;
        if (root != -1 || st[0] == 0) { bad("expected rejection", ctx); return; }
        ++values;
        const std::string gerr((const char *) E.data(), (size_t) st[1]);
        if (gerr != nerr) {
            bad("error text", ctx + "\n  got =[" + visible(gerr) + "]\n  want=[" + visible(nerr) + "]");
        }
        return;
    }

    ++values;
    if (root < 0 || st[0] != 0) {
        const std::string gerr((const char *) E.data(), (size_t) st[1]);
        bad("unexpected rejection", ctx + ": " + visible(gerr));
        return;
    }

    prog.pos = 0;
    Out no, fo;
    dn(&prog, 0, no);
    Flat fl(A, F, B, st);
    Flat_node(fl, root, 0, fo);
    ++values;
    if (no.s != fo.s) {
        bad("tree", ctx + "\n--- native ---\n" + visible(no.s, 4000) + "\n--- goose ---\n" + visible(fo.s, 4000));
    }
}

int main(void) {
    goose_init(0, 0);

    const std::vector<std::pair<std::string, std::string>> corpus = {
        {"", "empty"},
        {"text", "text only"},
        {"{{ x }}", "identifier"},
        {"{{ 1 }}", "integer"},
        {"{{ 1.5 }}", "float"},
        {"{{ 'a' }}", "string single"},
        {"{{ \"a\" \"b\" }}", "adjacent strings"},
        {"{{ a.b }}", "member"},
        {"{{ a[0] }}", "index"},
        {"{{ a[:] }}", "full slice"},
        {"{{ a[1:] }}", "slice start"},
        {"{{ a[:2] }}", "slice stop"},
        {"{{ a[1:2] }}", "slice both"},
        {"{{ a[1:2:3] }}", "slice step"},
        {"{{ a[] }}", "blank index"},
        {"{{ f() }}", "call no args"},
        {"{{ f(1) }}", "call one arg"},
        {"{{ f(1, 2) }}", "call two args"},
        {"{{ f(x=1) }}", "keyword arg"},
        {"{{ f(*a) }}", "spread"},
        {"{{ f(1, *a, x=2) }}", "mixed args"},
        {"{{ a.b().c[0] }}", "chain"},
        {"{{ a + b }}", "add"},
        {"{{ a - b }}", "sub"},
        {"{{ a * b }}", "mul"},
        {"{{ a / b }}", "div"},
        {"{{ a % b }}", "mod"},
        {"{{ a ~ b }}", "concat op"},
        {"{{ a < b }}", "lt"},
        {"{{ a <= b }}", "le"},
        {"{{ a == b }}", "eq"},
        {"{{ a != b }}", "ne"},
        {"{{ a in b }}", "in"},
        {"{{ a not in b }}", "not in"},
        {"{{ a and b }}", "and"},
        {"{{ a or b }}", "or"},
        {"{{ not a }}", "not"},
        {"{{ -a }}", "unary minus"},
        {"{{ +1 }}", "unary plus"},
        {"{{ -1 }}", "negative literal"},
        {"{{ a | upper }}", "filter"},
        {"{{ a | f(1) }}", "filter call"},
        {"{{ a | f | g }}", "filter chain"},
        {"{{ a is defined }}", "test"},
        {"{{ a is not defined }}", "negated test"},
        {"{{ a is divisibleby 3 }}", "test bare arg"},
        {"{{ a is f(1) }}", "test call"},
        {"{{ a if b else c }}", "ternary"},
        {"{{ a if b }}", "select"},
        {"{{ a if b else c if d else e }}", "chained ternary"},
        {"{{ [1,2,3] }}", "array"},
        {"{{ (1,2) }}", "tuple"},
        {"{{ {'a': 1, 'b': 2} }}", "object"},
        {"{{ {} }}", "empty object"},
        {"{{ [] }}", "empty array"},
        {"{{ (a + b) * c }}", "grouping"},
        {"{{ f(a)[0].b }}", "call then member"},
        {"{% set x = 1 %}tail", "set"},
        {"{% set x %}body{% endset %}", "multiline set"},
        {"{% set a, b = 1, 2 %}", "tuple set"},
        {"{% if x %}a{% endif %}", "if"},
        {"{% if x %}a{% else %}b{% endif %}", "if else"},
        {"{% if x %}a{% elif y %}b{% else %}c{% endif %}", "if elif"},
        {"{% for x in y %}a{% endfor %}", "for"},
        {"{% for x in y %}a{% else %}b{% endfor %}", "for else"},
        {"{% for a, b in y %}{% endfor %}", "for tuple"},
        {"{% break %}", "break"},
        {"{% continue %}", "continue"},
        {"{% generation %}", "generation"},
        {"{% endgeneration %}", "endgeneration"},
        {"{% macro m(a, b=1) %}x{% endmacro %}", "macro"},
        {"{% call m() %}body{% endcall %}", "call statement"},
        {"{% call(u) m(1) %}body{% endcall %}", "call with caller args"},
        {"{% filter upper %}x{% endfilter %}", "filter statement"},
        {"{% filter f(1) %}x{% endfilter %}", "filter statement call"},
        {"{% set x = a if b else c %}", "ternary in set"},
        {"text{# comment #}more", "comment"},
        {"{{ a.b[c].d }}", "nested members"},
        {"a{{ b }}c{% if d %}e{% endif %}f", "mixed"},
        // rejected
        {"{{ }}", "empty expression"},
        {"{{ a", "unterminated expression"},
        {"{{ a }", "single close brace"},
        {"{% if x %}", "missing endif"},
        {"{% if x %}a", "missing endif with body"},
        {"{% unknown %}", "unknown statement"},
        {"{% for x y %}{% endfor %}", "missing in"},
        {"{% for x in %}{% endfor %}", "missing iterable"},
        {"{{ a + }}", "missing right operand"},
        {"{{ f( }}", "missing argument"},
        {"{{ f(1 }}", "missing close paren"},
        {"{{ a[1 }}", "missing close bracket"},
        {"{{ {'a' 1} }}", "missing colon"},
        {"{{ a. }}", "missing member"},
        {"{% set %}", "set without expression"},
        {"{{ (1,2 }}", "missing paren after tuple"},
        {"{% call 1() %}{% endcall %}", "call callee not identifier"},
        {"text {% endif %}", "stray endif"},
        {"{% set x = %}", "set without value"},
        {"{{ a is }}", "test without name"},
        {"{{ (1,) }}", "trailing comma tuple"},
    };
    for (const auto & [src, label] : corpus) check(src, label);

    { // 2000 text/expression pairs
        std::string s;
        for (int i = 0; i < 2000; ++i) s += "a{{ b }}";
        check(s, "2000 pairs");
    }
    { // 100 nested parentheses: the native parser also recurses per level
        std::string s = "{{ ";
        s += std::string(100, '(');
        s += "1";
        s += std::string(100, ')');
        s += " }}";
        check(s, "100 nested parens");
    }
    { // a 500-term additive chain
        std::string s = "{{ a";
        for (int i = 0; i < 500; ++i) s += " + a";
        s += " }}";
        check(s, "500-term chain");
    }
    { // 500 arguments
        std::string s = "{{ f(";
        for (int i = 0; i < 500; ++i) { if (i) s += ","; s += "1"; }
        s += ") }}";
        check(s, "500 args");
    }
    { // 1000 statement lines
        std::string s;
        for (int i = 0; i < 1000; ++i) s += "{% if x %}a{% endif %}";
        check(s, "1000 if blocks");
    }

    printf("FG503_PARSER cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails ? 1 : 0;
}
