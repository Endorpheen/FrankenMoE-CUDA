// Gate for the typed-direction slice (EXP565): content serialization as a
// typed array, as a joined string, and the caps rewrite (only_string /
// only_typed). Every pair compares the port bytes with a real native call:
//   JSON  -> common_chat_msgs_to_json_oaicompat(...).dump()
//   render-> common_chat_templates_apply() with the typed-only template
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <string>
#include <vector>

#include "runtime-gen.h"
#include "jinja/value.h"
#include "jinja/lexer.h"
#include "jinja/parser.h"
#include "jinja/runtime.h"
#include "jinja/string.h"
#include "chat.h"
#include "json.h"

using json = common_json;

extern "C" double gs_json_strtod(sl_u8 s) {
    std::string tmp((const char *) s.data, (size_t) s.len);
    return std::strtod(tmp.c_str(), nullptr);
}
extern "C" int64_t gs_json_fmt_e(sl_u8 out, int64_t prec, double x) {
    return snprintf((char *) out.data, (size_t) out.len, "%.*e", (int) prec, x);
}
extern "C" int64_t gs_json_fmt_g(sl_u8 out, double x) {
    return snprintf((char *) out.data, (size_t) out.len, "%.17g", x);
}
extern "C" double gs_frexp_mant(double x) {
    int e = 0;
    return std::frexp(x, &e);
}
extern "C" int64_t gs_strftime_epoch(sl_u8 out, int64_t epoch, sl_u8 fmt) {
    const std::string f((const char *) fmt.data, (size_t) fmt.len);
    const std::time_t t = epoch < 0 ? std::time(nullptr) : (std::time_t) epoch;
    const std::tm * tmv = std::localtime(&t);
    if (!tmv) return -1;
    const size_t n = std::strftime((char *) out.data, (size_t) out.len, f.c_str(), tmv);
    return (int64_t) n;
}
extern "C" int64_t gs_to_string_f64(sl_u8 out, double x) {
    const std::string s = std::to_string(x);
    memcpy(out.data, s.data(), s.size());
    return (int64_t) s.size();
}

static int pairs = 0, values = 0, fails = 0;
static void bad(const char * what, const std::string & detail) {
    ++fails;
    if (fails <= 100) printf("FAIL %s: %s\n", what, detail.c_str());
}
static sl_u8 sl(const std::string & s) { return sl_u8{ (uint8_t *) s.data(), (int64_t) s.size() }; }
static sl_u8 slv(std::vector<uint8_t> & v) { return sl_u8{ v.data(), (int64_t) v.size() }; }
static sl_i64 sli(std::vector<int64_t> & v) { return sl_i64{ v.data(), (int64_t) v.size() }; }
static sl_f64 slf(std::vector<double> & v) { return sl_f64{ v.data(), (int64_t) v.size() }; }

static std::string visible(const std::string & s, size_t max = 400) {
    std::string r;
    size_t n = 0;
    for (unsigned char c : s) {
        if (n++ > max) { r += "..."; break; }
        if (c == '\n') r += "\\n";
        else if (c == '\r') r += "\\r";
        else if (c == '\t') r += "\\t";
        else if (c == '\\') r += "\\\\";
        else if (c == '"') r += "\\\"";
        else if (c >= 32 && c < 127) r.push_back((char) c);
        else { char b[8]; snprintf(b, sizeof b, "\\x%02X", c); r += b; }
    }
    return r;
}
// ---- flat encoding ---------------------------------------------------------
struct Enc {
    std::vector<int64_t> kind, ival, foff, flen, cs, cc, ks, kc, vs, vc;
    std::vector<double> F;
    std::vector<uint8_t> B;
    std::vector<int64_t> pool;

    int64_t str(const std::string & s) {
        const int64_t off = (int64_t) B.size();
        B.insert(B.end(), s.begin(), s.end());
        return off;
    }
    int enc(const jinja::value & v) {
        const int id = (int) kind.size();
        kind.push_back(-1); ival.push_back(0); foff.push_back(0); flen.push_back(0);
        cs.push_back(0); cc.push_back(0); ks.push_back(0); kc.push_back(0);
        vs.push_back(0); vc.push_back(0);
        F.push_back(0.0);
        if (jinja::is_val<jinja::value_int>(v)) {
            kind[id] = 0; ival[id] = v->as_int();
            double df = (double) ival[id];
            if ((int64_t) df != ival[id]) df = ival[id] < 0 ? -INFINITY : INFINITY;
            F[id] = df;
        } else if (jinja::is_val<jinja::value_float>(v)) {
            kind[id] = 1; ival[id] = v->as_int(); F[id] = v->as_float();
        } else if (jinja::is_val<jinja::value_string>(v)) {
            kind[id] = 2;
            const std::string s = v->as_string().str();
            foff[id] = str(s); flen[id] = (int64_t) s.size();
        } else if (jinja::is_val<jinja::value_bool>(v)) {
            kind[id] = 3; ival[id] = v->as_int(); F[id] = (double) ival[id];
        } else if (jinja::is_val<jinja::value_tuple>(v)) {
            kind[id] = 5;
            std::vector<int> tmp;
            for (const auto & e : v->as_array()) tmp.push_back(enc(e));
            cs[id] = (int64_t) pool.size(); cc[id] = (int64_t) tmp.size();
            for (int c : tmp) pool.push_back(c);
        } else if (jinja::is_val<jinja::value_array>(v)) {
            kind[id] = 4;
            std::vector<int> tmp;
            for (const auto & e : v->as_array()) tmp.push_back(enc(e));
            cs[id] = (int64_t) pool.size(); cc[id] = (int64_t) tmp.size();
            for (int c : tmp) pool.push_back(c);
        } else if (jinja::is_val<jinja::value_object>(v)) {
            kind[id] = 6;
            std::vector<std::pair<int, int>> tmp;
            for (const auto & p : v->as_ordered_object()) tmp.push_back({enc(p.first), enc(p.second)});
            ks[id] = (int64_t) pool.size(); kc[id] = (int64_t) tmp.size();
            for (auto & p : tmp) pool.push_back(p.first);
            vs[id] = (int64_t) pool.size(); vc[id] = (int64_t) tmp.size();
            for (auto & p : tmp) pool.push_back(p.second);
        } else if (jinja::is_val<jinja::value_none>(v)) {
            kind[id] = 7;
        } else {
            kind[id] = 8;
            std::string hint;
            const std::string t = v->type();
            const std::string pre = "Undefined (hint: '";
            if (t.size() > pre.size() && t.compare(0, pre.size(), pre) == 0) {
                hint = t.substr(pre.size(), t.size() - pre.size() - 2);
            }
            foff[id] = str(hint); flen[id] = (int64_t) hint.size();
        }
        return id;
    }
};

struct AstEnc {
    std::vector<int64_t> kind, pos, ival, soff, slen, c0, c1, c2, l0s, l0c, l1s, l1c;
    std::vector<double> fval;
    std::vector<int64_t> pool;
    std::vector<uint8_t> & B;
    explicit AstEnc(std::vector<uint8_t> & b) : B(b) {}
    int64_t put(const std::string & t) {
        const int64_t o = (int64_t) B.size();
        B.insert(B.end(), t.begin(), t.end());
        return o;
    }
    void init(int id, int k, const jinja::statement * s) {
        kind[id] = k; pos[id] = (int64_t) s->pos;
        ival[id] = 0; soff[id] = 0; slen[id] = 0;
        c0[id] = -1; c1[id] = -1; c2[id] = -1;
        l0s[id] = 0; l0c[id] = 0; l1s[id] = 0; l1c[id] = 0;
        fval[id] = 0.0;
    }
    void put_list(std::vector<int64_t> & target, const std::vector<int> & kids, int id, bool second) {
        if (kids.empty()) return;
        const int64_t start = (int64_t) pool.size();
        for (int k : kids) pool.push_back(k);
        if (second) { l1s[id] = start; l1c[id] = (int64_t) kids.size(); }
        else { l0s[id] = start; l0c[id] = (int64_t) kids.size(); }
    }
    int enc(const jinja::statement * s) {
        const int id = (int) kind.size();
        kind.push_back(0); pos.push_back(0); ival.push_back(0); soff.push_back(0); slen.push_back(0);
        c0.push_back(-1); c1.push_back(-1); c2.push_back(-1);
        l0s.push_back(0); l0c.push_back(0); l1s.push_back(0); l1c.push_back(0);
        fval.push_back(0.0);
        if (auto x = dynamic_cast<const jinja::integer_literal *>(s)) {
            init(id, 13, s); ival[id] = x->val;
        } else if (auto x = dynamic_cast<const jinja::float_literal *>(s)) {
            init(id, 14, s); fval[id] = x->val;
        } else if (auto x = dynamic_cast<const jinja::string_literal *>(s)) {
            init(id, 15, s); soff[id] = put(x->val); slen[id] = (int64_t) x->val.size();
        } else if (auto x = dynamic_cast<const jinja::identifier *>(s)) {
            init(id, 12, s); soff[id] = put(x->val); slen[id] = (int64_t) x->val.size();
        } else if (auto x = dynamic_cast<const jinja::binary_expression *>(s)) {
            init(id, 19, s); soff[id] = put(x->op.value); slen[id] = (int64_t) x->op.value.size();
            c0[id] = enc(x->left.get()); c1[id] = enc(x->right.get());
        } else if (auto x = dynamic_cast<const jinja::unary_expression *>(s)) {
            init(id, 24, s); soff[id] = put(x->op.value); slen[id] = (int64_t) x->op.value.size();
            c0[id] = enc(x->argument.get());
        } else if (auto x = dynamic_cast<const jinja::array_literal *>(s)) {
            init(id, 16, s);
            std::vector<int> kids; for (const auto & c : x->val) kids.push_back(enc(c.get()));
            put_list(l0s, kids, id, false);
        } else if (auto x = dynamic_cast<const jinja::tuple_literal *>(s)) {
            init(id, 17, s);
            std::vector<int> kids; for (const auto & c : x->val) kids.push_back(enc(c.get()));
            put_list(l0s, kids, id, false);
        } else if (auto x = dynamic_cast<const jinja::program *>(s)) {
            init(id, 0, s);
            std::vector<int> kids; for (const auto & c : x->body) kids.push_back(enc(c.get()));
            put_list(l0s, kids, id, false);
        } else if (auto x = dynamic_cast<const jinja::if_statement *>(s)) {
            init(id, 1, s);
            c0[id] = enc(x->test.get());
            std::vector<int> b; for (const auto & c : x->body) b.push_back(enc(c.get()));
            std::vector<int> e; for (const auto & c : x->alternate) e.push_back(enc(c.get()));
            put_list(l0s, b, id, false); put_list(l1s, e, id, true);
        } else if (auto x = dynamic_cast<const jinja::set_statement *>(s)) {
            init(id, 6, s);
            c0[id] = enc(x->assignee.get());
            c1[id] = x->val ? enc(x->val.get()) : -1;
            std::vector<int> b; for (const auto & c : x->body) b.push_back(enc(c.get()));
            put_list(l0s, b, id, false);
        } else if (auto x = dynamic_cast<const jinja::member_expression *>(s)) {
            init(id, 20, s); ival[id] = x->computed ? 1 : 0;
            c0[id] = enc(x->object.get()); c1[id] = enc(x->property.get());
        } else if (auto x = dynamic_cast<const jinja::select_expression *>(s)) {
            init(id, 25, s);
            c0[id] = enc(x->lhs.get()); c1[id] = enc(x->test.get());
        } else if (auto x = dynamic_cast<const jinja::for_statement *>(s)) {
            init(id, 2, s);
            c0[id] = enc(x->loopvar.get()); c1[id] = enc(x->iterable.get());
            std::vector<int> body; for (const auto & c : x->body) body.push_back(enc(c.get()));
            std::vector<int> dflt; for (const auto & c : x->default_block) dflt.push_back(enc(c.get()));
            put_list(l0s, body, id, false); put_list(l1s, dflt, id, true);
        } else if (auto x = dynamic_cast<const jinja::macro_statement *>(s)) {
            init(id, 7, s);
            c0[id] = enc(x->name.get());
            std::vector<int> a; for (const auto & c : x->args) a.push_back(enc(c.get()));
            std::vector<int> b; for (const auto & c : x->body) b.push_back(enc(c.get()));
            put_list(l0s, a, id, false); put_list(l1s, b, id, true);
        } else if (auto x = dynamic_cast<const jinja::call_statement *>(s)) {
            init(id, 8, s);
            c0[id] = enc(x->call.get());
            std::vector<int> a; for (const auto & c : x->caller_args) a.push_back(enc(c.get()));
            std::vector<int> b; for (const auto & c : x->body) b.push_back(enc(c.get()));
            put_list(l0s, a, id, false); put_list(l1s, b, id, true);
        } else if (auto x = dynamic_cast<const jinja::keyword_argument_expression *>(s)) {
            init(id, 22, s);
            c0[id] = enc(x->key.get()); c1[id] = enc(x->val.get());
        } else if (auto x = dynamic_cast<const jinja::test_expression *>(s)) {
            init(id, 26, s); ival[id] = x->negate ? 1 : 0;
            c0[id] = enc(x->operand.get()); c1[id] = enc(x->test.get());
        } else if (auto x = dynamic_cast<const jinja::filter_expression *>(s)) {
            init(id, 23, s);
            c0[id] = enc(x->operand.get()); c1[id] = enc(x->filter.get());
        } else if (auto x = dynamic_cast<const jinja::call_expression *>(s)) {
            init(id, 21, s);
            c0[id] = enc(x->callee.get());
            std::vector<int> a; for (const auto & c : x->args) a.push_back(enc(c.get()));
            put_list(l0s, a, id, false);
        } else if (dynamic_cast<const jinja::break_statement *>(s)) {
            init(id, 3, s);
        } else if (dynamic_cast<const jinja::continue_statement *>(s)) {
            init(id, 4, s);
        } else {
            throw std::runtime_error(std::string("unsupported AST node: ") + s->type());
        }
        return id;
    }
};

// nlohmann -> jinja::value, so the port's own serializer output can feed the
// port's own template engine without a second parser.
static jinja::value to_val(const json & j) {
    if (j.is_string()) return jinja::mk_val<jinja::value_string>(j.get<std::string>());
    if (j.is_boolean()) return jinja::mk_val<jinja::value_bool>(j.get<bool>());
    if (j.is_number_integer()) return jinja::mk_val<jinja::value_int>(j.get<int64_t>());
    if (j.is_number_float()) return jinja::mk_val<jinja::value_float>(j.get<double>());
    if (j.is_null()) return jinja::mk_val<jinja::value_none>();
    if (j.is_array()) {
        auto a = jinja::mk_val<jinja::value_array>();
        for (const auto & e : j) a->push_back(to_val(e));
        return a;
    }
    if (j.is_object()) {
        auto o = jinja::mk_val<jinja::value_object>();
        for (auto it = j.begin(); it != j.end(); ++it) o->insert(jinja::mk_val<jinja::value_string>(it.key()), to_val(it.value()));
        return o;
    }
    return jinja::mk_val<jinja::value_none>();
}

// Native render: typed-only template through the public apply().
static std::string nat_render(const std::string & src, const common_chat_msg & msg) {
    auto tmpls = common_chat_templates_init(nullptr, src);
    common_chat_templates_inputs in;
    in.messages.push_back(msg);
    in.add_generation_prompt = false;
    return common_chat_templates_apply(tmpls.get(), in).prompt.str();
}

// Port render: the same template, executed by the port over the flat AST.
static std::string port_render(const std::string & src, const jinja::value & messages, bool * ok) {
    jinja::lexer lx;
    auto lr = lx.tokenize(src);
    auto prog = jinja::parse_from_tokens(lr);

    std::vector<std::pair<std::string, jinja::value>> cvals;
    cvals.push_back({ "messages", messages });

    Enc e;
    std::vector<int64_t> croot;
    for (const auto & kv : cvals) croot.push_back(e.enc(kv.second));
    AstEnc a(e.B);
    const int64_t aroot = a.enc(&prog);

    const int64_t vn = (int64_t) e.kind.size();
    const int64_t valcap = vn + 1024;
    const int64_t poolcap = (int64_t) e.pool.size() + 65536;
    const int64_t an = (int64_t) a.kind.size();
    const int64_t astcap = an + 4;
    const int64_t astpoolcap = (int64_t) a.pool.size() + 64;
    std::vector<int64_t> A((size_t) (32 + 11 * valcap + poolcap + 12 * astcap + astpoolcap), 0);
    A[0] = valcap; A[1] = vn; A[2] = (int64_t) e.pool.size(); A[3] = astcap; A[5] = poolcap; A[6] = astpoolcap;
    const int64_t vb = 32;
    for (int64_t i = 0; i < vn; ++i) {
        A[(size_t) (vb + i)] = e.kind[(size_t) i];
        A[(size_t) (vb + valcap + i)] = e.ival[(size_t) i];
        A[(size_t) (vb + 2 * valcap + i)] = e.foff[(size_t) i];
        A[(size_t) (vb + 3 * valcap + i)] = e.flen[(size_t) i];
        A[(size_t) (vb + 4 * valcap + i)] = e.cs[(size_t) i];
        A[(size_t) (vb + 5 * valcap + i)] = e.cc[(size_t) i];
        A[(size_t) (vb + 6 * valcap + i)] = e.ks[(size_t) i];
        A[(size_t) (vb + 7 * valcap + i)] = e.kc[(size_t) i];
        A[(size_t) (vb + 8 * valcap + i)] = e.vs[(size_t) i];
        A[(size_t) (vb + 9 * valcap + i)] = e.vc[(size_t) i];
    }
    for (size_t i = 0; i < e.pool.size(); ++i) A[(size_t) (vb + 11 * valcap + i)] = e.pool[i];
    const int64_t ab = 32 + 11 * valcap + poolcap;
    for (int64_t i = 0; i < an; ++i) {
        A[(size_t) (ab + i)] = a.kind[(size_t) i];
        A[(size_t) (ab + astcap + i)] = a.pos[(size_t) i];
        A[(size_t) (ab + 2 * astcap + i)] = a.ival[(size_t) i];
        A[(size_t) (ab + 3 * astcap + i)] = a.soff[(size_t) i];
        A[(size_t) (ab + 4 * astcap + i)] = a.slen[(size_t) i];
        A[(size_t) (ab + 5 * astcap + i)] = a.c0[(size_t) i];
        A[(size_t) (ab + 6 * astcap + i)] = a.c1[(size_t) i];
        A[(size_t) (ab + 7 * astcap + i)] = a.c2[(size_t) i];
        A[(size_t) (ab + 8 * astcap + i)] = a.l0s[(size_t) i];
        A[(size_t) (ab + 9 * astcap + i)] = a.l0c[(size_t) i];
        A[(size_t) (ab + 10 * astcap + i)] = a.l1s[(size_t) i];
        A[(size_t) (ab + 11 * astcap + i)] = a.l1c[(size_t) i];
    }
    for (size_t i = 0; i < a.pool.size(); ++i) A[(size_t) (ab + 12 * astcap + i)] = a.pool[i];

    std::vector<int64_t> noff, nlen;
    for (const auto & kv : cvals) {
        noff.push_back((int64_t) e.B.size());
        nlen.push_back((int64_t) kv.first.size());
        e.B.insert(e.B.end(), kv.first.begin(), kv.first.end());
    }
    const int64_t blen = (int64_t) e.B.size();
    std::vector<uint8_t> B(1 << 22, 0);
    memcpy(B.data(), e.B.data(), e.B.size());
    std::vector<int64_t> cnoff((size_t) (cvals.size() + 1024), 0), cnlen((size_t) (cvals.size() + 1024), 0),
        cval((size_t) (cvals.size() + 1024), 0);
    for (size_t i = 0; i < cvals.size(); ++i) { cnoff[i] = noff[i]; cnlen[i] = nlen[i]; cval[i] = croot[i]; }

    std::vector<double> F((size_t) (valcap + astcap), 0.0);
    for (int64_t i = 0; i < vn; ++i) F[(size_t) i] = e.F[(size_t) i];
    for (int64_t i = 0; i < an; ++i) F[(size_t) (valcap + i)] = a.fval[(size_t) i];

    std::vector<uint8_t> out(1 << 22, 0), E(1 << 16, 0);
    std::vector<int64_t> st(64, 0);
    st[11] = (int64_t) cvals.size();
    const int64_t len = exec_program(aroot, blen, sli(cnoff), sli(cnlen), sli(cval),
        sli(A), slf(F), slv(B), slv(E), slv(out), sli(st));
    if (len < 0) {
        *ok = false;
        return std::string((const char *) E.data(), (size_t) st[2]);
    }
    *ok = true;
    return std::string((const char *) out.data(), (size_t) len);
}

static common_chat_msg parts_msg(const std::vector<std::pair<std::string, std::string>> & parts) {
    common_chat_msg m;
    m.role = "user";
    for (const auto & pr : parts) {
        common_chat_msg_content_part p;
        p.type = pr.first;
        p.text = pr.second;
        m.content_parts.push_back(p);
    }
    return m;
}

static std::string port_parts(const std::vector<std::pair<std::string, std::string>> & parts,
                              int64_t supports_string, int64_t supports_typed, bool * ok) {
    std::string blob;
    std::vector<int64_t> toff, tlen, xoff, xlen;
    for (const auto & pr : parts) {
        toff.push_back((int64_t) blob.size()); tlen.push_back((int64_t) pr.first.size());
        blob += pr.first;
        xoff.push_back((int64_t) blob.size()); xlen.push_back((int64_t) pr.second.size());
        blob += pr.second;
    }
    std::vector<uint8_t> out(1 << 16, 0);
    sl_u8 bs = sl(blob);
    sl_i64 to = sli(toff), tl = sli(tlen), xo = sli(xoff), xl = sli(xlen);
    sl_u8 os = slv(out);
    const int64_t n = chat_content_parts_caps_json(bs, to, tl, xo, xl, (int64_t) parts.size(),
                                                   supports_string, supports_typed, os);
    *ok = n >= 0;
    return std::string((const char *) out.data(), (size_t) (n < 0 ? 0 : n));
}

static std::string port_string(const std::string & content, int64_t supports_string,
                               int64_t supports_typed, bool * ok) {
    std::vector<uint8_t> out(1 << 16, 0);
    sl_u8 bs = sl(content);
    sl_u8 os = slv(out);
    const int64_t n = chat_content_string_caps_json(bs, 0, (int64_t) content.size(),
                                                    supports_string, supports_typed, os);
    *ok = n >= 0;
    return std::string((const char *) out.data(), (size_t) (n < 0 ? 0 : n));
}

// JSON pair: the whole message array, byte for byte against the native dump.
static void check_parts_json(const char * label,
                             const std::vector<std::pair<std::string, std::string>> & parts,
                             bool concat) {
    const common_chat_msg msg = parts_msg(parts);
    const std::string want = common_chat_msgs_to_json_oaicompat(std::vector<common_chat_msg>{ msg }, concat).dump();
    bool ok = false;
    const std::string frag = port_parts(parts, 1, concat ? 0 : 1, &ok);
    const std::string got = "[{\"role\":\"user\",\"content\":" + frag + "}]";
    ++pairs;
    ++values;
    if (!ok) {
        bad("json rejected", label);
        return;
    }
    if (got != want) {
        bad("json", std::string(label) + "\n  got =[" + visible(got) + "]\n  want=[" + visible(want) + "]");
    }
}

// Render pair: the port serializes the content, then the port engine runs the
// typed-only template; the result is compared with native apply(). The native
// message shape is chosen explicitly, because it decides which normalize branch
// the oracle takes: a string content goes through only_typed, parts do not.
static void check_render(const char * label, const std::string & frag, bool native_string,
                         const std::string & native_text,
                         const std::vector<std::pair<std::string, std::string>> & native_parts) {
    const std::string src = "{% for m in messages %}{{ m.content[0] | tojson }}{% endfor %}";
    json parsed;
    try {
        parsed = json::parse(frag);
    } catch (const std::exception & e) {
        bad("render parse", std::string(label) + " " + e.what() + " frag=[" + visible(frag) + "]");
        return;
    }
    auto msgobj = jinja::mk_val<jinja::value_object>();
    msgobj->insert(jinja::mk_val<jinja::value_string>("role"), jinja::mk_val<jinja::value_string>("user"));
    msgobj->insert(jinja::mk_val<jinja::value_string>("content"), to_val(parsed));
    auto msgs = jinja::mk_val<jinja::value_array>();
    msgs->push_back(msgobj);

    bool ok = false;
    const std::string got = port_render(src, msgs, &ok);
    ++pairs;
    ++values;
    if (!ok) {
        bad("render port error", std::string(label) + ": " + visible(got));
        return;
    }
    common_chat_msg m;
    m.role = "user";
    if (native_string) {
        m.content = native_text;
    } else {
        m.content_parts = parts_msg(native_parts).content_parts;
    }
    const std::string want = nat_render(src, m);
    ++values;
    if (got != want) {
        bad("render", std::string(label) + "\n  got =[" + visible(got) + "]\n  want=[" + visible(want) + "]");
    }
}

// Invalid UTF-8: native dump() throws type_error.316, so the port must reject
// the content instead of emitting a byte sequence JSON cannot carry.
static void check_invalid_utf8(const char * label, const std::string & text) {
    bool native_threw = false;
    try {
        common_chat_msgs_to_json_oaicompat(std::vector<common_chat_msg>{ parts_msg({ { "text", text } }) }, true).dump();
    } catch (const std::exception &) {
        native_threw = true;
    }
    bool ok_parts = false, ok_string = false;
    port_parts({ { "text", text } }, 1, 0, &ok_parts);
    port_string(text, 0, 1, &ok_string);
    ++pairs;
    values += 2;
    if (!native_threw) {
        bad("utf8 oracle", std::string(label) + ": native accepted the bytes");
        return;
    }
    if (ok_parts || ok_string) {
        bad("utf8", std::string(label) + ": port accepted invalid UTF-8 (parts=" +
            (ok_parts ? "yes" : "no") + " string=" + (ok_string ? "yes" : "no") + ")");
    }
}

// Separators: native dump() is compact. The module's own writer is spaced, so
// the two differ in whitespace only; this pair pins which one is the oracle.
static void check_separators() {
    const std::string content = "a\nb";
    common_chat_msg m;
    m.role = "user";
    m.content = content;
    const std::string want = common_chat_msgs_to_json_oaicompat(std::vector<common_chat_msg>{ m }, false).dump();

    const int64_t valcap = 64, poolcap = 256;
    std::vector<int64_t> A((size_t) (32 + 10 * valcap + poolcap), 0);
    A[0] = valcap;
    A[5] = poolcap;
    std::vector<double> F((size_t) valcap, 0.0);
    std::vector<uint8_t> B(1 << 16, 0), E(1 << 12, 0), out(1 << 16, 0);
    std::vector<int64_t> st(64, 0);
    sl_i64 as{A.data(), (int64_t) A.size()};
    sl_f64 fs{F.data(), (int64_t) F.size()};
    sl_u8 bs{B.data(), (int64_t) B.size()};
    sl_u8 es{E.data(), (int64_t) E.size()};
    sl_i64 ss{st.data(), (int64_t) st.size()};
    sl_u8 rs{(uint8_t *) "user", 4};
    sl_u8 cs{(uint8_t *) content.data(), (int64_t) content.size()};
    sl_u8 os{out.data(), (int64_t) out.size()};
    const int64_t n = chat_message_json(as, fs, bs, es, ss, rs, cs, os);
    const std::string spaced((const char *) out.data(), (size_t) (n < 0 ? 0 : n));

    bool ok = false;
    const std::string compact = "[{\"role\":\"user\",\"content\":" + port_string(content, 1, 1, &ok) + "}]";

    ++pairs;
    values += 3;
    if (!ok || compact != want) {
        bad("separators compact", "got=[" + visible(compact) + "] want=[" + visible(want) + "]");
    }
    if (spaced == want) {
        bad("separators spaced", "the module writer unexpectedly matches the dump bytes: [" + visible(spaced) + "]");
    }
    bool same_value = false;
    try {
        same_value = json::parse(spaced).dump() == want;
    } catch (const std::exception &) {
        same_value = false;
    }
    if (!same_value) {
        bad("separators value", "spaced=[" + visible(spaced) + "] want=[" + visible(want) + "]");
    }
}

int main(void) {
    goose_init(0, 0);

    // P1..P10: the native pairs, JSON direction.
    check_parts_json("P1 two texts, string", { { "text", "first" }, { "text", "second" } }, true);
    check_parts_json("P2 two texts, typed", { { "text", "first" }, { "text", "second" } }, false);
    check_parts_json("P3 single text, typed", { { "text", "only" } }, false);
    check_parts_json("P4 text+media, string", { { "text", "look:" }, { "media_marker", "<__media__>" } }, true);
    check_parts_json("P5 media+text, string", { { "media_marker", "<__media__>" }, { "text", "describe" } }, true);
    check_parts_json("P6 media only, string", { { "media_marker", "<__media__>" } }, true);
    check_parts_json("P7 unknown between texts, string",
                     { { "text", "a" }, { "image_url", "u" }, { "text", "b" } }, true);
    check_parts_json("P8 text with own newline, string", { { "text", "a\n" }, { "text", "b" } }, true);
    check_parts_json("P9 text+media, typed caps",
                     { { "text", "look:" }, { "media_marker", "<__media__>" } }, false);
    check_parts_json("P10 unknown between texts, typed",
                     { { "text", "a" }, { "image_url", "u" }, { "text", "b" } }, false);

    // P11..P16: renders through the typed-only template.
    {
        const std::vector<std::pair<std::string, std::string>> hw = {
            { "text", "hello" }, { "text", "world" } };
        bool ok = false;
        const std::string frag = port_parts(hw, 0, 1, &ok);
        if (!ok) bad("P11 port rejected", "typed parts");
        check_render("P11 typed parts, typed-only template", frag, false, "", hw);
    }
    {
        bool ok = false;
        const std::string frag = port_string("hello", 0, 1, &ok);
        if (!ok) bad("P12 port rejected", "hello");
        check_render("P12 flat string, typed-only template", frag, true, "hello", {});
    }
    // P13: a message with neither content nor parts.
    check_parts_json("P13 no parts, both caps", {}, false);
    {
        bool ok = false;
        const std::string frag = port_parts({}, 0, 1, &ok);
        if (!ok) bad("P14 port rejected", "no parts");
        check_render("P14 no parts, typed-only template", frag, true, "", {});
        const std::vector<std::pair<std::string, std::string>> tm = {
            { "text", "look:" }, { "media_marker", "<__media__>" } };
        bool ok2 = false;
        const std::string frag2 = port_parts(tm, 0, 1, &ok2);
        if (!ok2) bad("P15 port rejected", "text+media");
        check_render("P15 text+media, typed-only template", frag2, false, "", tm);
        const std::vector<std::pair<std::string, std::string>> tmt = {
            { "text", "a" }, { "media_marker", "<__media__>" }, { "text", "b" } };
        bool ok3 = false;
        const std::string frag3 = port_parts(tmt, 0, 1, &ok3);
        if (!ok3) bad("P16 port rejected", "text+media+text");
        check_render("P16 text+media+text, typed-only template", frag3, false, "", tmt);
    }

    // P17..P19: escaping of non-ASCII and control bytes.
    check_parts_json("P17 cyrillic, string", { { "text", "\xd0\x9f\xd1\x80\xd0\xb8\xd0\xb2\xd0\xb5\xd1\x82" } }, true);
    check_parts_json("P18 emoji, typed", { { "text", "\xf0\x9f\x99\x82" } }, false);
    check_parts_json("P19 control 0x01, string", { { "text", std::string("a\x01z", 3) } }, true);
    check_parts_json("P19b control 0x0b, string", { { "text", std::string("a\x0bz", 3) } }, true);

    // P20: invalid UTF-8 must be rejected on both sides.
    check_invalid_utf8("P20 overlong c0 80", std::string("a\xc0\x80z", 4));
    check_invalid_utf8("P20b truncated d0", std::string("a\xd0", 2));
    check_invalid_utf8("P20c surrogate ed a0 80", std::string("\xed\xa0\x80", 3));

    // P21: separators, compact versus the module writer.
    check_separators();

    printf("FG565_TYPED pairs=%d values=%d failures=%d\n", pairs, values, fails);
    return fails == 0 ? 0 : 1;
}
