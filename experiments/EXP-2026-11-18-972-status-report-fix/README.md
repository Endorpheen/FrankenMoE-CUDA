# EXP-2026-11-18-972: the status report was lying about the tokenizer engine

Status: the accounting script corrected. No new runtime code; the numbers it
reports are now computed instead of asserted.

## What was wrong

`tools/migration-status.sh` printed a fixed sentence:

    tokenizer engine             no load-and-tokenise path: the decisions answer,
                                 the engine around them does not

That was true before EXP949 and false after EXP950-956, which added `btl_load`,
`bt_bpe` and `bt_token_to_piece` to the unit and wired them into the shipping
library. The line kept printing the old claim for seven days of work, and the
honest-reading paragraph repeated it.

This matters more than the line itself: a status report is what the next decision
is made from. A stale assertion there is worse than no report, because it looks
like a measurement.

## What was done

- the engine line is now **computed** from the artifact, like the decisions line
  above it: `nm` counts `btl_load`, `bt_build`, `bt_bpe`, `bt_token_to_piece`, and
  the output distinguishes three states - present (4), partial (1..3), absent (0);
- the absent state is split further: if any engine source is newer than the binary,
  the report says **stale artifact** and prints the rebuild command, because an old
  binary is not evidence that the unit lacks the engine;
- the honest-reading paragraph now says what is actually missing: CUDA, HTTP
  transport, full-stack execution, and an artifact new enough to contain the
  engine - not "the engine is missing".

## Result

Before, for the same tree:

    tokenizer engine   no load-and-tokenise path: the decisions answer, the engine around them does not

After:

    tokenizer engine   stale artifact (0 engine exports; its sources are newer -
                       rebuild: bash tools/build-main-runtime.sh && bash tools/build-standalone-runtime.sh)

Measured: `builds/goose-standalone/frankenmoe-v4` is from Nov 16 10:03;
`goose/model/bpe-load.goose` is from Nov 18 06:35. The binary predates the engine,
which is why `nm` finds none of the four exports - the unit has them, this artifact
does not.

The standalone build could not be run here to refresh it: it needs
`/tmp/opencode/sub-661/tree/build-off/llama.cpp/ggml/src`, which is absent. The
report says so rather than pretending the capability is absent.

## Not claimed

No capability changed. The tokenizer engine was already in the unit and already
called by the shipping library (EXP954-957); this commit only stops the report from
contradicting the repository.

The standalone artifact is still stale, and rebuilding it is a local build whose
inputs are missing, not a migration step.

No model was started.

## Reproduction

    bash tools/migration-status.sh | grep -A1 'tokenizer engine'
    nm builds/goose-standalone/frankenmoe-v4 | grep -cE ' T (btl_load|bt_build|bt_bpe|bt_token_to_piece)'
    ls -la builds/goose-standalone/frankenmoe-v4 goose/model/bpe-load.goose
