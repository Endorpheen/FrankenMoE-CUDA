# EXP-2026-11-15-747 — the full batch map, and the correction of two wrong conclusions

Base: `0c004dbd`. EXP-746 concluded the trigger was prefill chunking. That conclusion is wrong, and so
was EXP-744's "ubatch changes the logits". This sweep establishes what actually discriminates.

## 1. The map (512-token prompt, cache 4352, reference sha `c2ff42cc`)

| `-b` | `-ub` | effective ubatch | answer sha | matches reference? |
|---:|---:|---:|---|---|
| 4096 | 8192 | 4096 (clamped) | `c2ff42cc` | **yes** |
| 4096 | 6144 | 4096 (clamped) | `c2ff42cc` | **yes** |
| 4096 | 4096 | 4096 | `c2ff42cc` | **yes** - the reference |
| 4096 | 3584 | 3584 | `4807a9b4` | no |
| 4096 | 3072 | 3072 | `1fa8f77c` | no |
| 4096 | 2048 | 2048 | `8c50dd2d` | no |
| **8192** | **4096** | **4096** | `dc2d651b` | **no** |

## 2. Two conclusions retracted

**EXP-744 said "ubatch changes the logits".** Half right: lowering `-ub` below `-b` does. But the last
row shows **raising `-b` with `-ub` unchanged also changes the answer**. So the variable is the pair, not
ubatch alone.

**EXP-746 said "the trigger is prefill chunking".** Wrong, and this sweep disproves it directly: at
`-b 4096 -ub 8192` the ubatch is clamped to 4096, so the chunking is identical to the reference and the
answer matches - which the chunk hypothesis also predicts. But `-b 8192 -ub 4096` has **the same 4096
chunk size** and a *different* answer. Chunking is therefore not the discriminator.

The chunk-count sweep in EXP-746 (64 / 2048 / 4096 tokens) appeared to support it only because those
lengths change chunk *boundaries* at a fixed ubatch - a confound I did not control for.

## 3. What is actually true

The reference answer is reproduced **only** by `-b 4096 -ub 4096`, and by configurations that clamp to
it. Every other combination changes the output:

- `-ub` below `-b` changes it;
- `-b` above 4096 with `-ub` at 4096 changes it.

This is consistent with a **batch-size-dependent code path that is not equivalent to the reference path**
- most likely the way the MoE expert gather and the recurrent state update are organised for a batch that
is not exactly the reference shape. Not a rounding difference: the hashes are unrelated, and at the
token level the divergence is at the first or second token.

## 4. Why the reference is special

`-b 4096 -ub 4096` is what the reference argv uses, and it is the only shape whose output was ever
recorded. It is possible that the other shapes are *correct* and the reference is the odd one - but the
recorded hash and every downstream comparison in this project are anchored to the reference, so a
configuration that does not reproduce it is unusable as an equivalent replacement regardless of which is
"right".

**Establishing correctness independent of the reference needs an oracle** - a full-precision recompute,
or a second independent implementation. The Goose differential gate is the natural place for that.

## 5. Consequences

- `run-apex.sh` at `--ub 4096 --cache 4352` is correct and stays.
- All speed figures at ub != 4096 or b != 4096 describe a different computation and must carry the hash
  in the row. This is now the third card saying so, which means it belongs in the tooling rather than in
  prose.
- The mechanism is still unknown. The next step is not another sweep: it is a differential comparison of
  one decode step at two shapes, which shows *where* in the graph the two paths part.

## 6. Cost

Four configurations, ten runs, no failed starts.
