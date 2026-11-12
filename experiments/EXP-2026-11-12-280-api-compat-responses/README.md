# EXP280: responses API and multi-client interleaving

Final two groups of the P8-04 compatibility suite, run against the accepted staging runtime.

## Responses API

| check | result |
|---|---|
| non-stream `/v1/responses` | `status: completed`, output text non-empty (59 chars), usage `input 15 / output 20 / total 35` |
| `max_output_tokens: 4` | the budget binds (`output_tokens = 4`) |

Recorded as a compatibility note rather than an OpenAI-exact claim: this implementation translates the responses body into
the chat-completion builder (`server-chat.cpp:295` maps `max_output_tokens` to `max_tokens`), and it does not emit
`status: incomplete` or `incomplete_details`. The observable contract is that the token budget binds; the status field
stays `completed`. If exact OpenAI semantics are wanted, that is a deliberate change to the translation layer, not a
porting gap.

## Multi-client interleaving

Two concurrent chat clients with different prompts:

| check | result |
|---|---|
| both answered | 158 and 101 characters |
| answers distinct | yes |
| usage per client | 51 and 48 total tokens |

No cross-talk: each request got its own answer and its own usage counters while sharing one slot-less queue position per
step. This complements EXP-271/273/275, where the same harness showed a second request deferring while the first held the
slot.

## Coverage summary for P8-04

Now covered by EXP278-280: completion non-stream and stream, finish reasons (`limit`, `stop`), error responses, behaviour
after a disconnect, chat non-stream and stream with reasoning content, usage accounting, the responses API, and
two-client interleaving. Still uncovered: tool calls (this profile emits none) and usage details beyond the totals.
