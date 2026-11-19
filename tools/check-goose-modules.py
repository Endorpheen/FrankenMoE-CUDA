#!/usr/bin/env python3
"""Ask every composed Goose module in the shipped library a question with a known answer.

A module that stops answering - because a symbol was dropped in a rebuild, an init was lost, a table was not
built - fails silently otherwise: the C++ falls back or the bridge returns a default, and only something far
downstream notices. This checks them directly, from the library a user runs.

No model needed. Usage: python3 tools/check-goose-modules.py [path-to-libllama.so]

Caveat on the reachability witness: calling fg_gr_check here makes FG_GR_REUSE print a row count, exactly as a
model run would. A count appearing while running THIS script is not evidence that the profile reaches that
variant; only counts printed during a server run are.
"""
import ctypes
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
I = ctypes.c_int64
LETTER, UPPER, LOWER, NUMBER, WHITE, SEP = 0x0004, 0x0400, 0x0200, 0x0002, 0x0100, 0x0008


def main():
    lib_path = sys.argv[1] if len(sys.argv) > 1 else str(ROOT / "runtime/goose-stage/lib/libllama.so")
    lib = ctypes.CDLL(lib_path)
    results = []

    def check(name, fn):
        try:
            results.append((name, bool(fn())))
        except Exception as exc:                      # a missing symbol is a failure, not a crash
            results.append((name + " (" + type(exc).__name__ + ")", False))

    def sym(name, restype=I, argtypes=None):
        # Accessing lib.<name> raises AttributeError when the door is not exported by the library
        # under test, and doing it outside check() made the whole run a traceback instead of a failed
        # check - which is what happened on 2026-11-19, when fg_gr_enabled was simply absent.
        try:
            f = getattr(lib, name)
        except AttributeError:
            raise RuntimeError(f"{name} is not exported by this library") from None
        f.restype = restype
        if argtypes is not None:
            f.argtypes = argtypes
        return f

    check("graph-reuse switch defaults on", lambda: sym("fg_gr_enabled", ctypes.c_int)() == 1)
    check("arch-registry has nine recipes", lambda: sym("fg_n01s_count")() == 9)

    def apex_plan():
        lib.fg_q35s_plan.argtypes = [ctypes.POINTER(I), I, I, ctypes.POINTER(I), I]
        lib.fg_q35s_plan.restype = I
        p = (I * 32)()
        p[0], p[2], p[3], p[4], p[5], p[17], p[18] = 2048, 40, 41, 16, 2, 4, 1
        k = (I * 512)()
        n = lib.fg_q35s_plan(p, 32, 0, k, 512)
        c = {}
        for i in range(n):
            c[k[i]] = c.get(k[i], 0) + 1
        return (n, c.get(0, 0), c.get(1, 0), c.get(2, 0)) == (41, 10, 30, 1)
    check("apex plan is 41/10/30/1", apex_plan)

    # Resolved inside the checks, not before them: a symbol this library does not export must fail
    # its own check rather than end the run.
    def unicode_size():
        return sym("fg_unicode_flags_count_c")() == 0x110000

    def unicode_class():
        v = sym("fg_unicode_flags_view_c", ctypes.POINTER(ctypes.c_uint16))()
        for ch, want, mask in (('A', LETTER | UPPER, LETTER | UPPER),
                               ('z', LETTER | LOWER, LETTER | LOWER),
                               ('7', NUMBER, NUMBER),
                               (' ', WHITE | SEP, WHITE | SEP)):
            if (v[ord(ch)] & mask) != want:
                return False
        return True
    check("unicode view covers 0x110000 codepoints", unicode_size)
    check("unicode view classifies ASCII correctly", unicode_class)


    check("file-alignment switch defaults on", lambda: sym("fg377s_enabled", ctypes.c_int)() == 1)

    def reuse_true():
        lib.fg_gr_check.argtypes = [ctypes.POINTER(I), ctypes.POINTER(I), ctypes.POINTER(I), I]
        lib.fg_gr_check.restype = I
        pr, w, h = (I * 2)(1, 1), (I * 2)(5, 7), (I * 2)(5, 7)
        return lib.fg_gr_check(pr, w, h, 2) == 1

    def reuse_false():
        pr, w, h = (I * 2)(1, 1), (I * 2)(5, 7), (I * 2)(5, 8)
        return lib.fg_gr_check(pr, w, h, 2) == 0
    check("reuse decision accepts a match", reuse_true)
    check("reuse decision rejects a mismatch", reuse_false)

    for name, passed in results:
        print(("PASS " if passed else "FAIL ") + name)
    good = sum(1 for _, p in results if p)
    print(f"FG_MODULES {good}/{len(results)} " + ("PASS" if good == len(results) else "FAIL"))
    return 0 if good == len(results) else 1


if __name__ == "__main__":
    sys.exit(main())
