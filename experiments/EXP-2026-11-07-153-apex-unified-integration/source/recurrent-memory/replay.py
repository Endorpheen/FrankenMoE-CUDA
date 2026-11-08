#!/usr/bin/env python3
"""replay.py - hash-bound integration of the recurrent Goose modules.

Proposal for Codex. Reads immutable inputs/, never mutates them; writes only
worker-3/patched/.

This now performs ACTUAL hooks, not just an unused constructor:
  * every owned seq/context/state method gets an early-return hook call into the
    emitted Goose entry, while the original body stays below for rollback;
  * a Goose-managed instance is explicitly registered (fg153_hooks_register),
    unregistered instances run the original bodies (captured native reference);
  * an additive tagged constructor (metadata only) plus a friend projection
    include are added; the original constructors are untouched.
No struct offsets and no private/public macro are used.

Usage:
  python3 worker-3/replay.py record
  python3 worker-3/replay.py verify
  python3 worker-3/replay.py diff
  python3 worker-3/replay.py emit
"""
import difflib
import hashlib
import json
import os
import sys

BASE = os.path.dirname(os.path.abspath(__file__))
HERE = os.path.dirname(os.path.abspath(__file__))
INPUTS = [
    "inputs/llama-memory-recurrent.h",
    "inputs/llama-memory-recurrent.cpp",
    "inputs/llama-memory.cpp",
]
HASH_FILE = os.path.join(HERE, "source-hashes.json")
OUT_DIR = os.path.join(HERE, "patched")

HEADER_OPEN = "class llama_memory_recurrent : public llama_memory_i {"
HEADER_CTOR = "    ~llama_memory_recurrent() = default;"
CPP_ANCHOR = "void llama_memory_recurrent::clear(bool data) {"
CPP_INCLUDE_ANCHOR = '#include "llama-model.h"'

HEADER_PRE = """struct llama_hparams;

struct fg153_native_init_tag {};

"""
HEADER_FRIEND = """    friend struct fg153_recurrent_projection;
"""
HEADER_TAGGED = """    // additive Goose-init branches (metadata only); originals kept for rollback
    llama_memory_recurrent(
            fg153_native_init_tag,
            const llama_model & model,
                    uint32_t   mem_size,
                     uint32_t   n_seq_max,
                     uint32_t   n_rs_seq);
    llama_memory_recurrent(
            fg153_native_init_tag,
            const llama_hparams & hparams,
                    uint32_t   mem_size,
                     uint32_t   n_seq_max,
                     uint32_t   n_rs_seq);

"""
HEADER_TAIL = """
// Goose projection support (additive; original class untouched for rollback)
#include "fg153-projection.h"
"""
HEADER_DTOR_OUT_OF_LINE = "    ~llama_memory_recurrent();"
HEADER_CTX_OPEN = "class llama_memory_recurrent_context : public llama_memory_context_i {"
HEADER_CTX_FRIEND = "\n    friend struct fg153_context_access;"
CPP_INCLUDE_HOOKS = '\n#include "fg153-hooks.h"'
CTX_DTOR_OLD = "llama_memory_recurrent_context::~llama_memory_recurrent_context() = default;"
CTX_DTOR_NEW = """llama_memory_recurrent_context::~llama_memory_recurrent_context() {
    fg153_context_release(this);
}"""
CPP_MEM_DTOR = """llama_memory_recurrent::~llama_memory_recurrent() {
    fg153_hooks_forget(this);
}

"""

# Replace the native allocation block by a Goose branch while keeping the native
# block as the rollback `else`. Anchors are the start of the buft map setup and
# the is_empty() check after allocation.
A_START = "    // define a comparator for the buft -> ctx map"
B_END = "    if (is_empty()) {"
GOOSE_OPEN = """    int fg153_ctor_rc = 0;
    if (fg153_hooks_ctor_enabled()) {
        r_l.assign(n_layer, nullptr);
        s_l.assign(n_layer, nullptr);
        p_l.assign(n_layer, nullptr);
        fg153_ctor_rc = (int) fg153_hooks_ctor_alloc(this, &filter, (void *) &model,
                                                    (int) type_r, (int) type_s, offload ? 1 : 0,
                                                    mem_size, n_seq_max, n_rs_seq);
    } else {
"""
GOOSE_CLOSE = """    }
    if (fg153_ctor_rc != 0) {
        fg153_hooks_forget(this);
        throw std::runtime_error("failed to create rs cache");
    }
"""

CPP_TAGGED = """// additive Goose-init branch: metadata only, no tensor allocation.
// The original constructor above is preserved unchanged for rollback.
llama_memory_recurrent::llama_memory_recurrent(
        fg153_native_init_tag,
        const llama_model & model,
                uint32_t   mem_size,
                uint32_t   n_seq_max,
                uint32_t   n_rs_seq) : hparams(model.hparams), n_seq_max(n_seq_max) {
    head = 0;
    size = mem_size;
    used = 0;
    this->n_rs_seq = n_rs_seq;
    rs_idx.assign(n_seq_max, 0);
    cells.clear();
    cells.resize(mem_size);
    const int32_t n_layer = hparams.n_layer();
    r_l.resize(n_layer);
    s_l.resize(n_layer);
    p_l.resize(n_layer);
}

llama_memory_recurrent::llama_memory_recurrent(
        fg153_native_init_tag,
        const llama_hparams & hparams,
                uint32_t   mem_size,
                uint32_t   n_seq_max,
                uint32_t   n_rs_seq) : hparams(hparams), n_seq_max(n_seq_max) {
    head = 0;
    size = mem_size;
    used = 0;
    this->n_rs_seq = n_rs_seq;
    rs_idx.assign(n_seq_max, 0);
    cells.clear();
    cells.resize(mem_size);
    const int32_t n_layer = this->hparams.n_layer();
    r_l.resize(n_layer);
    s_l.resize(n_layer);
    p_l.resize(n_layer);
}

"""

# (unique method prefix, injected early-return hook line). The hook is inserted
# right after the opening brace of that method; whitespace inside the signature
# is never part of the anchor, so it is robust to alignment.
HOOKS = [
    ("void llama_memory_recurrent::clear(bool data)",
     "\n    if (fg153_hooks_active(this)) { fg153_hook_clear(this, data); return; }"),
    ("bool llama_memory_recurrent::seq_rm(llama_seq_id seq_id",
     "\n    if (fg153_hooks_active(this)) { return fg153_hook_seq_rm(this, seq_id, p0, p1); }"),
    ("void llama_memory_recurrent::seq_cp(llama_seq_id seq_id_src",
     "\n    if (fg153_hooks_active(this)) { fg153_hook_seq_cp(this, seq_id_src, seq_id_dst, p0, p1); return; }"),
    ("void llama_memory_recurrent::seq_keep(llama_seq_id seq_id)",
     "\n    if (fg153_hooks_active(this)) { fg153_hook_seq_keep(this, seq_id); return; }"),
    ("void llama_memory_recurrent::seq_add(llama_seq_id seq_id",
     "\n    if (fg153_hooks_active(this)) { fg153_hook_seq_add(this, seq_id, p0, p1, shift); return; }"),
    ("void llama_memory_recurrent::seq_div(llama_seq_id seq_id",
     "\n    if (fg153_hooks_active(this)) { fg153_hook_seq_div(this, seq_id, p0, p1, d); return; }"),
    ("llama_pos llama_memory_recurrent::seq_pos_min(llama_seq_id seq_id) const",
     "\n    if (fg153_hooks_active(this)) { return fg153_hook_seq_pos_min(this, seq_id); }"),
    ("llama_pos llama_memory_recurrent::seq_pos_max(llama_seq_id seq_id) const",
     "\n    if (fg153_hooks_active(this)) { return fg153_hook_seq_pos_max(this, seq_id); }"),
    ("bool llama_memory_recurrent::get_can_shift() const",
     "\n    if (fg153_hooks_active(this)) { return fg153_hook_get_can_shift(this); }"),
    ("bool llama_memory_recurrent::prepare(const std::vector<llama_ubatch>",
     "\n    if (fg153_hooks_active(this)) { return fg153_hook_prepare(this, ubatches); }"),
    ("bool llama_memory_recurrent::find_slot(const llama_ubatch",
     "\n    if (fg153_hooks_active(this)) { return fg153_hook_find_slot(this, ubatch); }"),
    ("llama_memory_context_ptr llama_memory_recurrent::init_batch(llama_batch_allocr & balloc",
     "\n    if (fg153_hooks_active(this)) { return fg153_hook_init_batch(this, balloc, n_ubatch, embd_all); }"),
    ("llama_memory_context_ptr llama_memory_recurrent::init_full()",
     "\n    if (fg153_hooks_active(this)) { return fg153_hook_init_full(this); }"),
    ("llama_memory_context_ptr llama_memory_recurrent::init_update(llama_context * lctx",
     "\n    if (fg153_hooks_active(this)) { return fg153_hook_init_update(this, lctx, optimize); }"),
    ("void llama_memory_recurrent::state_write(llama_io_write_i & io",
     "\n    if (fg153_hooks_active(this)) { fg153_hook_state_write(this, io, seq_id, flags); return; }"),
    ("void llama_memory_recurrent::state_read(llama_io_read_i & io",
     "\n    if (fg153_hooks_active(this)) { fg153_hook_state_read(this, io, seq_id, flags); return; }"),
    ("std::map<ggml_backend_buffer_type_t, size_t> llama_memory_recurrent::memory_breakdown() const",
     "\n    if (fg153_hooks_active(this)) { return fg153_hook_memory_breakdown(this); }"),
]

# (unique concrete-context method prefix, injected early-return hook line)
CONTEXT_HOOKS = [
    ("bool llama_memory_recurrent_context::next()",
     "\n    if (fg153_context_active(this)) { return fg153_context_hook_next(this) != 0; }"),
    ("bool llama_memory_recurrent_context::apply()",
     "\n    if (fg153_context_active(this)) { return fg153_context_hook_apply(this) != 0; }"),
    ("llama_memory_status llama_memory_recurrent_context::get_status() const",
     "\n    if (fg153_context_active(this)) { return (llama_memory_status) fg153_context_hook_status(this); }"),
    ("const llama_ubatch & llama_memory_recurrent_context::get_ubatch() const",
     "\n    if (fg153_context_active(this)) { return *(const llama_ubatch *)(intptr_t) fg153_context_hook_ubatch(this); }"),
    ("uint32_t llama_memory_recurrent_context::get_n_rs() const",
     "\n    if (fg153_context_active(this)) { return (uint32_t) fg153_context_hook_n_rs(this); }"),
    ("uint32_t llama_memory_recurrent_context::get_head() const",
     "\n    if (fg153_context_active(this)) { return (uint32_t) fg153_context_hook_head(this); }"),
    ("int32_t llama_memory_recurrent_context::get_rs_z() const",
     "\n    if (fg153_context_active(this)) { return (int32_t) fg153_context_hook_rs_z(this); }"),
    ("uint32_t llama_memory_recurrent_context::get_size() const",
     "\n    if (fg153_context_active(this)) { return (uint32_t) fg153_context_hook_size(this); }"),
    ("ggml_tensor * llama_memory_recurrent_context::get_r_l(int32_t il) const",
     "\n    if (fg153_context_active(this)) { return (ggml_tensor *)(intptr_t) fg153_context_hook_tensor(this, 0, il); }"),
    ("ggml_tensor * llama_memory_recurrent_context::get_s_l(int32_t il) const",
     "\n    if (fg153_context_active(this)) { return (ggml_tensor *)(intptr_t) fg153_context_hook_tensor(this, 1, il); }"),
    ("ggml_tensor * llama_memory_recurrent_context::get_p_l(int32_t il) const",
     "\n    if (fg153_context_active(this)) { return (ggml_tensor *)(intptr_t) fg153_context_hook_tensor(this, 2, il); }"),
    ("int32_t llama_memory_recurrent_context::s_copy(int i) const",
     "\n    if (fg153_context_active(this)) { return (int32_t) fg153_context_hook_s_copy(this, i); }"),
]

SEAM_CPP = """// seam.cpp - strong definitions of the Goose-to-Goose cells seams.
#include "bridge.h"
extern "C" int64_t fg153_goose_cells_entry(int64_t mode, int64_t a, int64_t b,
                                           int64_t c, int64_t d, int64_t e);
static int64_t fg153_cfg(int64_t h, int64_t k) { return fg153_lg(h, k, 0); }

extern "C" int64_t fg153_lc(int64_t op, int64_t a, int64_t b, int64_t c) {
    switch (op) {
        case 0:  return fg153_goose_cells_entry(0, a, fg153_cfg(b, 0), fg153_cfg(b, 1),
                                                fg153_cfg(b, 2), fg153_cfg(b, 3));
        case 10: return fg153_goose_cells_entry(10, a, b, 0, 0, 0);
        case 11: return fg153_goose_cells_entry(11, a, b, 0, 0, 0);
        case 12: return fg153_goose_cells_entry(12, a, b, 0, 0, 0);
        case 13: return fg153_goose_cells_entry(13, 0, 0, 0, 0, 0);
        default: return -1;
    }
}
extern "C" int64_t fg153_sc(int64_t op, int64_t a, int64_t b, int64_t c) {
    switch (op) {
        case 1:  return fg153_goose_cells_entry(1,  a, b, 0, 0, 0);
        case 2:  return fg153_goose_cells_entry(2,  a, b, -1, -1, 0);
        case 9:  return fg153_goose_cells_entry(9,  a, b, c, 0, 0);
        case 10: return fg153_goose_cells_entry(10, a, b, 0, 0, 0);
        case 18: return fg153_goose_cells_entry(18, a, b, c, 0, 0);
        case 19: return fg153_goose_cells_entry(25, a, b, 0, 0, 0);
        default: return -1;
    }
}
"""


def _p(rel):
    return os.path.join(BASE, rel)


def read_text(rel):
    with open(_p(rel), "r", encoding="utf-8") as f:
        return f.read()


def sha256(rel):
    h = hashlib.sha256()
    with open(_p(rel), "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def record():
    data = {rel: sha256(rel) for rel in INPUTS}
    with open(HASH_FILE, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)
        f.write("\n")
    print("recorded", HASH_FILE)


def verify(verbose=True):
    if not os.path.exists(HASH_FILE):
        print("NO RECORD: run `record` on the trusted inputs snapshot first", file=sys.stderr)
        return False
    with open(HASH_FILE, "r", encoding="utf-8") as f:
        want = json.load(f)
    ok = True
    for rel in INPUTS:
        got = sha256(rel)
        if want.get(rel) != got:
            print(f"HASH MISMATCH {rel}: recorded {want.get(rel)} got {got}", file=sys.stderr)
            ok = False
        elif verbose:
            print(f"ok {rel} {got}")
    return ok


def insert_after(text, anchor, payload, what):
    if payload in text:
        return text
    idx = text.find(anchor)
    if idx < 0:
        raise SystemExit(f"anchor not found: {what}")
    end = idx + len(anchor)
    return text[:end] + payload + text[end:]


def insert_before(text, anchor, payload, what):
    if payload in text:
        return text
    idx = text.find(anchor)
    if idx < 0:
        raise SystemExit(f"anchor not found: {what}")
    return text[:idx] + payload + text[idx:]


def insert_method_hook(text, prefix, payload, what):
    """Insert payload right after the opening brace of the unique method prefix."""
    if payload in text:
        return text
    idx = text.find(prefix)
    if idx < 0:
        raise SystemExit(f"method not found: {what}")
    nl = text.find("\n", idx)
    if nl < 0:
        nl = len(text)
    brace = text.rfind("{", idx, nl)
    if brace < 0:
        raise SystemExit(f"opening brace not found for: {what}")
    pos = brace + 1
    return text[:pos] + payload + text[pos:]


def insert_before_method(text, prefix, payload, what):
    if payload in text:
        return text
    idx = text.find(prefix)
    if idx < 0:
        raise SystemExit(f"method not found: {what}")
    line_start = text.rfind("\n", 0, idx) + 1
    return text[:line_start] + payload + text[line_start:]


def insert_before_ctor_close(text, prefix, payload, what):
    """Insert payload just before the closing brace of the first function whose
    definition starts with prefix. Braces inside the body are balanced; the
    constructor body contains no braces in string literals."""
    if payload in text:
        return text
    idx = text.find(prefix)
    if idx < 0:
        raise SystemExit(f"ctor not found: {what}")
    brace = text.find("{", idx)
    if brace < 0:
        raise SystemExit(f"ctor body not found: {what}")
    depth = 0
    for i in range(brace, len(text)):
        ch = text[i]
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return text[:i] + payload + text[i:]
    raise SystemExit(f"unbalanced ctor body: {what}")


def build_patched():
    h = read_text(INPUTS[0])
    c = read_text(INPUTS[1])

    h = insert_before(h, HEADER_OPEN, HEADER_PRE, "class open")
    h = insert_after(h, HEADER_OPEN, HEADER_FRIEND, "friend")
    h = insert_before(h, HEADER_CTOR, HEADER_TAGGED, "dtor anchor")
    if HEADER_TAIL not in h:
        h = h.rstrip("\n") + "\n" + HEADER_TAIL
    # out-of-line destructor so unregister is guaranteed (vtable slot unchanged)
    if h.count(HEADER_CTOR) != 1:
        raise SystemExit("expected exactly one inline recurrent destructor")
    h = h.replace(HEADER_CTOR, HEADER_DTOR_OUT_OF_LINE, 1)
    h = insert_after(h, HEADER_CTX_OPEN, HEADER_CTX_FRIEND, "context friend")

    c = insert_after(c, CPP_INCLUDE_ANCHOR, CPP_INCLUDE_HOOKS, "includes")
    c = insert_before_method(c, "void llama_memory_recurrent::clear(bool data)",
                             CPP_TAGGED + CPP_MEM_DTOR, "clear")
    for prefix, payload in HOOKS:
        c = insert_method_hook(c, prefix, payload, prefix[:60])
    if CTX_DTOR_OLD not in c:
        raise SystemExit("context destructor default definition not found")
    c = c.replace(CTX_DTOR_OLD, CTX_DTOR_NEW, 1)
    for prefix, payload in CONTEXT_HOOKS:
        c = insert_method_hook(c, prefix, payload, prefix[:60])
    # Goose ctor allocation/resource ownership, with the native block kept in the
    # rollback `else` branch. Wrapped block spans A_START..B_END.
    c = insert_before(c, A_START, GOOSE_OPEN, "alloc block start")
    c = insert_before(c, B_END, GOOSE_CLOSE, "alloc block end")
    return h, c


def unified(rel, old, new):
    return "".join(difflib.unified_diff(
        old.splitlines(True), new.splitlines(True),
        fromfile="a/" + rel, tofile="b/" + rel))


def diff():
    h, c = build_patched()
    sys.stdout.write(unified(INPUTS[0], read_text(INPUTS[0]), h))
    sys.stdout.write(unified(INPUTS[1], read_text(INPUTS[1]), c))


def emit():
    if not verify(verbose=False):
        sys.exit("refusing to emit: inputs are not hash-bound")
    h, c = build_patched()
    os.makedirs(OUT_DIR, exist_ok=True)
    with open(os.path.join(OUT_DIR, "llama-memory-recurrent.h"), "w", encoding="utf-8") as f:
        f.write(h)
    with open(os.path.join(OUT_DIR, "llama-memory-recurrent.cpp"), "w", encoding="utf-8") as f:
        f.write(c)
    for name in ("fg153-projection.h", "bridge.h", "services.cpp",
                 "fg153-hooks.h", "fg153-hooks.cpp"):
        with open(os.path.join(HERE, name), "r", encoding="utf-8") as f:
            body = f.read()
        with open(os.path.join(OUT_DIR, name), "w", encoding="utf-8") as f:
            f.write(body)
    with open(os.path.join(OUT_DIR, "seam.cpp"), "w", encoding="utf-8") as f:
        f.write(SEAM_CPP)
    print("emitted", OUT_DIR)


def main():
    cmd = sys.argv[1] if len(sys.argv) > 1 else "verify"
    if cmd == "record":
        record()
    elif cmd == "verify":
        sys.exit(0 if verify() else 1)
    elif cmd == "diff":
        diff()
    elif cmd == "emit":
        emit()
    else:
        sys.exit(f"unknown command {cmd!r}")


if __name__ == "__main__":
    main()
