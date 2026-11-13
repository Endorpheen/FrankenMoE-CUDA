// Differential fixture for schema introspection (EXP495).
// Oracle: common_chat_schema::value_types / may_be_string / kind_name /
// type_name from common/json-schema.cpp.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <map>

#include "schemaint-gen.h"
#include <vector>
std::string string_join(const std::vector<std::string> &v,const std::string &s){std::string r;for(size_t i=0;i<v.size();++i){if(i)r+=s;r+=v[i];}return r;}
std::vector<std::string> string_split(const std::string &s,const std::string &d){std::vector<std::string>o;size_t p=0;while(true){size_t q=s.find(d,p);if(q==std::string::npos){o.push_back(s.substr(p));break;}o.push_back(s.substr(p,q-p));p=q+d.size();}return o;}
std::string string_repeat(const std::string &s,size_t n){std::string r;while(n--)r+=s;return r;}
#include "json-schema.h"

static int cases = 0, values = 0, fails = 0;
static void bad(const char * what, const std::string & detail) {
    ++fails;
    if (fails <= 12) printf("FAIL %s: %s\n", what, detail.c_str());
}
static sl_u8 slv(std::vector<uint8_t> & v) { return sl_u8{ v.data(), (int64_t) v.size() }; }
static sl_i64 sli(std::vector<int64_t> & v) { return sl_i64{ v.data(), (int64_t) v.size() }; }

struct Enc {
    std::vector<const common_chat_schema *> ptr;
    std::vector<int64_t> kind, cf, cc, child, pf, pc, pname, pnode, add, items, target, ctype,
        ef, ec, evalt;
    std::map<const common_chat_schema *, int> idx;

    int jt(const common_json & v) const {
        if (v.is_null()) return 0;
        if (v.is_boolean()) return 1;
        if (v.is_number_integer()) return 3;
        if (v.is_number()) return 2;
        if (v.is_string()) return 4;
        if (v.is_array()) return 5;
        return 6;
    }

    int enc(const common_chat_schema * n) {
        if (!n) return -1;
        auto it = idx.find(n);
        if (it != idx.end()) return it->second;
        const int me = (int) kind.size();
        idx[n] = me;
        ptr.push_back(n);
        kind.push_back((int) n->kind());
        cf.push_back(0); cc.push_back(0); pf.push_back(0); pc.push_back(0);
        pname.push_back(0); pnode.push_back(0); add.push_back(-1);
        items.push_back(-1); target.push_back(-1); ctype.push_back(0); ef.push_back(0); ec.push_back(0);
        switch (n->kind()) {
            case common_chat_schema::KIND_REF: {
                const auto * r = static_cast<const common_chat_schema_ref *>(n);
                target[me] = enc(r->target);
                break;
            }
            case common_chat_schema::KIND_ANY_OF: {
                const auto * a = static_cast<const common_chat_schema_any_of *>(n);
                std::vector<int> tmp;
                for (const auto & c : a->children) tmp.push_back(enc(c.get()));
                cf[me] = (int64_t) child.size();
                for (int ci : tmp) child.push_back(ci);
                cc[me] = (int64_t) tmp.size();
                break;
            }
            case common_chat_schema::KIND_ALL_OF: {
                const auto * a = static_cast<const common_chat_schema_all_of *>(n);
                std::vector<int> tmp;
                for (const auto & c : a->children) tmp.push_back(enc(c.get()));
                cf[me] = (int64_t) child.size();
                for (int ci : tmp) child.push_back(ci);
                cc[me] = (int64_t) tmp.size();
                break;
            }
            case common_chat_schema::KIND_CONST: {
                ctype[me] = jt(static_cast<const common_chat_schema_const *>(n)->value);
                break;
            }
            case common_chat_schema::KIND_ENUM: {
                const auto * e = static_cast<const common_chat_schema_enum *>(n);
                ef[me] = (int64_t) evalt.size();
                for (const auto & v : e->values) evalt.push_back(jt(v));
                ec[me] = (int64_t) evalt.size() - ef[me];
                break;
            }
            case common_chat_schema::KIND_ARRAY: {
                items[me] = enc(static_cast<const common_chat_schema_array *>(n)->items.get());
                break;
            }
            case common_chat_schema::KIND_TUPLE: {
                const auto * t = static_cast<const common_chat_schema_tuple *>(n);
                std::vector<int> tmp;
                for (const auto & c : t->items) tmp.push_back(enc(c.get()));
                cf[me] = (int64_t) child.size();
                for (int ci : tmp) child.push_back(ci);
                cc[me] = (int64_t) tmp.size();
                break;
            }
            case common_chat_schema::KIND_OBJECT: {
                const auto * o = static_cast<const common_chat_schema_object *>(n);
                std::vector<std::pair<int, std::pair<int, int>>> tmp;
                for (const auto & p : o->properties) {
                    const int ci = enc(p.schema.get());
                    tmp.push_back({0, {ci, p.required ? 1 : 0}});
                }
                pf[me] = (int64_t) pname.size();
                for (auto & e2 : tmp) {
                    pname.push_back(e2.first);
                    pnode.push_back(e2.second.first);
                }
                pc[me] = (int64_t) tmp.size();
                add[me] = enc(o->additional_properties.get());
                break;
            }
            default: break;
        }
        return me;
    }
};

static int64_t mask_of(const common_chat_schema & s) {
    const auto t = s.value_types();
    int64_t m = 0;
    for (int i = 0; i < 7; ++i) {
        if (t.has((common_chat_schema::value_type) i)) m |= (1 << i);
    }
    return m;
}

static void check(const std::string & text, const std::string & label) {
    common_json doc;
    try {
        doc = common_json::parse(text);
    } catch (const std::exception & e) {
        bad("json", label + ": " + e.what());
        return;
    }
    bool nok = true;
    common_chat_schema_document sd;
    try {
        sd = common_chat_schema_from_json(doc);
    } catch (const std::exception &) {
        nok = false;
    }
    if (!nok) return; // rejection is EXP492's gate

    Enc e;
    e.enc(sd.root.get());
    const int64_t n = (int64_t) e.kind.size();

    const int maxn = 4096;
    std::vector<int64_t> vis(maxn, 0), st(8, 0);
    ++cases;
    for (int64_t i = 0; i < n; ++i) {
        const std::string ctx = label + " node " + std::to_string(i) + " kind " +
                                common_chat_schema::kind_name(e.ptr[i]->kind());
        ++values;
        const int64_t want = mask_of(*e.ptr[i]);
        const int64_t got = schema_value_types(i, sli(e.kind), sli(e.cf), sli(e.cc), sli(e.child),
            sli(e.pf), sli(e.pc), sli(e.pnode), sli(e.add), sli(e.items), sli(e.target),
            sli(e.ctype), sli(e.ef), sli(e.ec), sli(e.evalt), sli(vis), sli(st));
        if (got != want) bad("value_types", ctx + " goose=" + std::to_string(got) + " native=" + std::to_string(want));

        ++values;
        const int64_t wstr = e.ptr[i]->may_be_string() ? 1 : 0;
        const int64_t gstr = schema_may_be_string(i, sli(e.kind), sli(e.cf), sli(e.cc), sli(e.child),
            sli(e.target), sli(e.ctype), sli(e.ef), sli(e.ec), sli(e.evalt), sli(vis), sli(st));
        if (gstr != wstr) bad("may_be_string", ctx + " goose=" + std::to_string(gstr) + " native=" + std::to_string(wstr));
    }
}

int main(void) {
    goose_init(0, 0);

    // the name tables
    {
        std::vector<uint8_t> out(64, 0);
        for (int k = 0; k < 14; ++k) {
            const int64_t n = schema_kind_name(k, slv(out));
            const std::string got((const char *) out.data(), (size_t) n);
            const std::string want = common_chat_schema::kind_name((common_chat_schema::node_kind) k);
            ++cases; ++values;
            if (got != want) bad("kind_name", std::to_string(k) + " got=[" + got + "] want=[" + want + "]");
        }
        for (int t = 0; t < 7; ++t) {
            const int64_t n = schema_type_name(t, slv(out));
            const std::string got((const char *) out.data(), (size_t) n);
            const std::string want = common_chat_schema::type_name((common_chat_schema::value_type) t);
            ++cases; ++values;
            if (got != want) bad("type_name", std::to_string(t) + " got=[" + got + "] want=[" + want + "]");
        }
    }

    const std::vector<std::pair<std::string, std::string>> docs = {
        {"{}", "any"},
        {"{\"type\":\"null\"}", "null"},
        {"{\"type\":\"boolean\"}", "boolean"},
        {"{\"type\":\"number\"}", "number"},
        {"{\"type\":\"integer\"}", "integer"},
        {"{\"type\":\"string\"}", "string"},
        {"{\"type\":\"array\"}", "array"},
        {"{\"type\":\"object\"}", "object"},
        {"{\"type\":\"array\",\"items\":[{\"type\":\"string\"}]}", "tuple"},
        {"{\"const\":\"x\"}", "const string"},
        {"{\"const\":3}", "const integer"},
        {"{\"const\":1.5}", "const number"},
        {"{\"const\":true}", "const boolean"},
        {"{\"const\":null}", "const null"},
        {"{\"const\":[]}", "const array"},
        {"{\"const\":{}}", "const object"},
        {"{\"enum\":[\"a\",\"b\"]}", "enum strings"},
        {"{\"enum\":[1,2]}", "enum ints"},
        {"{\"enum\":[\"a\",1,true,null]}", "enum mixed"},
        {"{\"enum\":[[],{}]}", "enum containers"},
        {"{\"enum\":[1,\"a\"]}", "enum string second"},
        {"{\"anyOf\":[{\"$ref\":\"#/definitions/A\"},{\"$ref\":\"#/definitions/A\"}],\"definitions\":{\"A\":{\"const\":\"x\"}}}", "two refs same target"},
        {"{\"allOf\":[{\"$ref\":\"#/definitions/A\"},{\"$ref\":\"#/definitions/A\"}],\"definitions\":{\"A\":{\"type\":\"string\"}}}", "allOf two refs same target"},
        {"{\"anyOf\":[{\"type\":\"string\"},{\"type\":\"integer\"}]}", "anyOf"},
        {"{\"anyOf\":[]}", "rejected: anyOf empty"},
        {"{\"allOf\":[{\"type\":\"string\"},{\"type\":\"string\"}]}", "allOf strings"},
        {"{\"allOf\":[{\"type\":\"string\"},{\"type\":\"integer\"}]}", "allOf conflict"},
        {"{\"allOf\":[{\"type\":\"string\"},{},{\"type\":\"string\"}]}", "allOf with any"},
        {"{\"allOf\":[{},{}]}", "allOf only any"},
        {"{\"allOf\":[{\"type\":\"number\"},{\"type\":\"integer\"}]}", "allOf number/integer"},
        {"{\"$ref\":\"#/definitions/A\",\"definitions\":{\"A\":{\"type\":\"string\"}}}", "ref string"},
        {"{\"$ref\":\"#/definitions/A\",\"definitions\":{\"A\":{\"anyOf\":[{\"type\":\"string\"},{\"type\":\"number\"}]}}}", "ref anyOf"},
        {"{\"$ref\":\"#/definitions/A\",\"definitions\":{\"A\":{\"type\":\"object\",\"properties\":{\"b\":{\"$ref\":\"#/definitions/B\"}}},\"B\":{\"type\":\"object\",\"properties\":{\"a\":{\"$ref\":\"#/definitions/A\"}}}}}", "ref cycle"},
        {"{\"$ref\":\"#/definitions/A\",\"definitions\":{\"A\":{\"$ref\":\"#/definitions/B\"},\"B\":{\"$ref\":\"#/definitions/A\"}}}", "pure ref cycle"},
        {"{\"type\":\"object\",\"properties\":{\"a\":{\"type\":\"string\"},\"b\":{\"const\":2}}}", "object props"},
        {"{\"type\":[\"string\",\"integer\"]}", "type array"},
    };
    for (const auto & [text, label] : docs) check(text, label);

    printf("FG495_SCHEMAINT cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails ? 1 : 0;
}
