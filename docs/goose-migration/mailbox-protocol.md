# Mailbox protocol — read it at every checkpoint, not when convenient

Igor has asked repeatedly that the compiler/runtime mailbox be checked. It is the only coordination
channel with the agent that owns the Fried Goose compiler, and it has already carried information that
changed my conclusions. Treat it as a standing rule, not an optional courtesy.

## Where

    /home/end0/AI/Fried-Goose/.cache/runtime-compiler-link/
      to-runtime/     read these      (compiler -> me)
      to-compiler/    write here      (me -> compiler)
      acks/runtime/   my receipts
      resources.lock  shared exclusive-resource token

## When — mandatory

1. **At the start of every work iteration**, before choosing what to do.
2. Before any commit that changes the runtime lineage.
3. Before claiming a result is complete.

## How to check for genuinely new mail

    ls -lat to-runtime/*.json | head -8
    find to-runtime -name "*.json" -newer runtime/status.json

"Do I remember reading it" is not the test; a message can arrive between iterations and be older than the
last one I opened. The `-newer runtime/status.json` probe is the cheap check; bump `runtime/status.json`
after processing so the next probe is honest.

## What already came through this channel that mattered

- the named-result codegen defect, before I would have hit it (I verified it, then verified the fix);
- independent confirmation of the EXP235 `dp4a` mechanism — the compiler owner rebuilt my own harness with
  their flags and got 808 instructions / 0 `IDP.4A` for the longhand arm and 136 / 16 for the intrinsic
  arm, which is the same fact as my 741/133;
- a fix for the unused `gg_done` label I reported as minor;
- the commit-tag convention (see below);
- two requests I had left unanswered: the Apex inventory and a resource window.

## Reply discipline

Answer at the next checkpoint, once, with facts. No ACK-of-ACK, no keepalives. State what I measured and
what I did not. If a request is stale by the time I get to it, say so and say why rather than answering as
if nothing happened.

## Commit tags

The mailbox introduced a tag convention for the dashboard and Telegram relay. One tag per commit, only for
confirmed results, never for docs or unverified work:

- `#achievement` — a completed port or accepted stage
- `#insight` — a proven root cause or finding
- `#speedup` — a measured speedup, with numbers
- `#regression` — a confirmed slowdown; `#critical` only for correctness or safety

I will use these from now on. Past commits keep their messages.

## Commit tags — exact rules, read from the dashboard source

`classify_commit()` in `/home/end0/AI/Fried-Goose-Dashboard/server.py` is the authority. What it accepts:

- subject must **not** start with `merge`, `revert`, `docs:`, `docs `, `wip:`, `wip ` — those always yield no
  card, whatever else they contain;
- **exactly one** of `#speedup`, `#achievement`, `#insight`, `#regression`. Two tags in one message give no
  card at all, so `#regression #speedup` is a mistake;
- the tag counts in the subject, or on a body line that **contains only tags**; a sentence mentioning the
  convention inline does not count;
- `#critical` only modifies `#regression`; alone it does nothing;
- `#speedup` additionally requires a number in the message matching `%`, `median`, `mean`, `ns`, `tok/s`,
  `interleaved` or `runs` — an accidental tag cannot turn a hypothesis into a card.

Examples that work, from the project's own test table:

    EXP235: the fix is on my side #achievement
    EXP235: located the 19% #insight
    EXP235: wired and 19% slower #regression
    EXP235: wrong result on the gate #regression #critical
    EXP236: kernel bit-exact and at parity #achievement

Examples that produce nothing:

    EXP235: no tag here
    EXP235: both #regression #speedup        <- two tags
    docs: #achievement                       <- docs prefix
    WIP: #achievement                        <- wip prefix

Untagged commits still appear as ordinary rows; the tag only decides whether a highlight card is made.
A tag is not acceptance and not proof — the message carries its own evidence.
