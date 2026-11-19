# EXP-2026-11-19-1392: the guard that keeps the ported encoder away from other families, and the proof it refuses

## Why a guard and not an option

`bt_bpe` calls one hardcoded segmenter (`pt_qwen35_body`); the native tokenizer has eighteen pre-tokenizer families.
Serving the port for a vocabulary it was never written for would produce plausible, silently wrong ids - the
single-architecture shortcut rule 17 forbids. So the table is built only when the file says the port's family, and the
native encoder remains the only one otherwise.

```
if tokenizer.ggml.pre == "qwen35"  -> the table is built, the comparison runs
otherwise                          -> no table, no comparison, native only
```

## The proof it permits

Apex declares `tokenizer.ggml.pre = qwen35`:

```
FG_TK_BPE_GUARD pre_supported=1
FG_TK_BPE_LOAD  ok n_tok=248320 n_mrg=247587
FG_TK_BPE_COMPARE calls=1 disagreements=0
case 0..3: 2, 5, 8, 2 tokens - all equal
```

## The proof it refuses

The fixture vocabulary also declares `qwen35`, so it cannot test the refusal. The test therefore patched the string in
place - `qwen35` to `qwen36`, same length, so the GGUF stays structurally valid - and ran the same harness:

```
FG_TK_BPE_GUARD pre_supported=0
```

and nothing else: no table line, no comparison, no ids. The port is not consulted at all for a vocabulary of another
family, which is the behaviour the rule requires, and it is demonstrated rather than asserted.

## What this closes, and what it leaves

Closed: the pre-type guard, which was one of the three items between the encoder's equality and serving.

Left:
- a wider corpus through a server with chat templates, where specials and BOS/EOS boundaries are produced by the template
  rather than by a hand-written string;
- the library-side `--no-bos` path;
- serving itself, which needs its own acceptance triple with the hash.

## Shipped path

Unchanged: everything behind `FG_TK_BPE_DIAG`, acceptance 62.52 / 63.13 / 62.96 with hash `c2ff42cc1219`.
