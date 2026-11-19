# EXP-2026-11-18-1005: The Apex plan reads the file's own array

Status: the plan module now reproduces the recurrent-layer flags **read out of the real
GGUF**, and a gate proves it against that file. 10 checks. No behaviour changed - the
module already supported this branch; nothing exercised it.

## What was missing

`vendor/flint/src/models/qwen35moe.cpp:20-27` decides the layer kinds in two ways:

    if (!get_key_or_arr(ATTENTION_RECURRENT_LAYERS, is_recr_impl, n_layer_all)) {
        interval = 4; get_key(FULL_ATTENTION_INTERVAL, interval);
        is_recr_impl[i] = (i < n_layer()) && ((i + 1) % interval != 0);
    }

The **array is preferred**; the interval formula is the fallback. Every existing gate
passed `flags_base = 0`, which selects the formula - so the tested branch was the one a
real file does not use.

The Apex GGUF declares the array:

    qwen35moe.attention.recurrent_layers: array of int32, n=41
      1 1 1 0 1 1 1 0 ... 1 1 1 0 0

## Result on the real file

    layers 40 + 1 nextn = 41, interval 4
    plan lengths: formula 41, array 41
    formula and array agree on this file: yes
    array plan: attn 10 recr 30 mtp 1
    PASS checks=10 failures=0

Both branches were run and compared against an independent recomputation from the
file's own flags, so this is not the module agreeing with itself.

**The formula happens to be right for this file.** That is luck, not design: if a future
GGUF declares an array the interval does not describe, the accepted code path would be
wrong and nothing would have shown it. The gate now covers the branch that matches the
loader.

## A gate bug worth recording

The first run failed with the array plan giving 25 attention / 15 recurrent instead of
11/30, and the gate accused the module. It was mine: `fg200g_ld_u32` reads with a
**4-byte stride** (`integration/m1/host/goose_host.c:289`) because `is_recr_impl` is
`std::array<uint32_t, N>`, and I passed a `std::vector<int64_t>`. Every second layer read
the high half of the previous element as zero.

So the module is right, and a wrong buffer stride reproduced as a wrong decision - which
is the failure mode that makes layout errors expensive: it does not crash, it answers.

## Suite

15 gates plus one finding, 0 failures. Unit unchanged: nothing under `goose/` was edited.

## Not claimed

The shipping loader still decides this itself; connecting `q35_plan` to it is the wiring
step, and EXP1004 measured that neither build calls `q35_plan` from inside. This cycle
proves the branch works, not that it is used.

"Formula and array agree on this file" is a fact about one GGUF, not a rule. The gate
prints it and does not fail on it, because the two disagreeing would be legitimate.

A `/tmp` quota exhaustion produced a zero-length log and a silent `total=` on the first
attempt; output goes to the working directory now. Worth naming because a silent empty
log looks exactly like a suite that ran and found nothing.

No model was started, no benchmark was run.
