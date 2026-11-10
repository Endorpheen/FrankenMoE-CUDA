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
