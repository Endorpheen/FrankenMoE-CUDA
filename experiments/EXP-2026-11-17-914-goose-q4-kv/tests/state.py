#!/usr/bin/env python3
"""Host state/checkpoint contract, no tensor or model allocation."""
import ctypes as C
import sys

I, U, F = C.c_int64, C.c_uint8, C.c_float


class S(C.Structure):
    _fields_ = [("data", C.POINTER(I)), ("len", I)]


class B(C.Structure):
    _fields_ = [("data", C.POINTER(U)), ("len", I)]


lib = C.CDLL(sys.argv[1])
lib.goose_init.argtypes = [C.c_int, C.c_void_p]
lib.goose_init(0, None)
for name, types in {
    "q35_kv_init": [I, I, S], "q35_kv_begin": [S, I],
    "q35_kv_commit": [S, I], "q35_kv_abort": [S, I], "q35_kv_rewind": [S, I],
    "q35_kv_host_save": [S, I, B, B, S, B], "q35_kv_host_restore": [S, I, B, B, S, B],
}.items():
    fn = getattr(lib, name); fn.argtypes = types; fn.restype = I

checks = 0


def check(ok, why):
    global checks
    checks += 1
    assert ok, why


state, meta = (I * 6)(), (I * 8)()
k, v, saved = (U * 144)(*range(144)), (U * 144)(*[255-i for i in range(144)]), (U * 146)(*([219]*146))
ss, mm = S(state, 6), S(meta, 8)
kb, vb = B(k, 144), B(v, 144)
bp = C.cast(C.byref(saved, 1), C.POINTER(U)); bb = B(bp, 144)
check(lib.q35_kv_init(8, 8, ss) == 0, "init")
t = lib.q35_kv_begin(ss, 3)
check(t > 0 and list(state)[2:5] == [0, 8, 3], "begin")
check(lib.q35_kv_begin(ss, 1) == -1, "double begin")
check(lib.q35_kv_commit(ss, t) == 3, "commit")
check(lib.q35_kv_commit(ss, t) == -1, "duplicate commit")
check(lib.q35_kv_host_save(ss, 32, kb, vb, mm, bb) == 108, "save byte count")
check(bytes(saved[1:109]) == bytes(k[:54]) + bytes(v[:54]), "active prefix save")
check(saved[0] == 219 and bytes(saved[109:]) == bytes([219]*37), "save guards")
snapshot, original = list(meta), bytes(k) + bytes(v)
for field, bad in [(0, 0), (1, 16), (2, -1), (2, 9), (3, 4), (4, 64), (5, 1), (6, -1), (7, 1)]:
    before = bytes(state), bytes(k), bytes(v)
    meta[field] = bad
    check(lib.q35_kv_host_restore(ss, 32, kb, vb, mm, bb) == -1, "invalid metadata accepted")
    check(before == (bytes(state), bytes(k), bytes(v)), "invalid metadata partial write")
    meta[:] = snapshot
for fn in [lib.q35_kv_host_save, lib.q35_kv_host_restore]:
    for which in range(4):
        before = bytes(state), bytes(k), bytes(v), bytes(meta), bytes(saved)
        args = [ss, 32, kb, vb, mm, bb]
        if which < 2: args[2+which] = B(k if which == 0 else v, 53)
        elif which == 2: args[4] = S(meta, 7)
        else: args[5] = B(bp, 107)
        check(fn(*args) == -1, "short checkpoint buffer accepted")
        check(before == (bytes(state), bytes(k), bytes(v), bytes(meta), bytes(saved)), "short-buffer partial write")
t = lib.q35_kv_begin(ss, 1)
check(lib.q35_kv_abort(ss, t) == 3, "abort")
check(lib.q35_kv_commit(ss, t) == -1, "stale aborted ticket")
check(lib.q35_kv_rewind(ss, 1) == 1, "rewind")
check(lib.q35_kv_host_restore(ss, 32, kb, vb, mm, bb) == 3, "restore")
check(bytes(k) + bytes(v) == original, "restore prefix/suffix")
check(state[5] > t and lib.q35_kv_commit(ss, t) == -1, "restore resurrected old ticket")
before = bytes(state)
state[5] = (1 << 63)-1
limit = bytes(state)
check(lib.q35_kv_begin(ss, 1) == -1 and bytes(state) == limit, "ticket overflow")
check(lib.q35_kv_host_restore(ss, 32, kb, vb, mm, bb) == -1 and bytes(state) == limit, "restore ticket overflow")
print(f"PASS host checkpoint checks={checks} model_runs=0")
