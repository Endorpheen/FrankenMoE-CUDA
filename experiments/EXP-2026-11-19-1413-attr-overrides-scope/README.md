# EXP-2026-11-19-1413: the model-name attribute overrides do not apply to the family the port serves

## What the requirement lists

P7-06 asks for the attribute overrides by model name - `phi-3`, `modern-bert`, and the `<mask>` rule for the jina/mmbert
families. The native vocabulary applies them **after** the attributes are derived, and the port's `bt_attrs` does not
implement them, which reads like a gap.

## What the code says, and what the model says

The native rules are triggered by:

| branch | trigger |
|---|---|
| `<mask>` gets LSTRIP | `tokenizer_pre` in {jina-v2-de, jina-v2-es, jina-v2-code, mmbert}, or arch in {nomic-bert-moe, jina-bert-v3} |
| phi-3 RSTRIP rules | model name contains `phi-3` or `phi3` |
| `[MASK]` gets LSTRIP | model name contains `modern-bert` |

Checked against Apex's own metadata - pre `qwen35`, arch `qwen35moe`, name `Apex-35B-V1.0`:

```
mask LSTRIP (jina/mmbert/nomic-bert-moe)      applies=False
phi-3 RSTRIP rules                            applies=False
modern-bert [MASK]                            applies=False
none applies
```

So on the model this runtime serves, the overrides cannot change a single attribute, and the port's output is complete
**for the served family**. The encoder's equality on 248 320 token texts is consistent with that: the comparison covers
every token of this vocabulary, attributes included.

## What this is not

It is not a claim that the overrides are implemented. They are not, and a family that needs one - jina, mmbert, nomic-bert,
phi-3, modern-bert - would be tokenised with the port's attributes unchanged even if its pre-tokenizer were ported
tomorrow.

That is why the serving carries a **pre-type guard**: the port is consulted only for the qwen35 family, so an attribute
override can never be skipped silently. If another family is ever served, its attribute rules move into the port first -
the same order as everything else in this migration: the rule before the serving, not after.

## State for the requirement

| item | state |
|---|---|
| connect the Goose tokenizer to the shipped path | done for qwen35, guarded (EXP-1391, -1394, -1395) |
| delete the replaced C++ tokenizer | blocked by rule 15 until acceptance |
| attribute overrides by model name | **not implemented in the port**; verified not to apply to the served family |
| pre-tokenizer families beyond BPE/qwen35 | not ported; the guard keeps them native |
