# EXP276: stop-sequence matching in Goose

P8-04's per-token stop decision: `server_slot::find_stopping_strings()` in `tools/server/server-context.cpp` plus
`string_find_partial_stop()` in `common/common.h`.

## The rules

- **full stop**: for each anti-prompt, search only the tail window `[len - (|word| + last_token_size), len)`; the earliest
  position across all words wins, and the word it belongs to is recorded (that is what sets `STOP_TYPE_WORD` and
  `stopping_word`).
- **partial stop**: the text ends with a proper prefix of the word; the native helper walks the prefix length downwards
  and compares the last byte first. Reproduced in that order.

Byte buffers throughout: the rule is byte-based in native too, so multi-byte characters need no special handling.

## Gate

    full: first wins / later in tail / none / tail window cuts   OK
    full: overlap picks earliest / earliest within window        OK
    partial: suffix / full suffix / none                          OK
    checks: 9, failures: 0

The gate was shown to fail: replacing the earliest-position test with a latest-position test breaks the
"earliest within window" case (9/1).

Two fixtures were needed before that worked, and both are worth keeping in mind:

1. the first "overlap" cases returned `-1` in **both** arms, because `last_token_size` put the match outside the tail
   window -- a case that never enters the window proves nothing;
2. the discriminating case must have two words matching at **different** positions inside the window (`"baaa"` with
   `"b"` at 0 and `"aaa"` at 1); otherwise earliest and latest coincide.

## Still to do

Integration into `find_stopping_strings()` behind a default-on switch, archive replacement, relink, and the A/B. The
accepted request does not stop on a sequence (`ignore_eos` with `n_predict`), so liveness there would need a request with
an anti-prompt -- a test request, not the accepted profile.
