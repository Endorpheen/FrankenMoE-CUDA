#!/usr/bin/env python3
"""Generate the S5 (model -> saver) Goose block and the fixture contract.

Both artifacts come from one source of truth, the native
src/llama-model-saver.cpp and src/llama-hparams.h:

  source/model-saver.goose   accepted S1-S4 port + generated savm_* block
  tests/slots.h              slot numbers, key table, hparam fillers

The generator only translates the *shape* of the native calls: which key,
which hparam, which overload, which conditional. The semantics - the GGUF
type of each value, the per-layer reduction, the vocabulary attr -> token-type
mapping, the tensor aliasing and dedup rules - live in the Goose port.

Usage: python3 tests/gen.py
"""

import os
import re
import sys

BASE = os.path.dirname(os.path.abspath(__file__))
SUB = os.path.dirname(BASE)
L = os.environ.get("FG_LLAMA", "/home/end0/AI/LlamaServer/llama.cpp-flint-cuda")

SRC_NATIVE = os.path.join(L, "src", "llama-model-saver.cpp")
SRC_HPARAMS = os.path.join(L, "src", "llama-hparams.h")
SRC_CARRY = os.path.join(SUB, "source", "s4-carry.goose")
OUT_GOOSE = os.path.join(SUB, "source", "model-saver.goose")
OUT_SLOTS = os.path.join(BASE, "slots.h")


# ---------------------------------------------------------------- hparams types

def parse_struct(path, name):
    text = open(path, encoding="utf-8").read()
    m = re.search(r"^struct %s \{\n(.*?)^\};" % re.escape(name), text, re.S | re.M)
    if not m:
        raise SystemExit("struct %s not found in %s" % (name, path))
    fields = {}
    for line in m.group(1).splitlines():
        line = line.split("//")[0].strip()
        if not line or line.startswith("#"):
            continue
        mm = re.match(r"(?:struct\s+)?(std::[A-Za-z_]+<[^;]*?>|[A-Za-z_][\w:]*)\s+([A-Za-z_]\w*)\s*(?:=[^;]*)?;$", line)
        if not mm:
            continue
        typ, fname = mm.group(1), mm.group(2)
        if typ.startswith("std::"):
            m2 = re.match(r"std::(\w+)<(.*)>$", typ)
            if not m2:
                fields[fname] = ("opaque", typ)
                continue
            kind, inner = m2.group(1), m2.group(2)
            if kind == "array":
                parts = [p.strip() for p in inner.rsplit(",", 1)]
                try:
                    n = int(parts[1], 0)
                except ValueError:
                    n = parts[1]
                fields[fname] = ("array", (parts[0], n))
            elif kind == "bitset":
                fields[fname] = ("bitset", inner.strip())
            else:
                fields[fname] = ("opaque", typ)
            continue
        fields[fname] = ("scalar", typ)
    return fields


HP = parse_struct(SRC_HPARAMS, "llama_hparams")
POSNET = parse_struct(SRC_HPARAMS, "llama_hparams_posnet")
CONVNEXT = parse_struct(SRC_HPARAMS, "llama_hparams_convnext")

SCALAR_KIND = {
    "bool": "bool",
    "uint32_t": "u32",
    "int32_t": "i32",
    "uint64_t": "u64",
    "float": "f32",
    "llama_token": "i32",
    "llama_swa_type": "u32",
}


def field_kind(field):
    if field not in HP:
        raise SystemExit("unknown hparams field: %s" % field)
    shape, info = HP[field]
    if shape != "scalar":
        raise SystemExit("hparams.%s is %s, not a scalar" % (field, shape))
    if info not in SCALAR_KIND:
        raise SystemExit("unhandled scalar type %s for %s" % (info, field))
    return SCALAR_KIND[info]


# ---------------------------------------------------------------- body parser

def strip_comments(text):
    out = []
    i = 0
    n = len(text)
    while i < n:
        if text[i] == "/" and i + 1 < n and text[i + 1] == "/":
            while i < n and text[i] != "\n":
                i += 1
            continue
        out.append(text[i])
        i += 1
    return "".join(out)


def split_tokens(text):
    toks = []
    cur = []
    depth = 0
    for c in text:
        if c in "([":
            depth += 1
        elif c in ")]":
            depth -= 1
        if depth == 0 and c in ";{}":
            if cur:
                t = "".join(cur).strip()
                if t:
                    toks.append(("text", t))
                cur = []
            if c != ";":
                toks.append(("sym", c))
            continue
        cur.append(c)
    if cur:
        s = "".join(cur).strip()
        if s:
            toks.append(("text", s))
    return toks


def parse_block(toks, i, top=False):
    nodes = []
    while i < len(toks):
        kind, tok = toks[i]
        if kind == "sym" and tok == "}":
            if top:
                raise SystemExit("unbalanced }")
            return nodes, i + 1
        if kind == "sym":
            raise SystemExit("unexpected %r" % tok)
        if tok.startswith("if ") or tok.startswith("if("):
            cond = tok[tok.index("(") + 1:tok.rindex(")")].strip()
            if toks[i + 1][1] != "{":
                raise SystemExit("expected { after if")
            then, i = parse_block(toks, i + 2)
            els = None
            if i < len(toks) and toks[i][1] == "else":
                if toks[i + 1][1] != "{":
                    raise SystemExit("expected { after else")
                els, i = parse_block(toks, i + 2)
            nodes.append(("if", cond, then, els))
            continue
        if tok.startswith("for"):
            if toks[i + 1][1] != "{":
                raise SystemExit("expected { after for")
            body, i = parse_block(toks, i + 2)
            nodes.append(("for", tok, body))
            continue
        if tok.startswith("std::vector") or tok.startswith("values."):
            nodes.append(("stmt", tok))
            i += 1
            continue
        nodes.append(("stmt", tok))
        i += 1
    return nodes, i


def extract_body():
    text = open(SRC_NATIVE, encoding="utf-8").read()
    m = re.search(r"void llama_model_saver::add_kv_from_model\(\) \{\n(.*?)\n\}\n\n"
                  r"void llama_model_saver::add_tensors_from_model", text, re.S)
    if not m:
        raise SystemExit("add_kv_from_model not found")
    body = m.group(1)
    body = body[body.index("add_kv(LLM_KV_GENERAL_ARCHITECTURE"):]
    nodes, _ = parse_block(split_tokens(strip_comments(body)), 0, top=True)
    return nodes


# ---------------------------------------------------------------- slot tables

KEY_SLOTS = []
KEY_INDEX = {}
H_SLOTS = []
H_INDEX = {}
HA_SLOTS = []
HA_INDEX = {}


def key_slot(name):
    if name not in KEY_INDEX:
        KEY_INDEX[name] = len(KEY_SLOTS)
        KEY_SLOTS.append(name)
    return KEY_INDEX[name]


def h_slot(kind, expr):
    ident = (kind, expr)
    if ident not in H_INDEX:
        H_INDEX[ident] = len(H_SLOTS)
        H_SLOTS.append(ident)
    return H_INDEX[ident]


def ha_slot(elem, field, count, per_layer=False):
    ident = (elem, field, count, per_layer)
    if ident not in HA_INDEX:
        HA_INDEX[ident] = len(HA_SLOTS)
        HA_SLOTS.append(ident)
    return HA_INDEX[ident]


def up(name):
    return re.sub(r"[^A-Za-z0-9]+", "_", name).upper()


# ---------------------------------------------------------------- render

STATE = "st, pool, key_off, key_len, types, arr, v0, v1, v2"
KSTATE = "k_off, k_len, k_pool"


def field_access(expr):
    m = re.fullmatch(r"hparams\.(\w+)", expr)
    if m:
        f = m.group(1)
        return field_kind(f), h_slot(field_kind(f), "hparams." + f)
    m = re.fullmatch(r"hparams\.posnet\.(\w+)", expr)
    if m:
        f = m.group(1)
        return "u32", h_slot("u32", "posnet." + f)
    m = re.fullmatch(r"hparams\.convnext\.(\w+)", expr)
    if m:
        f = m.group(1)
        return "u32", h_slot("u32", "convnext." + f)
    if expr == "hparams.n_expert_used()":
        return "u32", h_slot("u32", "call:n_expert_used")
    if expr == "uint32_t(hparams.pooling_type)":
        return "u32", h_slot("u32", "cast:pooling_type")
    raise SystemExit("unhandled scalar source: %r" % expr)


def arr_call(key, etype, arrname, sid, n, per):
    kref = "K_" + up(key)
    if etype == "K_F32":
        return ("rc = karrf(%s, tst, %s, %s, scratch, %s[%d * av..(%d + 1) * av], %s, %s as i64, out);"
                % (STATE, KSTATE, kref, arrname, sid, sid, n, per))
    return ("rc = karr(%s, tst, %s, %s, %s, scratch, %s[%d * av..(%d + 1) * av], %s, %s as i64, out);"
            % (STATE, KSTATE, kref, etype, arrname, sid, sid, n, per))


def render_kv(key, arg2, per_layer):
    kref = "K_" + up(key)
    key_slot(key)

    if per_layer:
        m = re.fullmatch(r"hparams\.(\w+)", arg2)
        if not m:
            raise SystemExit("per-layer source %r" % arg2)
        f = m.group(1)
        elem = HP[f][1][0]
        etype = {"uint32_t": "K_U32", "int32_t": "K_I32", "float": "K_F32"}[elem]
        arr = "haf" if elem == "float" else "ha"
        sid = ha_slot(elem, f, "per_layer", True)
        return arr_call(key, etype, arr, sid, "tst[TS_NLAYER]", "1")

    if arg2 == "tokens":
        return ("rc = kstrs(%s, %s, %s, s_off, s_len, s_pool, ix[I_TOK_FIRST],"
                " ix[I_NTOKENS], scratch, out);" % (STATE, KSTATE, kref))
    if arg2 == "token_types":
        return ("rc = ktoktypes(%s, tst, %s, %s, attrs, ix[I_NTOKENS], scratch, out);"
                % (STATE, KSTATE, kref))
    if arg2 == "scores":
        return ("rc = kscores(%s, tst, %s, %s, scores, ix[I_NTOKENS], scratch, out);"
                % (STATE, KSTATE, kref))
    if arg2 == "tensor_names":
        return ("rc = kstrs(%s, %s, %s, s_off, s_len, s_pool, ix[I_PREC_FIRST],"
                " ix[I_NPREC], scratch, out);" % (STATE, KSTATE, kref))
    if arg2 == "ple_layers":
        sid = ha_slot("uint32_t", "is_ple_impl", "whole")
        return ("rc = kple(%s, tst, %s, %s, ha[%d * av..(%d + 1) * av],"
                " ix[I_NLAYER_ALL], scratch, out);" % (STATE, KSTATE, kref, sid, sid))

    if arg2 == "model->classifier_labels":
        return ("rc = kstrs(%s, %s, %s, s_off, s_len, s_pool, ix[I_LABEL_FIRST],"
                " ix[I_NLABELS], scratch, out);" % (STATE, KSTATE, kref))
    if arg2 == "model->arch_name()":
        return ("rc = kstr(%s, %s, %s, ss(s_off, s_len, s_pool, S_ARCH), out);"
                % (STATE, KSTATE, kref))
    if arg2 == "model->name":
        return ("rc = kstr(%s, %s, %s, ss(s_off, s_len, s_pool, S_NAME), out);"
                % (STATE, KSTATE, kref))
    if arg2 == "llama_rope_section_order_name(hparams.rope_section_order)":
        return ("rc = kstr(%s, %s, %s, ss(s_off, s_len, s_pool, S_ROPE_ORDER), out);"
                % (STATE, KSTATE, kref))
    if arg2 == "llama_rope_scaling_type_name(hparams.rope_scaling_type_train)":
        return ("rc = kstr(%s, %s, %s, ss(s_off, s_len, s_pool, S_ROPE_SCALE), out);"
                % (STATE, KSTATE, kref))
    if arg2 == "vocab.get_tokenizer_model()":
        return ("rc = kstr(%s, %s, %s, ss(s_off, s_len, s_pool, S_TOK_MODEL), out);"
                % (STATE, KSTATE, kref))
    if arg2 == "vocab.get_tokenizer_pre()":
        return ("rc = kstr(%s, %s, %s, ss(s_off, s_len, s_pool, S_TOK_PRE), out);"
                % (STATE, KSTATE, kref))
    if arg2 == "vocab.get_bpe_merges()":
        return ("rc = kstrs(%s, %s, %s, s_off, s_len, s_pool, ix[I_MERGE_FIRST],"
                " ix[I_NMERGES], scratch, out);" % (STATE, KSTATE, kref))
    if arg2 == "vocab.get_precompiled_charsmap()":
        return ("if charmap_len > 0 as i64 {\n"
                "        // native reaches the char overload and GGML_ABORTs; the port refuses\n"
                "        out[0] = -(1 as i64);\n"
                "        return -(1 as i64);\n"
                "    }")
    if arg2 == "rope_scaling_factor":
        sid = h_slot("f32", "hparams.rope_freq_scale_train")
        return ("let _rsf: f32 = rope_scale(hf[%d]);\n"
                "    rc = kf32(%s, %s, %s, f32_bits(_rsf), out);"
                % (sid, STATE, KSTATE, kref))
    if arg2 == "vocab.n_tokens()":
        sid = h_slot("u32", "call:n_tokens")
        return ("rc = kv_u32(%s, %s, %s, hv[%d], out);" % (STATE, KSTATE, kref, sid))
    if arg2 == "vocab.n_token_types()":
        sid = h_slot("u32", "call:n_token_types")
        return ("rc = kv_u32(%s, %s, %s, hv[%d], out);" % (STATE, KSTATE, kref, sid))

    m = re.fullmatch(r"uint32_t\(vocab\.(token_\w+)\(\)\)", arg2)
    if m:
        sid = h_slot("u32", "call:" + m.group(1))
        return ("if hv[%d] != ix[I_TOKEN_NULL] {\n        rc = kv_u32(%s, %s, %s, hv[%d], out);\n    }"
                % (sid, STATE, KSTATE, kref, sid))

    m = re.fullmatch(r"vocab\.(get_\w+)\(\)", arg2)
    if m:
        sid = h_slot("bool", "call:" + m.group(1))
        return ("rc = kbool(%s, %s, %s, hv[%d], out);"
                % (STATE, KSTATE, kref, sid))

    m = re.fullmatch(r"std::vector<(\w+)>\(\s*hparams\.(\w+)\.begin\(\),\s*hparams\.\2\.begin\(\) \+ ([^)]+)\)", arg2)
    if m:
        elem, f, count = m.group(1), m.group(2), m.group(3).strip()
        cmap = {"hparams.n_layer_all": "ix[I_NLAYER_ALL]",
                "hparams.ple_ngram_size": "ix[I_PLE_NGRAM]",
                "hparams.ple_n_heads": "ix[I_PLE_HEADS]"}
        if count not in cmap:
            raise SystemExit("unhandled vector count %r" % count)
        etype = {"uint32_t": "K_U32", "float": "K_F32", "uint64_t": "K_U64"}[elem]
        arr = "haf" if elem == "float" else "ha"
        sid = ha_slot(elem, f, count)
        return arr_call(key, etype, arr, sid, cmap[count], "0")

    m = re.fullmatch(r"hparams\.(\w+)", arg2)
    if m and HP.get(m.group(1), ("",))[0] == "array":
        f = m.group(1)
        elem = HP[f][1][0]
        etype = {"uint32_t": "K_U32", "int": "K_I32", "int32_t": "K_I32",
                 "float": "K_F32"}[elem]
        arr = "haf" if elem == "float" else "ha"
        sid = ha_slot(elem, f, "whole")
        n = "ix[I_ROPE_SECTIONS_N]" if f == "rope_sections" else "ix[I_AV]"
        return arr_call(key, etype, arr, sid, n, "0")

    kind, sid = field_access(arg2)
    fn = {"u32": "kv_u32", "i32": "kv_i32", "u64": "kv_u64",
          "f32": "kf32", "bool": "kbool"}[kind]
    val = "f32_bits(hf[%d])" % sid if kind == "f32" else "hv[%d]" % sid
    return "rc = %s(%s, %s, %s, %s, out);" % (fn, STATE, KSTATE, kref, val)


def indent(text, n):
    pad = " " * n
    return text.replace("\n", "\n" + pad)


def render_simple_if(cond_expr, inner):
    m = re.match(r"add_kv\((LLM_KV_\w+),\s*(.*)\)$", inner[0][1], re.S)
    key, arg2 = m.group(1)[len("LLM_KV_"):], m.group(2).strip()
    return "if %s {\n    %s\n}" % (cond_expr, indent(render_kv(key, arg2, False), 4))


def render_node(node):
    if node[0] == "stmt":
        s = node[1]
        if s.startswith("const "):
            return None
        m = re.match(r"add_kv\((LLM_KV_\w+),\s*(.*?)(?:,\s*(true|false))?\)$", s, re.S)
        if m:
            return render_kv(m.group(1)[len("LLM_KV_"):], m.group(2).strip(),
                             m.group(3) == "true")
        if s.startswith("gguf_set_arr_data") or s.startswith("std::vector") or s.startswith("for"):
            raise SystemExit("unhandled statement: %r" % s)
        raise SystemExit("unhandled statement: %r" % s)

    cond, inner, els = node[1], node[2], node[3]
    if cond == "!model->prec_policy.prec_src1.empty()":
        key_slot("GENERAL_TENSOR_EXTRA_PREC_A4")
        return ("if ix[I_NPREC] > 0 as i64 {\n"
                "    " + indent(render_kv("GENERAL_TENSOR_EXTRA_NAME", "tensor_names", False), 4) + "\n"
                "    rc = kprec(%s, tst, %s, %s, s_off, s_len, s_pool, ix[I_PREC_FIRST],"
                " ix[I_NPREC], prec, scratch, out);\n}" % (STATE, KSTATE,
                                                          "K_GENERAL_TENSOR_EXTRA_PREC_A4"))
    if cond == "hparams.ple_n_heads > 0":
        out = ["if hv[%d] > 0 as i64 {" % h_slot("u32", "hparams.ple_n_heads")]
        for n in inner:
            if n[0] == "for":
                continue
            s = n[1]
            if s.startswith("std::vector"):
                continue
            m = re.match(r"add_kv\((LLM_KV_\w+),\s*(.*)\)$", s, re.S)
            out.append("    " + indent(render_kv(m.group(1)[len("LLM_KV_"):],
                                                 m.group(2).strip(), False), 4))
        out.append("}")
        return "\n".join(out)
    if cond == "model->arch == LLM_ARCH_DEEPSEEK4 || hparams.dsv4_hc_mult > 0":
        sid = ha_slot("uint32_t", "dsv4_compress_ratios", "whole")
        return ("if ix[I_ARCH] == ix[I_ARCH_DSV4] || hv[%d] > 0 as i64 {\n"
                "    %s\n} else {\n    %s\n}"
                % (h_slot("u32", "hparams.dsv4_hc_mult"),
                   indent(arr_call("ATTENTION_COMPRESS_RATIOS", "K_U32", "ha", sid,
                                   "ix[I_NLAYER_ALL]", "0"), 4),
                   indent(render_kv("ATTENTION_COMPRESS_RATIOS",
                                    "hparams.dsv4_compress_ratios", True), 4)))
    if cond == "hparams.swa_type != LLAMA_SWA_TYPE_NONE":
        out = ["if hv[%d] != ix[I_SWA_NONE] {" % h_slot("u32", "hparams.swa_type")]
        for n in inner:
            m = re.match(r"add_kv\((LLM_KV_\w+),\s*(.*)\)$", n[1], re.S)
            out.append("    " + indent(render_kv(m.group(1)[len("LLM_KV_"):],
                                                 m.group(2).strip(), False), 4))
        out.append("}")
        return "\n".join(out)
    if cond == "hparams.n_embd_out_impl > 0":
        return render_simple_if("hv[%d] > 0 as i64" % h_slot("u32", "hparams.n_embd_out_impl"),
                                inner)
    if re.fullmatch(r"vocab\.token_\w+\(\)\s*!=\s*LLAMA_TOKEN_NULL", cond):
        # the guarded add_kv renders its own sentinel check
        return render_node(inner[0])
    raise SystemExit("unhandled condition: %r" % cond)


# ---------------------------------------------------------------- helpers

HELPERS = r'''
// The raw IEEE-754 image of an f32, exactly what gguf_set_val_f32 copies.
extern "ms_bits" fn f32_bits(v: f32) -> i64;

fn ks(k_off: const i64[:], k_len: const i64[:], k_pool: const u8[:], i: i64) -> const u8[:] {
    return k_pool[k_off[i]..k_off[i] + k_len[i]];
}

fn ss(s_off: const i64[:], s_len: const i64[:], s_pool: const u8[:], i: i64) -> const u8[:] {
    return s_pool[s_off[i]..s_off[i] + s_len[i]];
}

// Native add_kv scalar overloads, addressed by slot instead of a key string.
fn kv_u32(st: i64[:], pool: u8[:], key_off: i64[:], key_len: i64[:],
          types: i64[:], arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
          k_off: const i64[:], k_len: const i64[:], k_pool: const u8[:],
          slot: i64, value: i64, out: i64[:]) -> i64 {
    return sav_add_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                       ks(k_off, k_len, k_pool, slot), value, out);
}

fn kv_i32(st: i64[:], pool: u8[:], key_off: i64[:], key_len: i64[:],
          types: i64[:], arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
          k_off: const i64[:], k_len: const i64[:], k_pool: const u8[:],
          slot: i64, value: i64, out: i64[:]) -> i64 {
    return sav_add_i32(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                       ks(k_off, k_len, k_pool, slot), value, out);
}

fn kv_u64(st: i64[:], pool: u8[:], key_off: i64[:], key_len: i64[:],
          types: i64[:], arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
          k_off: const i64[:], k_len: const i64[:], k_pool: const u8[:],
          slot: i64, value: i64, out: i64[:]) -> i64 {
    return sav_add_u64(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                       ks(k_off, k_len, k_pool, slot), value, out);
}

// `value` is the raw f32 bit image, as the native overload stores it.
fn kf32(st: i64[:], pool: u8[:], key_off: i64[:], key_len: i64[:],
        types: i64[:], arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
        k_off: const i64[:], k_len: const i64[:], k_pool: const u8[:],
        slot: i64, value: i64, out: i64[:]) -> i64 {
    return sav_add_f32(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                       ks(k_off, k_len, k_pool, slot), value, out);
}

fn kbool(st: i64[:], pool: u8[:], key_off: i64[:], key_len: i64[:],
         types: i64[:], arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
         k_off: const i64[:], k_len: const i64[:], k_pool: const u8[:],
         slot: i64, value: i64, out: i64[:]) -> i64 {
    return sav_add_bool(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                        ks(k_off, k_len, k_pool, slot), value, out);
}

// std::string goes through the native template, which returns before writing
// when the container is empty. vector<string> (kstrs) does not have that early
// return and still writes an empty array.
fn kstr(st: i64[:], pool: u8[:], key_off: i64[:], key_len: i64[:],
        types: i64[:], arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
        k_off: const i64[:], k_len: const i64[:], k_pool: const u8[:],
        slot: i64, val: const u8[:], out: i64[:]) -> i64 {
    if val.len == 0 as i64 {
        out[0] = 0 as i64;
        return 0 as i64;
    }
    return sav_add_str(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                       ks(k_off, k_len, k_pool, slot), val, out);
}

// ---- value encoding -------------------------------------------------------
//
// Every numeric array is passed as i64 values with a GGUF element type; f32
// arrays are passed as f32 and encoded through f32_bits. Widths are the same
// table the writer uses for ggml_nbytes.

fn enc_i64(scratch: u8[:], data: const i64[:], etype: i64, n: i64) -> i64 {
    let esize: i64 = k_elem_size(etype);
    if esize < 0 as i64 || n < 0 as i64 || n > data.len {
        return -(1 as i64);
    }
    if n * esize > scratch.len {
        return -(1 as i64);
    }
    var i: i64 = 0;
    while i < n {
        let v: i64 = data[i];
        var b: i64 = 0;
        while b < esize {
            scratch[i * esize + b] = (v >> (8 as i64 * b)) as! u8;
            b += 1 as i64;
        }
        i += 1 as i64;
    }
    return n * esize;
}

fn enc_f32(scratch: u8[:], data: const f32[:], n: i64) -> i64 {
    if n < 0 as i64 || n > data.len || n * 4 as i64 > scratch.len {
        return -(1 as i64);
    }
    var i: i64 = 0;
    while i < n {
        let v: i64 = f32_bits(data[i]);
        var b: i64 = 0;
        while b < 4 as i64 {
            scratch[i * 4 + b] = (v >> (8 as i64 * b)) as! u8;
            b += 1 as i64;
        }
        i += 1 as i64;
    }
    return n * 4 as i64;
}

// NUL-separated blob for gguf_set_arr_str, in table order.
fn enc_strs(scratch: u8[:], s_off: const i64[:], s_len: const i64[:],
            s_pool: const u8[:], first: i64, n: i64) -> i64 {
    if n < 0 as i64 || first < 0 as i64 || first + n > s_off.len {
        return -(1 as i64);
    }
    var p: i64 = 0;
    var i: i64 = 0;
    while i < n {
        let e: i64 = first + i;
        let o: i64 = s_off[e];
        let l: i64 = s_len[e];
        if o < 0 as i64 || o + l > s_pool.len || p + l + 1 > scratch.len {
            return -(1 as i64);
        }
        var j: i64 = 0;
        while j < l {
            scratch[p + j] = s_pool[o + j];
            j += 1 as i64;
        }
        scratch[p + l] = 0 as u8;
        p = p + l + 1;
        i += 1 as i64;
    }
    return p;
}

// llama_token_attr -> llama_token_type, the native switch plus its default.
// llama_token_attr is a bit flag (1,2,4,8,16,32), not an ordinal; the gate
// asserts every number against llama.h.
fn tok_type(attr: i64) -> i64 {
    if attr == 1 as i64 { return 2 as i64; }   // ATTR_UNKNOWN      -> TYPE_UNKNOWN
    if attr == 2 as i64 { return 5 as i64; }   // ATTR_UNUSED       -> TYPE_UNUSED
    if attr == 4 as i64 { return 1 as i64; }   // ATTR_NORMAL       -> TYPE_NORMAL
    if attr == 8 as i64 { return 3 as i64; }   // ATTR_CONTROL      -> TYPE_CONTROL
    if attr == 16 as i64 { return 4 as i64; }  // ATTR_USER_DEFINED -> TYPE_USER_DEFINED
    if attr == 32 as i64 { return 6 as i64; }  // ATTR_BYTE         -> TYPE_BYTE
    return 0 as i64;                           // UNDEFINED and the native default
}

// Numeric array with a GGUF element type (u32/i32/u64).
fn karr(st: i64[:], pool: u8[:], key_off: i64[:], key_len: i64[:],
        types: i64[:], arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
        tst: i64[:], k_off: const i64[:], k_len: const i64[:], k_pool: const u8[:],
        slot: i64, etype: i64, scratch: u8[:], data: const i64[:], n: i64,
        per: i64, out: i64[:]) -> i64 {
    let nb: i64 = enc_i64(scratch, data, etype, n);
    if nb < 0 as i64 {
        out[0] = -(1 as i64);
        return -(1 as i64);
    }
    return sav_add_arr(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                       tst, ks(k_off, k_len, k_pool, slot), etype,
                       scratch[0..nb], n, per, out);
}

// f32 array.
fn karrf(st: i64[:], pool: u8[:], key_off: i64[:], key_len: i64[:],
         types: i64[:], arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
         tst: i64[:], k_off: const i64[:], k_len: const i64[:], k_pool: const u8[:],
         slot: i64, scratch: u8[:], data: const f32[:], n: i64,
         per: i64, out: i64[:]) -> i64 {
    let nb: i64 = enc_f32(scratch, data, n);
    if nb < 0 as i64 {
        out[0] = -(1 as i64);
        return -(1 as i64);
    }
    return sav_add_arr(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                       tst, ks(k_off, k_len, k_pool, slot), K_F32,
                       scratch[0..nb], n, per, out);
}

// std::vector<std::string> as a NUL-separated GGUF string array.
fn kstrs(st: i64[:], pool: u8[:], key_off: i64[:], key_len: i64[:],
         types: i64[:], arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
         k_off: const i64[:], k_len: const i64[:], k_pool: const u8[:],
         slot: i64, s_off: const i64[:], s_len: const i64[:], s_pool: const u8[:],
         first: i64, n: i64, scratch: u8[:], out: i64[:]) -> i64 {
    let nb: i64 = enc_strs(scratch, s_off, s_len, s_pool, first, n);
    if nb < 0 as i64 {
        out[0] = -(1 as i64);
        return -(1 as i64);
    }
    return sav_add_strs(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                        ks(k_off, k_len, k_pool, slot), scratch[0..nb], n, out);
}

// std::vector<float> token scores.
fn kscores(st: i64[:], pool: u8[:], key_off: i64[:], key_len: i64[:],
           types: i64[:], arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
           tst: i64[:], k_off: const i64[:], k_len: const i64[:], k_pool: const u8[:],
           slot: i64, scores: const f32[:], n: i64, scratch: u8[:], out: i64[:]) -> i64 {
    let nb: i64 = enc_f32(scratch, scores, n);
    if nb < 0 as i64 {
        out[0] = -(1 as i64);
        return -(1 as i64);
    }
    return sav_add_arr(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                       tst, ks(k_off, k_len, k_pool, slot), K_F32,
                       scratch[0..nb], n, 0 as i64, out);
}

// std::vector<int32_t> token types from the raw attrs.
fn ktoktypes(st: i64[:], pool: u8[:], key_off: i64[:], key_len: i64[:],
             types: i64[:], arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
             tst: i64[:], k_off: const i64[:], k_len: const i64[:], k_pool: const u8[:],
             slot: i64, attrs: const i64[:], n: i64, scratch: u8[:], out: i64[:]) -> i64 {
    if n < 0 as i64 || n > attrs.len || n * 4 as i64 > scratch.len {
        out[0] = -(1 as i64);
        return -(1 as i64);
    }
    var i: i64 = 0;
    while i < n {
        let t: i64 = tok_type(attrs[i]);
        var b: i64 = 0;
        while b < 4 as i64 {
            scratch[i * 4 + b] = (t >> (8 as i64 * b)) as! u8;
            b += 1 as i64;
        }
        i += 1 as i64;
    }
    return sav_add_arr(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                       tst, ks(k_off, k_len, k_pool, slot), K_I32,
                       scratch[0..n * 4 as i64], n, 0 as i64, out);
}

// prec_policy: one bool per tensor name, GGML_PREC_Q8 -> 0, otherwise 1.
fn kprec(st: i64[:], pool: u8[:], key_off: i64[:], key_len: i64[:],
         types: i64[:], arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
         tst: i64[:], k_off: const i64[:], k_len: const i64[:], k_pool: const u8[:],
         slot: i64, s_off: const i64[:], s_len: const i64[:], s_pool: const u8[:],
         first: i64, n: i64, prec: const i64[:], scratch: u8[:], out: i64[:]) -> i64 {
    if n < 0 as i64 || n > prec.len || n > scratch.len {
        out[0] = -(1 as i64);
        return -(1 as i64);
    }
    var i: i64 = 0;
    while i < n {
        scratch[i] = 1 as u8;
        if prec[i] == 30 as i64 {  // GGML_PREC_Q8
            scratch[i] = 0 as u8;
        }
        i += 1 as i64;
    }
    return sav_add_arr(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                       tst, ks(k_off, k_len, k_pool, slot), K_BOOL,
                       scratch[0..n], n, 0 as i64, out);
}

// PLE layer list: every layer whose is_ple bit is set.
fn kple(st: i64[:], pool: u8[:], key_off: i64[:], key_len: i64[:],
        types: i64[:], arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],
        tst: i64[:], k_off: const i64[:], k_len: const i64[:], k_pool: const u8[:],
        slot: i64, is_ple: const i64[:], nlayer: i64, scratch: u8[:], out: i64[:]) -> i64 {
    var n: i64 = 0;
    var il: i64 = 0;
    while il < nlayer && il < is_ple.len {
        if is_ple[il] != 0 as i64 {
            if n * 4 + 4 > scratch.len {
                out[0] = -(1 as i64);
                return -(1 as i64);
            }
            var b: i64 = 0;
            while b < 4 as i64 {
                scratch[n * 4 + b] = (il >> (8 as i64 * b)) as! u8;
                b += 1 as i64;
            }
            n += 1 as i64;
        }
        il += 1 as i64;
    }
    return sav_add_arr(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                       tst, ks(k_off, k_len, k_pool, slot), K_U32,
                       scratch[0..n * 4], n, 0 as i64, out);
}

// 0 when the train scale is exactly 1.0f, otherwise 1/scale (native float math).
fn rope_scale(scale: f32) -> f32 {
    var one: f32 = 1.0;
    if scale == one {
        var zero: f32 = 0.0;
        return zero;
    }
    return one / scale;
}
'''

CONSTANTS = r'''
// ---- S5 slot contract -----------------------------------------------------
//
// Key slots: every metadata key the native function writes, in first-use
// order. The host passes the resolved LLM_KV names in this order.
//
// H_*  integer/bool scalar hparams (hv), HF_* float scalars (hf),
// HA_* integer arrays (ha, stride AV), HAF_* f32 arrays (haf).
// I_*  runtime counts and string-table ranges (ix).
// S_*  fixed entries of the resolved string table.

const S_ARCH: i64 = 0;
const S_NAME: i64 = 1;
const S_ROPE_ORDER: i64 = 2;
const S_ROPE_SCALE: i64 = 3;
const S_TOK_MODEL: i64 = 4;
const S_TOK_PRE: i64 = 5;

const I_NTOKENS: i64 = 0;
const I_NTOKEN_TYPES: i64 = 1;
const I_NMERGES: i64 = 2;
const I_NLABELS: i64 = 3;
const I_NPREC: i64 = 4;
const I_TOK_FIRST: i64 = 5;
const I_MERGE_FIRST: i64 = 6;
const I_LABEL_FIRST: i64 = 7;
const I_PREC_FIRST: i64 = 8;
const I_NLAYER_ALL: i64 = 9;
const I_NLAYER_NEXTN: i64 = 10;
const I_ARCH: i64 = 11;
const I_ARCH_DSV4: i64 = 12;
const I_SWA_NONE: i64 = 13;
const I_TOKEN_NULL: i64 = 14;
const I_AV: i64 = 15;
const I_ROPE_SECTIONS_N: i64 = 16;
const I_PLE_NGRAM: i64 = 17;
const I_PLE_HEADS: i64 = 18;
'''

S5_HEADER = '''// ---- S5: saver from a model ----------------------------------------------
//
// Port of llama_model_saver::add_kv_from_model (src/llama-model-saver.cpp
// 147-449) and add_tensors_from_model (451-488). The port never dereferences
// a llama_model: the host passes flat value tables and the resolved LLM_KV
// key names, the same way the accepted S4 writer takes a key parameter.
//
// Decisions kept here:
//   - the emission order and every native conditional (n_embd_out > 0,
//     prec_policy, swa_type, the deepseek4 compress-ratio branch, the
//     whole-or-nothing PLE group);
//   - the GGUF type of each value, taken from the native overload that the
//     hparam type selects;
//   - per-layer reduction: n_layer = n_layer_all - n_layer_nextn, the
//     all-equal collapse to one scalar, otherwise an array;
//   - the vocabulary attr -> llama_token_type switch and its UNDEFINED
//     default (the enum numbers are asserted by the gate against llama.h);
//   - a vocab-less model still writes the token-type array: the native vector
//     is zero-filled, which is LLAMA_TOKEN_TYPE_UNDEFINED;
//   - rope_scaling_factor: exactly 0 for a 1.0f train scale, else 1/scale;
//   - prec_policy: names first, then one bool per name (Q8 -> 0, else 1);
//   - the charmap: a non-empty map reaches the char overload, so native
//     aborts; the port refuses with -1 and does not write that key.
//
// The hparams themselves are input values; their meaning (which layer count,
// which epsilon) belongs to the loader and is not re-derived here.
'''

TENSORS = r'''
// ---- S5: add_tensors_from_model (451-488) --------------------------------
//
// The host passes a flat tensor table (name, n_dims, ne[4], type, data,
// pointer identity) plus:
//   gfix   the 28 model-level slots below, a tensor index or -1 for null;
//   lptrs  ln * LN tensor indices in native struct order (LN is the native
//          sizeof(llama_layer)/sizeof(ggml_tensor *), supplied by the gate).
//
// Rules kept from native:
//   - output and tok_embd sharing a name: tok_embd is not written twice, and
//     the alias branch only triggers when output is non-null and the names
//     differ (native dereferences both names at that point);
//   - a null slot is skipped;
//   - the layer scan skips a pointer already added anywhere (looped
//     architectures alias the same tensor across cache slots);
//   - add_tensor itself still skips a duplicate rope tensor and refuses any
//     other duplicate name (native GGML_ABORT).
//
// Returns the number of descriptors added, or -1 on a refusal.

const G_TYPE_EMBD: i64 = 1;
const G_POS_EMBD: i64 = 2;
const G_TOK_NORM: i64 = 3;
const G_TOK_NORM_B: i64 = 4;
const G_OUTPUT_NORM: i64 = 5;
const G_OUTPUT_NORM_B: i64 = 6;
const G_OUTPUT: i64 = 7;
const G_OUTPUT_B: i64 = 8;
const G_OUTPUT_NORM_ENC: i64 = 9;
const G_OUTPUT_S: i64 = 10;
const G_OUTPUT_IN_S: i64 = 11;
const G_OUTPUT_RES_SCORE: i64 = 12;
const G_NEXTN_PROJ_PRE: i64 = 13;
const G_NEXTN_PROJ_POST: i64 = 14;
const G_CLS: i64 = 15;
const G_CLS_B: i64 = 16;
const G_CLS_OUT: i64 = 17;
const G_CLS_OUT_B: i64 = 18;
const G_CLS_NORM: i64 = 19;
const G_HRM_Z_L_INIT: i64 = 20;
const G_HC_HEAD_FN: i64 = 21;
const G_HC_HEAD_BASE: i64 = 22;
const G_HC_HEAD_SCALE: i64 = 23;
const G_PER_LAYER_TOK_EMBD: i64 = 24;
const G_HC_HEAD_NORM: i64 = 25;
const G_HC_HEAD_DOWN: i64 = 26;
const G_HC_HEAD_UP: i64 = 27;
const G_TOK_EMBD: i64 = 28;

fn t_add(tst: i64[:], tpool: u8[:], tne: i64[:], trec: i64[:],
         t: i64, tname_off: const i64[:], tname_len: const i64[:], tname_pool: const u8[:],
         tnd: const i64[:], tne4: const i64[:], ttype: const i64[:],
         tdata_off: const i64[:], tdata_len: const i64[:], tdata_pool: const u8[:]) -> i64 {
    if t < 0 as i64 {
        return 0 as i64;
    }
    let name: const u8[:] = tname_pool[tname_off[t]..tname_off[t] + tname_len[t]];
    let base: i64 = t * 4 as i64;
    let ne: const i64[:] = tne4[base..base + 4 as i64];
    let data: const u8[:] = tdata_pool[tdata_off[t]..tdata_off[t] + tdata_len[t]];
    return sav_add_tensor(tst, tpool, tne, trec, name, tnd[t], ne, ttype[t], data);
}

fn t_same_name(t: i64, u: i64,
               tname_off: const i64[:], tname_len: const i64[:], tname_pool: const u8[:]) -> bool {
    if t < 0 as i64 || u < 0 as i64 {
        return false;
    }
    return tname_pool[tname_off[t]..tname_off[t] + tname_len[t]] ==
           tname_pool[tname_off[u]..tname_off[u] + tname_len[u]];
}

export fn savm_add_tensors_from_model(
        tst: i64[:], tpool: u8[:], tne: i64[:], trec: i64[:],
        gfix: const i64[:],
        lptrs: const i64[:], ln: i64, lnptrs: i64,
        tname_off: const i64[:], tname_len: const i64[:], tname_pool: const u8[:],
        tnd: const i64[:], tne4: const i64[:], ttype: const i64[:],
        tdata_off: const i64[:], tdata_len: const i64[:], tdata_pool: const u8[:],
        out: i64[:]) -> i64 {
    if ln < 0 as i64 || lnptrs < 0 as i64 || ln * lnptrs > lptrs.len {
        out[0] = -(1 as i64);
        return -(1 as i64);
    }
    var added: i64 = 0;

    // some models use the same tensor for tok_embd and output
    let out_t: i64 = gfix[G_OUTPUT];
    let tok_t: i64 = gfix[G_TOK_EMBD];
    if out_t >= 0 as i64 && tok_t >= 0 as i64 &&
            !t_same_name(out_t, tok_t, tname_off, tname_len, tname_pool) {
        let r: i64 = t_add(tst, tpool, tne, trec, tok_t, tname_off, tname_len, tname_pool,
                           tnd, tne4, ttype, tdata_off, tdata_len, tdata_pool);
        if r < 0 as i64 {
            out[0] = -(1 as i64);
            return -(1 as i64);
        }
        if r == 0 as i64 {
            added += 1 as i64;
        }
    }

    var g: i64 = G_TYPE_EMBD;
    while g < G_TOK_EMBD {
        let t: i64 = gfix[g];
        if t >= 0 as i64 {
            let r: i64 = t_add(tst, tpool, tne, trec, t, tname_off, tname_len, tname_pool,
                               tnd, tne4, ttype, tdata_off, tdata_len, tdata_pool);
            if r < 0 as i64 {
                out[0] = -(1 as i64);
                return -(1 as i64);
            }
            if r == 0 as i64 {
                added += 1 as i64;
            }
        }
        g += 1 as i64;
    }

    var il: i64 = 0;
    while il < ln {
        var f: i64 = 0;
        while f < lnptrs {
            let t: i64 = lptrs[il * lnptrs + f];
            if t >= 0 as i64 {
                // pointer seen already? the identity is the table index
                var seen: bool = false;
                var p: i64 = 0;
                while p < il * lnptrs + f {
                    if lptrs[p] == t {
                        seen = true;
                    }
                    p += 1 as i64;
                }
                if !seen {
                    let r: i64 = t_add(tst, tpool, tne, trec, t, tname_off, tname_len,
                                       tname_pool, tnd, tne4, ttype,
                                       tdata_off, tdata_len, tdata_pool);
                    if r < 0 as i64 {
                        out[0] = -(1 as i64);
                        return -(1 as i64);
                    }
                    if r == 0 as i64 {
                        added += 1 as i64;
                    }
                }
            }
            f += 1 as i64;
        }
        il += 1 as i64;
    }
    return added;
}

export fn savm_supports_arch(arch: i64, no_arch0: i64, no_arch1: i64, no_arch2: i64,
                             no_arch3: i64, no_arch4: i64, no_arch5: i64, no_arch6: i64) -> i64 {
    // llama_model_saver_supports_arch (16): the seven arches whose head
    // tensors are not saved. The host passes their ids so the tree's enum
    // values stay outside the port.
    if arch == no_arch0 || arch == no_arch1 || arch == no_arch2 || arch == no_arch3 ||
            arch == no_arch4 || arch == no_arch5 || arch == no_arch6 {
        return 0 as i64;
    }
    return 1 as i64;
}
'''


def build_goose(nodes):
    body = []
    for node in nodes:
        r = render_node(node)
        if r is not None:
            body.append(r)

    parts = []
    parts.append(S5_HEADER)
    parts.append(CONSTANTS)

    key_consts = []
    for name, idx in KEY_INDEX.items():
        key_consts.append("const K_%s: i64 = %d;" % (up(name), idx))
    parts.append("\n// key slots, first-use order\n" + "\n".join(key_consts) + "\n")

    h_consts, hf_consts, ha_consts, haf_consts = [], [], [], []
    for idx, (kind, expr) in enumerate(H_SLOTS):
        prefix = "HF_" if kind == "f32" else "H_"
        const = "const %s%s: i64 = %d;" % (prefix, up(expr), idx)
        (hf_consts if kind == "f32" else h_consts).append(const)
    for idx, (elem, field, count, per) in enumerate(HA_SLOTS):
        prefix = "HAF_" if elem == "float" else "HA_"
        const = "const %s%s: i64 = %d;" % (prefix, up(field) + ("_P" if per else ""), idx)
        (haf_consts if elem == "float" else ha_consts).append(const)
    parts.append("\n// integer/bool scalar hparams\n" + "\n".join(h_consts))
    parts.append("\n// f32 scalar hparams\n" + "\n".join(hf_consts))
    parts.append("\n// integer arrays, stride AV\n" + "\n".join(ha_consts))
    parts.append("\n// f32 arrays, stride AV\n" + "\n".join(haf_consts) + "\n")

    parts.append(HELPERS)

    sig = ("export fn savm_add_kv_from_model(\n"
           "        st: i64[:], pool: u8[:], key_off: i64[:], key_len: i64[:],\n"
           "        types: i64[:], arr: i64[:], v0: i64[:], v1: i64[:], v2: i64[:],\n"
           "        tst: i64[:],\n"
           "        k_off: const i64[:], k_len: const i64[:], k_pool: const u8[:],\n"
           "        s_off: const i64[:], s_len: const i64[:], s_pool: const u8[:],\n"
           "        ix: const i64[:], hv: const i64[:], hf: const f32[:],\n"
           "        ha: const i64[:], haf: const f32[:],\n"
           "        attrs: const i64[:], scores: const f32[:], prec: const i64[:],\n"
           "        charmap_len: i64, scratch: u8[:], out: i64[:]) -> i64 {\n"
           "    // per-layer reductions use n_layer_all - n_layer_nextn, exactly\n"
           "    // llama_hparams::n_layer(); the writer's TS_NLAYER slot carries it.\n"
           "    let nl: i64 = ix[I_NLAYER_ALL] - ix[I_NLAYER_NEXTN];\n"
           "    if nl < 0 as i64 {\n"
           "        out[0] = -(1 as i64);\n"
           "        return -(1 as i64);\n"
           "    }\n"
           "    tst[TS_NLAYER] = nl;\n"
           "    let av: i64 = ix[I_AV];\n"
           "    var rc: i64 = 0;\n")
    def guard_lines(block):
        out = []
        for line in block.splitlines():
            out.append(line)
            if line.lstrip().startswith(("rc = karr(", "rc = karrf(", "rc = kstrs(",
                                         "rc = kscores(", "rc = ktoktypes(",
                                         "rc = kprec(", "rc = kple(")):
                lead = line[:len(line) - len(line.lstrip())]
                out.append(lead + "if rc < 0 as i64 {")
                out.append(lead + "    out[0] = -(1 as i64);")
                out.append(lead + "    return -(1 as i64);")
                out.append(lead + "}")
        return "\n".join(out)

    bodytext = "\n".join("    " + indent(guard_lines(b), 4) for b in body)
    parts.append(sig + "\n" + bodytext + "\n\n    out[0] = 0 as i64;\n    return 0 as i64;\n}\n")

    parts.append(TENSORS)

    carry = open(SRC_CARRY, encoding="utf-8").read()
    # drop the S4 file banner, keep the code verbatim
    carry = carry[carry.index("const S_N: i64"):]

    head = ("// Port of llama_model_saver (src/llama-model-saver.cpp) to Goose.\n"
            "// S1-S4 come verbatim from the accepted writer experiment; S5 is the\n"
            "// model path: add_kv_from_model, add_tensors_from_model and the arch\n"
            "// predicate. Generated slot tables and helpers precede the carried code\n"
            "// so every name is defined before use.\n\n")
    return head + "\n".join(parts) + "\n\n" + carry


# ---------------------------------------------------------------- slots.h

KIND_CTYPE = {"bool": "bool", "u32": "uint32_t", "i32": "int32_t", "u64": "uint64_t"}


def build_slots():
    L = []
    L.append("// Generated by tests/gen.py from the native saver/hparams. Do not edit.")
    L.append("#pragma once")
    L.append("#include <cstdint>")
    L.append("#include <cstddef>")
    L.append("#include \"llama.h\"")
    L.append("#include \"llama-arch.h\"")
    L.append("#include \"llama-model.h\"")
    L.append("")
    L.append("/* enum assumptions the port hardcodes */")
    L.append("static_assert((int) LLAMA_TOKEN_TYPE_UNDEFINED  == 0, \"token type\");")
    L.append("static_assert((int) LLAMA_TOKEN_TYPE_NORMAL     == 1, \"token type\");")
    L.append("static_assert((int) LLAMA_TOKEN_TYPE_UNKNOWN    == 2, \"token type\");")
    L.append("static_assert((int) LLAMA_TOKEN_TYPE_CONTROL    == 3, \"token type\");")
    L.append("static_assert((int) LLAMA_TOKEN_TYPE_USER_DEFINED == 4, \"token type\");")
    L.append("static_assert((int) LLAMA_TOKEN_TYPE_UNUSED     == 5, \"token type\");")
    L.append("static_assert((int) LLAMA_TOKEN_TYPE_BYTE       == 6, \"token type\");")
    L.append("static_assert((int) LLAMA_TOKEN_ATTR_UNDEFINED  == 0,  \"token attr\");")
    L.append("static_assert((int) LLAMA_TOKEN_ATTR_UNKNOWN    == 1,  \"token attr\");")
    L.append("static_assert((int) LLAMA_TOKEN_ATTR_UNUSED     == 2,  \"token attr\");")
    L.append("static_assert((int) LLAMA_TOKEN_ATTR_NORMAL     == 4,  \"token attr\");")
    L.append("static_assert((int) LLAMA_TOKEN_ATTR_CONTROL    == 8,  \"token attr\");")
    L.append("static_assert((int) LLAMA_TOKEN_ATTR_USER_DEFINED == 16, \"token attr\");")
    L.append("static_assert((int) LLAMA_TOKEN_ATTR_BYTE       == 32, \"token attr\");")
    L.append("static_assert((int) GGML_PREC_Q8 == 30, \"ggml_prec\");")
    L.append("")
    L.append("#define SLOT_AV LLAMA_MAX_LAYERS")
    L.append("")
    L.append("/* model-level tensor slots, native add_tensors_from_model order */")
    for i, nm in enumerate(["TYPE_EMBD","POS_EMBD","TOK_NORM","TOK_NORM_B","OUTPUT_NORM",
                            "OUTPUT_NORM_B","OUTPUT","OUTPUT_B","OUTPUT_NORM_ENC","OUTPUT_S",
                            "OUTPUT_IN_S","OUTPUT_RES_SCORE","NEXTN_PROJ_PRE","NEXTN_PROJ_POST",
                            "CLS","CLS_B","CLS_OUT","CLS_OUT_B","CLS_NORM","HRM_Z_L_INIT",
                            "HC_HEAD_FN","HC_HEAD_BASE","HC_HEAD_SCALE","PER_LAYER_TOK_EMBD",
                            "HC_HEAD_NORM","HC_HEAD_DOWN","HC_HEAD_UP","TOK_EMBD"]):
        L.append("#define G_%s %d" % (nm, i + 1))
    L.append("")
    L.append("static const llm_kv SLOT_KEY[] = {")
    for name in KEY_SLOTS:
        L.append("    LLM_KV_%s," % name)
    L.append("};")
    L.append("static const int SLOT_NKEY = (int) (sizeof(SLOT_KEY) / sizeof(SLOT_KEY[0]));")
    L.append("")

    # key constants
    for name, idx in KEY_INDEX.items():
        L.append("#define K_%s %d" % (up(name), idx))
    L.append("")

    for idx, (kind, expr) in enumerate(H_SLOTS):
        L.append("#define %s%s %d" % ("HF_" if kind == "f32" else "H_", up(expr), idx))
    for idx, (elem, field, count, per) in enumerate(HA_SLOTS):
        prefix = "HAF_" if elem == "float" else "HA_"
        L.append("#define %s%s %d" % (prefix, up(field) + ("_P" if per else ""), idx))
    L.append("")

    L.append("static void slot_fill_hv(const llama_hparams & h, int64_t * v) {")
    for idx, (kind, expr) in enumerate(H_SLOTS):
        if kind == "f32" or expr.startswith("call:token_") or expr.startswith("call:get_") \
                or expr in ("call:n_tokens", "call:n_token_types"):
            continue
        if expr.startswith("hparams."):
            src = "h." + expr[len("hparams."):]
        elif expr.startswith("posnet."):
            src = "h.posnet." + expr[len("posnet."):]
        elif expr.startswith("convnext."):
            src = "h.convnext." + expr[len("convnext."):]
        elif expr == "call:n_expert_used":
            # the native getter aborts when there is no layer at all
            L.append("    v[H_%s] = h.n_layer_all > 0 ? (int64_t) h.n_expert_used() : 0;"
                     % up(expr))
            continue
        elif expr == "cast:pooling_type":
            src = "(uint32_t) h.pooling_type"
        else:
            raise SystemExit("slots.h: unknown h slot %r" % (expr,))
        L.append("    v[H_%s] = (int64_t) %s;" % (up(expr), src))
    L.append("}")
    L.append("")
    L.append("static void slot_fill_vocab(const llama_vocab & vocab, int64_t * v) {")
    for idx, (kind, expr) in enumerate(H_SLOTS):
        if expr.startswith("call:token_"):
            L.append("    v[H_%s] = (int64_t) vocab.%s();" % (up(expr), expr[len("call:"):]))
        elif expr == "call:n_tokens":
            L.append("    v[H_%s] = (int64_t) vocab.n_tokens();" % up(expr))
        elif expr == "call:n_token_types":
            L.append("    v[H_%s] = (int64_t) vocab.n_token_types();" % up(expr))
        elif expr.startswith("call:get_"):
            L.append("    v[H_%s] = (int64_t) vocab.%s();" % (up(expr), expr[len("call:"):]))
    L.append("}")
    L.append("")

    L.append("static void slot_fill_hf(const llama_hparams & h, float * v) {")
    for idx, (kind, expr) in enumerate(H_SLOTS):
        if kind != "f32":
            continue
        src = "h." + expr[len("hparams."):]
        L.append("    v[HF_%s] = %s;" % (up(expr), src))
    L.append("}")
    L.append("")

    L.append("static void slot_fill_ha(const llama_hparams & h, int64_t * a) {")
    for idx, (elem, field, count, per) in enumerate(HA_SLOTS):
        if elem == "float":
            continue
        if field == "dsv4_compress_ratios":
            src = "h.dsv4_compress_ratios"
        else:
            src = "h." + field
        n = HP[field][1][1] if HP[field][0] == "array" else "SLOT_AV"
        if isinstance(n, int) and n > 512:
            n = 512
        lim = "SLOT_AV" if n == 512 else str(n)
        L.append("    for (int i = 0; i < %s; ++i) a[HA_%s * SLOT_AV + i] = (int64_t) %s[i];"
                 % (lim, up(field) + ("_P" if per else ""), src))
    L.append("}")
    L.append("")

    L.append("static void slot_fill_haf(const llama_hparams & h, float * a) {")
    for idx, (elem, field, count, per) in enumerate(HA_SLOTS):
        if elem != "float":
            continue
        n = HP[field][1][1] if HP[field][0] == "array" else "SLOT_AV"
        lim = "SLOT_AV" if n == 512 else str(n)
        L.append("    for (int i = 0; i < %s; ++i) a[HAF_%s * SLOT_AV + i] = h.%s[i];"
                 % (lim, up(field), field))
    L.append("}")
    L.append("")
    return "\n".join(L)


def main():
    nodes = extract_body()
    goose = build_goose(nodes)
    slots = build_slots()
    open(OUT_GOOSE, "w", encoding="utf-8").write(goose)
    open(OUT_SLOTS, "w", encoding="utf-8").write(slots)
    print("wrote %s (%d lines) and %s (%d lines)" %
          (OUT_GOOSE, goose.count("\n"), OUT_SLOTS, slots.count("\n")))
    print("key slots=%d h=%d ha=%d" % (len(KEY_SLOTS), len(H_SLOTS), len(HA_SLOTS)))


if __name__ == "__main__":
    main()
