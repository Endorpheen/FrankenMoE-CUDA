# EXP531: caps usage flag and detector (P7-08 slice 5, part 21)

## Hypothesis

`jinja::caps::analyze` decides `supports_system_role` dynamically: it renders a
probe with `[system "System message", user "User message"]` and clears the flag
when `messages[0].content` was not used. A Goose port needs a usage flag in the
value model and a way to read it after a render. That worked.

## What is in this slice

| change | rule |
|---|---|
| arena | the value record gained an eleventh field, `st[27]`, holding the usage flag; `bases()` and `ast_bases()` were widened, and the fixture writes the pool at `11 * valcap` |
| marking | `append_rendered` marks every value it renders, and `vnew` clears the flag for new values |
| probe | `chat_probe_used(A, st, id)` reads a flag; `chat_probe_content_used(A, B, st, msgs)` walks `messages[0]` keys and returns the `content` flag; `chat_probe_fresh` checks a fresh value starts unused |

## The defect that blocked the first attempt

The probe read the key bytes as `B[st[18] + k ..]`, but `st[18] + k` is the
*index into the arena*, not the byte offset: the offset lives in
`A[st[18] + k]` and the length in `A[st[19] + k]`. Every key compare therefore
failed and the probe reported 0. Reading the two fields first fixed it:

```
let koff = A[st[18] + k];
let klen = A[st[19] + k];
let ks = B[koff .. koff + klen];
```

The same mistake had been made in the debug export, which is why the earlier
debug output looked plausible while the probe stayed at 0.

## Gate

The native expectation comes from `common_chat_template(src, "", "").original_caps().supports_system_role`;
our side renders the probe through the same pipeline as the template gate and
reads the flag, so only the flag is compared (the rendering is compared by the
other checks).

```
FG531_RUNTIME_CAPS cases=394 values=775 failures=0
mutations 6/6 caught (range 0:6)
mutations 6/6 caught (range 6:12)
baseline after restore: rc=0 cases=394 values=775 failures=0
```

Four caps cases: the role/content loop (supported), the role-only template
(unsupported), a conditional-content template (unsupported), and
`{{ messages[0]['content'] }}` (supported), plus the fresh-value unit check.

Mutations: marking disabled or written into another field, the probe always
unused or always used, wrong key match, the key flag returned instead of the
value flag, key offset/length swapped, fresh values starting used, the message
array ignored, an empty key walk, and matching any key.

## Not ported

- The remaining caps probes (`supports_typed_content`, tools, reasoning,
  `has_op` history). Only the system-role rule is ported, because it is the one
  the chat-layer workarounds consume.
- Typed content normalization, tool schemas/grammars, the autoparser and model
  parsers.
