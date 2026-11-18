# EXP-2026-11-18-1183: P7-06 symbol emission (resegment)

After merging, the surviving symbols are emitted: a symbol that is a token goes out directly,
a symbol that is not is split back into the two halves it was merged from, recursively, and a
leaf that matches nothing falls back to one token per byte of its text.

```
resegment: cases=6 fails=0
```

## Two defects of mine, both caught before the gate was green

1. **Conflated sizes.** I passed one number where two are needed: the length of the chain to
   emit and the size of the symbol table the children index into. With one number the function
   skipped every merge child because its index was beyond the chain length, and two cases
   emitted nothing:
   ```
   MISMATCH merge of tokens got=0 want=2
   ```
   The signature now takes `nchain` and `ntab` separately.
2. **A table row that recursed forever.** Two of my cases pointed a merge child back at its own
   parent, so the reference recursion in my harness never terminated and the run died with a
   segmentation fault. The rows now attach real leaves.

The second one is worth naming: the crash was in my *test table*, and the first instinct was to
blame the port. Reading the row showed the parent-child cycle immediately.

The stack is a fixed 64-entry array with a bounds check on push, because the native code
recurses and recursion is not available here; depth is bounded by the merge tree, which for a
token of at most a few hundred bytes is far below 64.
