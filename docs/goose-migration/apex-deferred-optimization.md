# Deferred plan: one optimization item after the small Apex path

Recorded from Igor's order on 2026-11-11. Nothing here is started yet; the sequential transfer continues
first.

## Precondition

The step opens only when **every original item** of the small Apex path is really done by its own criterion
— not because the number of live markers went up. Remaining criteria are checked item by item before the
step is declared open.

## Step 0 — final run of the whole path

One run of the complete path: accuracy, configuration, prefill/decode, RAM and VRAM, limitations and the
rollback story. One run, not a series.

## Step 1 — recover the pre-regression numbers

1. Restore the already-existing optimizations and patches first, from the actual inventory (expert tier,
   pinned ring, mmap expert registration and whatever else the inventory shows).
2. Then look for other causes.
3. Do not attribute losses to the ported code when they come from a different build, configuration or
   parallel load.

Remembered orientation: about 2700 prefill / 27 decode and about 3400 MiB VRAM. **This is a memory of the
baseline, not a confirmed reference.** Higher figures 2900/29 were mentioned earlier too. Before any
acceptance the baseline must be extracted from the saved original runs and compared on identical
configurations and identical memory accounting. No RAM threshold is invented.

## Step 2 — the targets

Prefill 3000+ tok/s and decode 30+ tok/s, with memory reduced where possible. VRAM below 3400 MiB is a
wanted improvement, never a promise.

## Rules for the step

- Final conclusions only from comparable A/B→B/A on a host with minimal load; single runs under load are
  only a trend.
- Do not substitute transferring with excluding unported parts from scope.
- Never trade correctness or safety for numbers.
- No new automatic notifications; Igor asks Codex for the saved numbers himself. Telegram stays untouched.
