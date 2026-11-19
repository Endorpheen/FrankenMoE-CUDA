# EXP-2026-11-18-1002: The Apex plan bridge is a wrapper, not a second implementation

Status: `fg-q35-bridge.o` is linked and uncalled, and it is **not** a duplicate of the
unit's decision - it calls the unit. Both produce the same 41-layer plan, so removing it
removes an indirection, not a decision. 38 324 checks.

## What EXP1001 left

`check-dead-objects.py` found one object whose every export is uncalled:
`fg-q35-bridge.o`, 5 exports. Uncalled is not the same as redundant - it could be the
only correct implementation - and rule 15 forbids removing before the replacement is
proven, so the verdict needed a comparison.

## What the comparison found

Reading the bridge first changed the question. `integration/m1/common/fg-apex-plan-bridge.c`
is 30 lines of adapter:

    static void ensure_init(void) {
        if (!g_started) { fg_q35_goose_init(0, 0); fg_qg_goose_init(0, 0); g_started = 1; }
    }
    int64_t fg_q35s_plan(...) { ensure_init(); return q35_plan(...); }

`fg_q35s_plan`, `fg_q35s_kind`, `fg_q35s_shape` and `fg_q35s_qg_split` all forward to the
unit's exports. So it is not a parallel implementation; it is `ensure_init` plus a cast.
That makes it **removable once the same initialisation happens at the real call site** -
which is a change to how the library starts, not a deletion, and is not this cycle's job.

Measured anyway, on Apex's real shape:

| comparison | result |
| --- | --- |
| layer plan, both paths | **41 layers, every kind equal** |
| attn / recurrent / MTP | **10 / 30 / 1**, matching EXP-699 |
| per-layer kind, including layers -1 and 41 | equal |
| shape for 3 kinds x 4 tensor ids | 12 comparisons, all equal |

## Four failures, each a different wrong assumption

This gate took five attempts and every attempt failed for its own reason, all of them
mine. Written down because each is a trap this project can hit again:

1. **`goose_init` is not the initialiser.** The unit's objects are compiled with
   `-Dgoose_init=<module>_goose_init`, one symbol per composed object. Calling the
   library's plain `goose_init` leaves exports refusing with `goose_init must be called
   before exported functions` - a message that names a symbol the unit does not use.
2. **`fg950_goose_init` is the wrong object.** That is the tokenizer unit. The Apex plan
   lives in `fg-q35.o` and `fg-qg.o`, which need `fg_q35_goose_init` and
   `fg_qg_goose_init`.
3. **`LD_LIBRARY_PATH` order decided which library ran.** With the accepted run directory
   first, the loader satisfied `libllama.so.0` from the *accepted* library and my symbol
   was missing there. `ldd` showed it; the fresh directory must come first.
4. **The slice ABI, again.** I declared `q35_plan(const int64_t *, int64_t, int64_t *)`,
   which links and then hangs, because the callee reads a struct as a pointer. EXP905
   recorded this exact trap; it caught me a second time. Correct form:
   `q35_plan(sl_i64, int64_t, sl_i64)`.

Every one of these was silent in the direction that matters: the first three produced no
output at all, and the fourth produced `-99` - a number that looks like a real answer.

## Result

| check | result |
| --- | --- |
| q35-bridge-redundant | 5 checks, 0 failures |
| plan agreement | 41/41 layers, all kinds |
| shape agreement | 12/12 |
| dead object | still linked; removal needs the init moved to the call site |
| whole suite | **38 324** checks, 0 failures |

## Not claimed

The bridge was **not removed**. It is redundant as an implementation, but it also carries
`ensure_init`, so deleting it now would break any caller that relies on the bridge to
initialise the unit. Removing it means making `llama-model-loader.cpp` call
`fg_q35_goose_init` itself, then deleting the object in the same pass - one change, and
it needs the full suite.

`fg-q35-bridge.o` also exports `fg_q35s_qg_split`, which the gate does not compare. That
is a gap, stated rather than assumed covered.

No model was started, no benchmark was run.
