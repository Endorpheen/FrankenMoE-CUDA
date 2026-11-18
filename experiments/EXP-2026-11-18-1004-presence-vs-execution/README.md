# EXP-2026-11-18-1004: Twelve are present, none are called

Status: presence and execution are now measured separately, and the answer differs
between the accepted artifact and a fresh build. No code changed.

## The question EXP1003 left

After the correction from 0 to 12, the honest next question was whether those twelve
**run**. A symbol can be exported, reachable by a client, and called by nobody inside the
library - which is precisely how the Apex plan looked alive for several cycles.

## The measurement

`tools/check-symbol-callers.py` reads the disassembly and asks whether any `call`
instruction targets a name or its PLT stub.

Accepted library:

    called: 0 | exported but called by nobody: 11

Fresh build from current sources:

    CALLED   btl_load, bt_bpe, bt_token_to_piece, tp_apply, tns_apply, dry_apply
    UNCALLED q35_plan, q35_kind

## The tool was tested against known-live symbols first

A count of zero from a new instrument is the exact failure that started this thread, so
the instrument was pointed at symbols with an obvious live caller:

    CALLED   llama_sampler_init_dist, llama_sampler_apply, q35s_split,
             fg386s_pick, fg386s_enabled
    UNCALLED f30s_split

Five found, one missed correctly. The zero on the Apex exports is therefore a measurement,
not a broken pattern.

`q35s_split` is worth noting: it is the one `q35`-prefixed symbol with a caller, and it
belongs to `fg-qwen35-split-bridge.c` - a frozen bridge - not to the unit's exports. So
the family has one live path and it is not the canonical one.

## What this means, stated precisely

**Accepted artifact:** the twelve exports exist and nothing inside calls them. A client
could call them; the library does not. The Apex plan is *shipped but not executed* there.

**Fresh build:** six of eight sampled exports are called from inside - all of the
tokenizer engine and all three sampler filters. The two that are not (`q35_plan`,
`q35_kind`) are called only through `fg-q35-bridge.o`, which EXP1002 measured as uncalled.

So the picture is not "the unit is unused"; it is specific: the tokenizer and filters run
in a fresh build, and the Apex plan does not run in either build.

## Report

    unit exports in accepted lib 12 of the header's functions
                                 (ar_arch ... qg_split)
    ... of which something calls 0 (the rest are exported only)

That third line is the one that was missing for five cycles.

## Not claimed

`nm`/`objdump` see direct calls. An indirect call through a function pointer, a `dlsym`
lookup by name, or a call from a different shared object would not appear. The Apex plan
is exported, so an external caller (a test binary, another DSO in the run directory) could
reach it - `integration/goose-standalone/tests/*.c` do exactly that. "Called by nobody
inside this library" is what was measured, and it is not "never called".

No gate re-run: nothing under `goose/` changed.

No model was started, no benchmark was run.
