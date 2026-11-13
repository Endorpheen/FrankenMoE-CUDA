// Differential fixture for the JSON-schema document builder (EXP492).
// Oracle: common_chat_schema_from_json from common/json-schema.cpp.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <functional>

#include <vector>

// the two common.cpp helpers json-schema.cpp links to
std::string string_join(const std::vector<std::string> & v, const std::string & sep) {
    std::string r;
    for (size_t i = 0; i < v.size(); ++i) { if (i) r += sep; r += v[i]; }
    return r;
}
std::vector<std::string> string_split(const std::string & s, const std::string & d) {
    std::vector<std::string> o; size_t p = 0;
    while (true) { size_t q = s.find(d, p); if (q == std::string::npos) { o.push_back(s.substr(p)); break; } o.push_back(s.substr(p, q - p)); p = q + d.size(); }
    return o;
}
std::string string_repeat(const std::string & s, size_t n) { std::string r; while (n--) r += s; return r; }

#include "schema-gen.h"
#include "json-schema.h"

static int cases = 0, values = 0, fails = 0;
static void bad(const char * what, const std::string & detail) {
    ++fails;
    if (fails <= 12) printf("FAIL %s: %s\n", what, detail.c_str());
}

static sl_u8 slv(std::vector<uint8_t> & v) { return sl_u8{ v.data(), (int64_t) v.size() }; }
static sl_i64 sli(std::vector<int64_t> & v) { return sl_i64{ v.data(), (int64_t) v.size() }; }
static sl_f64 slf(std::vector<double> & v) { return sl_f64{ v.data(), (int64_t) v.size() }; }

struct Interner {
    std::vector<std::string> strs;
    std::map<std::string, int> idx;
    int sid(const std::string & s) {
        auto it = idx.find(s);
        if (it != idx.end()) return it->second;
        int i = (int) strs.size();
        strs.push_back(s);
        idx[s] = i;
        return i;
    }
};

struct JsonFlat {
    Interner & in;
    std::vector<int64_t> kind, val, first, cnt, key, child;
    std::vector<double> dbl;
    std::vector<const common_json *> node_ptr;
    explicit JsonFlat(Interner & i) : in(i) {}
    int enc(const common_json & v) {
        const int me = (int) kind.size();
        kind.push_back(0); val.push_back(0); first.push_back(0); cnt.push_back(0);
        node_ptr.push_back(&v);
        dbl.push_back(0.0);
        if (v.is_null()) {
            kind[me] = 0;
        } else if (v.is_boolean()) {
            kind[me] = 1; val[me] = v.get<bool>() ? 1 : 0;
        } else if (v.is_number_integer()) {
            kind[me] = 2; val[me] = v.get<int64_t>();
        } else if (v.is_number()) {
            kind[me] = 3; dbl[me] = v.get<double>();
        } else if (v.is_string()) {
            kind[me] = 4; val[me] = in.sid(v.get<std::string>());
        } else if (v.is_array()) {
            kind[me] = 5;
            std::vector<int> tmp;
            for (const auto & e : v) tmp.push_back(enc(e));
            first[me] = (int64_t) child.size();
            for (int ci : tmp) { key.push_back(-1); child.push_back(ci); }
            cnt[me] = (int64_t) tmp.size();
        } else {
            kind[me] = 6;
            std::vector<std::pair<int, int>> tmp;
            for (const auto & [k, e] : v.items()) {
                const int ci = enc(e);
                tmp.emplace_back(in.sid(k), ci);
            }
            first[me] = (int64_t) child.size();
            for (auto & [ks, ci] : tmp) { key.push_back(ks); child.push_back(ci); }
            cnt[me] = (int64_t) tmp.size();
        }
        return me;
    }
};

struct SchemaFlat {
    Interner & in;
    std::map<const common_chat_schema *, int> idx;
    std::vector<int64_t> kind, cf, cc, child, pf, pc, pname, pnode, preq, add,
        strv, fmt, minl, maxl, mn, mx, items, cst, ef, ec, eval, target;
    explicit SchemaFlat(Interner & i) : in(i) {}
    int enc(const common_chat_schema * n) {
        if (!n) return -1;
        auto it = idx.find(n);
        if (it != idx.end()) return it->second;
        const int me = (int) kind.size();
        idx[n] = me;
        kind.push_back(-1); cf.push_back(0); cc.push_back(0); pf.push_back(0); pc.push_back(0);
        pname.push_back(0); pnode.push_back(0); preq.push_back(0); add.push_back(-1);
        strv.push_back(-1); fmt.push_back(0); minl.push_back(0); maxl.push_back(-1);
        mn.push_back(INT64_MIN); mx.push_back(INT64_MAX); items.push_back(-1); cst.push_back(-1);
        ef.push_back(0); ec.push_back(0); target.push_back(-1);
        kind[me] = (int) n->kind();
        switch (n->kind()) {
            case common_chat_schema::KIND_REF: {
                const auto * r = static_cast<const common_chat_schema_ref *>(n);
                strv[me] = in.sid(r->ref);
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
                const auto * c = static_cast<const common_chat_schema_const *>(n);
                cst[me] = in.sid(c->value.dump());
                break;
            }
            case common_chat_schema::KIND_ENUM: {
                const auto * e = static_cast<const common_chat_schema_enum *>(n);
                ef[me] = (int64_t) eval.size();
                for (const auto & v : e->values) eval.push_back(in.sid(v.dump()));
                ec[me] = (int64_t) eval.size() - ef[me];
                break;
            }
            case common_chat_schema::KIND_INTEGER: {
                const auto * i = static_cast<const common_chat_schema_integer *>(n);
                mn[me] = i->minimum; mx[me] = i->maximum;
                break;
            }
            case common_chat_schema::KIND_STRING: {
                const auto * s = static_cast<const common_chat_schema_string *>(n);
                strv[me] = s->pattern.empty() ? -1 : in.sid(s->pattern);
                fmt[me] = (int64_t) s->format;
                minl[me] = s->min_length; maxl[me] = s->max_length;
                break;
            }
            case common_chat_schema::KIND_ARRAY: {
                const auto * a = static_cast<const common_chat_schema_array *>(n);
                items[me] = enc(a->items.get());
                minl[me] = a->min_items; maxl[me] = a->max_items;
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
                    tmp.push_back({in.sid(p.name), {ci, p.required ? 1 : 0}});
                }
                pf[me] = (int64_t) pname.size();
                for (auto & e2 : tmp) {
                    pname.push_back(e2.first);
                    pnode.push_back(e2.second.first);
                    preq.push_back(e2.second.second);
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

static void check(const std::string & text, const std::string & label) {
    Interner in;
    JsonFlat jf(in);
    common_json root;
    try {
        root = common_json::parse(text);
    } catch (const std::exception & e) {
        bad("json parse", label + ": " + e.what());
        return;
    }
    const int rootj = jf.enc(root);

    bool nok = true;
    std::string nerr;
    common_chat_schema_document doc;
    try {
        doc = common_chat_schema_from_json(root);
    } catch (const std::exception & e) {
        nok = false;
        nerr = e.what();
    }

    SchemaFlat sf(in);
    if (nok) {
        sf.enc(doc.root.get());
        for (const auto & [k, v] : doc.refs) sf.enc(v.get());
    }

    // flat string table over everything interned so far
    std::vector<uint8_t> strs(1 << 16, 0);
    std::vector<int64_t> sdir(2 * in.strs.size(), 0);
    int64_t off = 0;
    for (size_t i = 0; i < in.strs.size(); ++i) {
        sdir[2 * i + 0] = off;
        sdir[2 * i + 1] = (int64_t) in.strs[i].size();
        for (unsigned char c : in.strs[i]) strs[off++] = c;
    }

    const int maxn = 1024, maxs = 8192, maxp = 512;
    std::vector<int64_t> nkind(maxn, -1), ncf(maxn, 0), ncc(maxn, 0), nchild(maxs, 0),
        npf(maxn, 0), npc(maxn, 0), pname(maxs, 0), pnode(maxs, 0), preq(maxs, 0), nadd(maxn, -1),
        nstr(maxn, -1), nfmt(maxn, 0), nminl(maxn, 0), nmaxl(maxn, -1),
        nmin(maxn, INT64_MIN), nmax(maxn, INT64_MAX), nitems(maxn, -1), nconst(maxn, -1),
        nef(maxn, 0), nec(maxn, 0), evalv(maxs, 0), ntarget(maxn, -1),
        rk(maxp, 0), rv(maxp, 0), pending(maxp, 0), st(16, 0);
    std::vector<uint8_t> pb(1 << 20, 0), mb(1 << 16, 0), err(1 << 16, 0);
    const int64_t docsid = in.sid("#");
    (void) docsid;
    ++cases;
    const int64_t gout = schema_build(rootj, docsid, sli(jf.kind), sli(jf.val), slf(jf.dbl),
        sli(jf.first), sli(jf.cnt), sli(jf.key), sli(jf.child), slv(strs), sli(sdir),
        sli(nkind), sli(ncf), sli(ncc), sli(nchild), sli(npf), sli(npc), sli(pname), sli(pnode), sli(preq),
        sli(nadd), sli(nstr), sli(nfmt), sli(nminl), sli(nmaxl), sli(nmin), sli(nmax), sli(nitems),
        sli(nconst), sli(nef), sli(nec), sli(evalv), sli(ntarget), sli(rk), sli(rv), sli(pending),
        slv(pb), slv(mb), slv(err), sli(st));

    if (!nok) {
        ++values;
        if (st[1] == 0) { bad("expected rejection", label + ": goose accepted"); return; }
        const std::string gerr((const char *) err.data(), (size_t) st[5]);
        ++values;
        if (gerr != nerr) bad("error text", label + "\n  got =[" + gerr + "]\n  want=[" + nerr + "]");
        return;
    }
    ++values;
    if (gout < 0) {
        const std::string gerr((const char *) err.data(), (size_t) st[5]);
        bad("unexpected rejection", label + ": " + gerr);
        return;
    }
    // native ids follow a root-first traversal; the port's follow creation order
    {
        std::map<int64_t, int64_t> m;
        std::vector<int64_t> order;
        std::function<void(int64_t)> dfs = [&](int64_t id) {
            if (id < 0 || m.count(id)) return;
            m[id] = (int64_t) order.size();
            order.push_back(id);
            const int64_t k = nkind[id];
            if (k == 2 || k == 3 || k == 12) for (int64_t j = 0; j < ncc[id]; ++j) dfs(nchild[ncf[id] + j]);
            if (k == 13) {
                for (int64_t j = 0; j < npc[id]; ++j) dfs(pnode[npf[id] + j]);
                dfs(nadd[id]);
            }
            if (k == 11) dfs(nitems[id]);
            if (k == 1) dfs(ntarget[id]);
        };
        dfs(gout);
        const size_t K = order.size();
        std::vector<int64_t> ak(K, -1), acf(K, 0), acc(K, 0), achild, apf(K, 0), apc(K, 0),
            apname, apnode, apreq, aadd(K, -1), astr(K, -1), afmt(K, 0), aminl(K, 0), amaxl(K, -1),
            amin(K, INT64_MIN), amax(K, INT64_MAX), aitems(K, -1), acst(K, -1), aef(K, 0), aec(K, 0),
            aeval, atarget(K, -1);
        for (size_t ni = 0; ni < K; ++ni) {
            const int64_t o = order[ni];
            ak[ni] = nkind[o];
            astr[ni] = nstr[o]; afmt[ni] = nfmt[o]; aminl[ni] = nminl[o]; amaxl[ni] = nmaxl[o];
            amin[ni] = nmin[o]; amax[ni] = nmax[o]; acst[ni] = nconst[o];
            aitems[ni] = nitems[o] >= 0 ? m[nitems[o]] : -1;
            atarget[ni] = ntarget[o] >= 0 ? m[ntarget[o]] : -1;
            aadd[ni] = nadd[o] >= 0 ? m[nadd[o]] : -1;
            if (nkind[o] == 2 || nkind[o] == 3 || nkind[o] == 12) {
                acf[ni] = (int64_t) achild.size();
                for (int64_t j = 0; j < ncc[o]; ++j) achild.push_back(m[nchild[ncf[o] + j]]);
                acc[ni] = (int64_t) achild.size() - acf[ni];
            }
            if (nkind[o] == 13) {
                apf[ni] = (int64_t) apname.size();
                for (int64_t j = 0; j < npc[o]; ++j) {
                    apname.push_back(pname[npf[o] + j]);
                    apnode.push_back(m[pnode[npf[o] + j]]);
                    apreq.push_back(preq[npf[o] + j]);
                }
                apc[ni] = (int64_t) apname.size() - apf[ni];
            }
            if (nkind[o] == 5) {
                aef[ni] = (int64_t) aeval.size();
                for (int64_t j = 0; j < nec[o]; ++j) aeval.push_back(evalv[nef[o] + j]);
                aec[ni] = (int64_t) aeval.size() - aef[ni];
            }
        }
        nkind = ak; ncf = acf; ncc = acc; nchild = achild; npf = apf; npc = apc;
        pname = apname; pnode = apnode; preq = apreq; nadd = aadd; nstr = astr; nfmt = afmt;
        nminl = aminl; nmaxl = amaxl; nmin = amin; nmax = amax; nitems = aitems; nconst = acst;
        nef = aef; nec = aec; evalv = aeval; ntarget = atarget;
        for (int64_t i = 0; i < st[2]; ++i) rv[i] = rv[i] >= 0 ? m[rv[i]] : -1;
        st[0] = (int64_t) K;
    }

    ++values;
    if (st[0] != (int64_t) sf.kind.size()) {
        bad("node count", label + " goose=" + std::to_string(st[0]) + " native=" + std::to_string(sf.kind.size()));
        return;
    }
    const size_t nn = sf.kind.size();
    for (size_t i = 0; i < nn; ++i) {
        const std::string ctx = label + " node " + std::to_string(i);
        ++values;
        if (nkind[i] != sf.kind[i]) { bad("kind", ctx + " goose=" + std::to_string(nkind[i]) + " native=" + std::to_string(sf.kind[i])); continue; }
        if (nkind[i] == 1) { // REF
            ++values;
            if (nstr[i] != sf.strv[i]) bad("ref text", ctx);
            ++values;
            if (ntarget[i] != sf.target[i]) bad("ref target", ctx + " goose=" + std::to_string(ntarget[i]) + " native=" + std::to_string(sf.target[i]));
            continue;
        }
        if (nkind[i] == 2 || nkind[i] == 3) { // ANY_OF / ALL_OF
            ++values;
            if (ncc[i] != sf.cc[i]) { bad("child count", ctx); continue; }
            for (int64_t k = 0; k < ncc[i]; ++k) {
                ++values;
                if (nchild[ncf[i] + k] != sf.child[sf.cf[i] + k]) bad("child", ctx + " index " + std::to_string(k));
            }
            continue;
        }
        if (nkind[i] == 12) { // TUPLE
            ++values;
            if (ncc[i] != sf.cc[i]) { bad("tuple count", ctx); continue; }
            for (int64_t k = 0; k < ncc[i]; ++k) {
                ++values;
                if (nchild[ncf[i] + k] != sf.child[sf.cf[i] + k]) bad("tuple item", ctx + " index " + std::to_string(k));
            }
            continue;
        }
        if (nkind[i] == 4) { // CONST
            ++values;
            const std::string want = sf.cst[i] >= 0 ? in.strs[sf.cst[i]] : std::string();
            const std::string got = nconst[i] >= 0 ? jf.node_ptr[nconst[i]]->dump() : std::string();
            if (got != want) bad("const value", ctx + " got=[" + got + "] want=[" + want + "]");
            continue;
        }
        if (nkind[i] == 5) { // ENUM
            ++values;
            if (nec[i] != sf.ec[i]) { bad("enum count", ctx); continue; }
            for (int64_t k = 0; k < nec[i]; ++k) {
                ++values;
                if (sf.ec[i] <= k) continue;
                const int64_t gj = evalv[nef[i] + k];
                const std::string got = gj >= 0 ? jf.node_ptr[gj]->dump() : std::string();
                const std::string want = in.strs[sf.eval[sf.ef[i] + k]];
                if (got != want) bad("enum value", ctx + " index " + std::to_string(k));
            }
            continue;
        }
        if (nkind[i] == 9) { // INTEGER
            ++values;
            if (nmin[i] != sf.mn[i] || nmax[i] != sf.mx[i]) {
                bad("bounds", ctx + " goose=(" + std::to_string(nmin[i]) + "," + std::to_string(nmax[i]) +
                                 ") native=(" + std::to_string(sf.mn[i]) + "," + std::to_string(sf.mx[i]) + ")");
            }
            continue;
        }
        if (nkind[i] == 10) { // STRING
            ++values;
            if (nminl[i] != sf.minl[i] || nmaxl[i] != sf.maxl[i]) bad("lengths", ctx);
            ++values;
            if (nstr[i] != sf.strv[i]) bad("pattern", ctx);
            ++values;
            if (nfmt[i] != sf.fmt[i]) bad("format", ctx);
            continue;
        }
        if (nkind[i] == 11) { // ARRAY
            ++values;
            if (nitems[i] != sf.items[i]) bad("items", ctx);
            ++values;
            if (nminl[i] != sf.minl[i] || nmaxl[i] != sf.maxl[i]) bad("item bounds", ctx);
            continue;
        }
        if (nkind[i] == 13) { // OBJECT
            ++values;
            if (npc[i] != sf.pc[i]) { bad("property count", ctx); continue; }
            for (int64_t k = 0; k < npc[i]; ++k) {
                ++values;
                if (pname[npf[i] + k] != sf.pname[sf.pf[i] + k]) bad("property name", ctx + " index " + std::to_string(k));
                ++values;
                if (pnode[npf[i] + k] != sf.pnode[sf.pf[i] + k]) bad("property node", ctx + " index " + std::to_string(k));
                ++values;
                if (preq[npf[i] + k] != sf.preq[sf.pf[i] + k]) bad("property required", ctx + " index " + std::to_string(k));
            }
            ++values;
            if (nadd[i] != sf.add[i]) bad("additional properties", ctx);
            continue;
        }
    }
    // document ref map
    ++values;
    std::map<std::string, int64_t> gmap;
    for (int64_t i = 0; i < st[2]; ++i) gmap[in.strs[rk[i]]] = rv[i];
    std::map<std::string, int64_t> nmap;
    for (const auto & [k, v] : doc.refs) nmap[k] = sf.enc(v.get());
    if (gmap != nmap) {
        std::string d;
        for (auto & [k, v] : nmap) {
            auto it = gmap.find(k);
            if (it == gmap.end()) d += " missing " + k;
            else if (it->second != v) d += " target " + k;
        }
        for (auto & [k, v] : gmap) if (!nmap.count(k)) d += " extra " + k;
        bad("ref map", label + ":" + d);
    }
}

int main(void) {
    goose_init(0, 0);

    const std::vector<std::pair<std::string, std::string>> cases_ok = {
        {"{\"type\":\"string\"}", "string"},
        {"{\"type\":\"number\"}", "number"},
        {"{\"type\":\"boolean\"}", "boolean"},
        {"{\"type\":\"null\"}", "null"},
        {"{}", "empty -> any"},
        {"{\"type\":\"object\"}", "object alone"},
        {"{\"type\":\"object\",\"properties\":{}}", "object empty props"},
        {"{\"type\":\"object\",\"properties\":{\"a\":{\"type\":\"string\"}}}", "object one prop"},
        {"{\"type\":\"object\",\"properties\":{\"a\":{\"type\":\"string\"},\"b\":{\"type\":\"integer\"}},\"required\":[\"a\"]}", "object required"},
        {"{\"type\":\"object\",\"properties\":{\"a\":{}},\"required\":[\"missing\"]}", "required unknown name"},
        {"{\"type\":\"object\",\"additionalProperties\":true}", "additional true"},
        {"{\"type\":\"object\",\"additionalProperties\":false}", "additional false"},
        {"{\"type\":\"object\",\"additionalProperties\":{\"type\":\"string\"}}", "additional schema"},
        {"{\"properties\":{\"a\":{}}}", "implicit object"},
        {"{\"type\":\"array\",\"items\":{\"type\":\"string\"},\"minItems\":1,\"maxItems\":3}", "array bounds"},
        {"{\"type\":\"array\"}", "array bare"},
        {"{\"type\":\"array\",\"items\":[{\"type\":\"string\"},{\"type\":\"integer\"}]}", "tuple"},
        {"{\"type\":\"array\",\"prefixItems\":[{\"type\":\"string\"}]}", "prefixItems"},
        {"{\"type\":\"array\",\"items\":{},\"prefixItems\":[{\"type\":\"string\"}]}", "items wins"},
        {"{\"type\":\"string\",\"pattern\":\"^a+$\",\"minLength\":1,\"maxLength\":5}", "string pattern"},
        {"{\"type\":\"string\",\"format\":\"date\"}", "format date"},
        {"{\"type\":\"string\",\"format\":\"time\"}", "format time"},
        {"{\"type\":\"string\",\"format\":\"date-time\"}", "format date-time"},
        {"{\"type\":\"string\",\"format\":\"uuid\"}", "format uuid"},
        {"{\"type\":\"string\",\"format\":\"uuid4\"}", "format uuid4"},
        {"{\"type\":\"string\",\"format\":\"uuid5\"}", "format uuid5"},
        {"{\"type\":\"string\",\"format\":\"email\"}", "format unknown"},
        {"{\"format\":\"date\"}", "implicit string by format"},
        {"{\"pattern\":\"x\"}", "implicit string by pattern"},
        {"{\"minLength\":2}", "implicit string by minLength"},
        {"{\"type\":\"integer\"}", "integer bare"},
        {"{\"type\":\"integer\",\"minimum\":3,\"maximum\":9}", "integer bounds"},
        {"{\"type\":\"integer\",\"minimum\":3.2}", "minimum rounds up"},
        {"{\"type\":\"integer\",\"maximum\":3.8}", "maximum rounds down"},
        {"{\"type\":\"integer\",\"exclusiveMinimum\":3}", "exclusive minimum"},
        {"{\"type\":\"integer\",\"exclusiveMaximum\":3}", "exclusive maximum"},
        {"{\"type\":\"integer\",\"exclusiveMinimum\":3.5}", "exclusive minimum fractional"},
        {"{\"type\":[\"string\",\"integer\"]}", "type array"},
        {"{\"type\":[\"string\"]}", "type array one"},
        {"{\"anyOf\":[{\"type\":\"string\"},{\"type\":\"integer\"}]}", "anyOf"},
        {"{\"oneOf\":[{\"type\":\"string\"},{\"type\":\"integer\"}]}", "oneOf"},
        {"{\"allOf\":[{\"type\":\"object\"},{\"type\":\"object\"}]}", "allOf"},
        {"{\"type\":\"object\",\"allOf\":[{\"type\":\"object\",\"properties\":{\"a\":{}}}]}", "object with allOf and props"},
        {"{\"type\":\"string\",\"allOf\":[{\"type\":\"string\"}]}", "string with allOf"},
        {"{\"const\":\"abc\"}", "const string"},
        {"{\"const\":42}", "const integer"},
        {"{\"const\":{\"a\":1}}", "const object"},
        {"{\"enum\":[\"a\",\"b\"]}", "enum"},
        {"{\"enum\":[1,2,3]}", "enum ints"},
        {"{\"$ref\":\"#/definitions/A\",\"definitions\":{\"A\":{\"type\":\"string\"}}}", "ref simple"},
        {"{\"$ref\":\"#/definitions/A\",\"definitions\":{\"A\":{\"type\":\"object\",\"properties\":{\"b\":{\"$ref\":\"#/definitions/B\"}},\"required\":[\"b\"]},\"B\":{\"type\":\"integer\"}}}", "ref chain"},
        {"{\"$ref\":\"#/definitions/A\",\"definitions\":{\"A\":{\"type\":\"object\",\"properties\":{\"b\":{\"$ref\":\"#/definitions/B\"}},\"required\":[\"b\"]},\"B\":{\"type\":\"object\",\"properties\":{\"a\":{\"$ref\":\"#/definitions/A\"}}}}}", "ref cycle"},
        {"{\"type\":\"object\",\"properties\":{\"x\":{\"$ref\":\"#/definitions/A\"},\"y\":{\"$ref\":\"#/definitions/A\"}},\"definitions\":{\"A\":{\"type\":\"string\"}}}", "two refs same target"},
        {"{\"$ref\":\"#/definitions/A\",\"definitions\":{\"A\":{\"type\":\"array\",\"items\":{\"$ref\":\"#/definitions/A\"}}}}", "self ref array"},
        {"{\"$ref\":\"#/items/0\",\"items\":[{\"type\":\"string\"}]}", "ref into array"},
    };
    for (const auto & [text, label] : cases_ok) check(text, label);

    const std::vector<std::pair<std::string, std::string>> cases_bad = {
        {"[]", "schema not object"},
        {"{\"type\":\"bogus\"}", "unrecognized type"},
        {"{\"enum\":[]}", "empty enum"},
        {"{\"enum\":\"x\"}", "enum not array"},
        {"{\"type\":[]}", "empty type array"},
        {"{\"type\":123}", "type not string"},
        {"{\"properties\":[]}", "properties not object"},
        {"{\"additionalProperties\":\"yes\"}", "additional wrong type"},
        {"{\"type\":\"string\",\"pattern\":5}", "pattern not string"},
        {"{\"type\":\"string\",\"format\":5}", "format not string"},
        {"{\"type\":\"array\",\"items\":{},\"minItems\":-1}", "negative minItems"},
        {"{\"type\":\"array\",\"items\":{},\"maxItems\":1.5}", "fractional maxItems"},
        {"{\"type\":\"integer\",\"minimum\":\"x\"}", "minimum not number"},
        {"{\"anyOf\":{}}", "anyOf not array"},
        {"{\"anyOf\":[]}", "anyOf empty"},
        {"{\"allOf\":[]}", "allOf empty"},
        {"{\"$ref\":5}", "ref not string"},
        {"{\"$ref\":\"other.json#/a\"}", "ref unsupported"},
        {"{\"$ref\":\"#/definitions/Missing\"}", "ref missing"},
        {"{\"$ref\":\"#/definitions/A\",\"definitions\":{\"A\":[]}}", "ref target not object"},
        {"{\"$ref\":\"#/xs/0\",\"xs\":[]}", "ref index out of range"},
        {"{\"type\":\"object\",\"properties\":{\"a\":{\"type\":\"bogus\"}}}", "nested error path"},
        {"{\"type\":\"object\",\"properties\":{\"a\":{\"pattern\":5}}}", "nested pattern path"},
        {"{\"anyOf\":[{\"type\":\"bogus\"}]}", "error inside anyOf"},
        {"{\"allOf\":[{\"type\":\"string\"},{\"enum\":[]}]}", "error inside allOf"},
        {"{\"type\":[\"string\",\"bogus\"]}", "error inside type expansion"},
        {"{\"type\":\"array\",\"items\":[{\"type\":\"bogus\"}]}", "error inside tuple"},
        {"{\"type\":\"array\",\"items\":{\"type\":\"bogus\"}}", "error inside items"},
        {"{\"type\":\"object\",\"additionalProperties\":{\"type\":\"bogus\"}}", "error inside additional"},
        {"{\"$ref\":\"#/definitions/A\",\"definitions\":{\"A\":{\"type\":\"bogus\"}}}", "error inside ref"},
    };
    for (const auto & [text, label] : cases_bad) check(text, label);

    printf("FG492_SCHEMA cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails ? 1 : 0;
}
