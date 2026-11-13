# EXP320: KV rotation matrices in Goose

The Walsh-Hadamard generator and the `n_rot_k` / `n_rot_v` choice behind
`set_input_k_rot` / `set_input_v_rot` (P5-02).

## Verdict

**GATED, NOT INTEGRATED** — the recursion and the size policy are exact; the source
tree is read-only. No performance claim. The memcpy of the precomputed matrix into a
tensor stays native.

## Scope transferred

- **Generator**: Sylvester's recursion. The corner is 1, and every doubling step
  copies the block into the right and bottom quadrants and negates the bottom-right
  one, so entries are ±1 and the norm comes from scaling the corner by `1/sqrt(n)`.
  The gate compares the integer signs and checks orthonormality of the reconstruction.
- **Rotation sizes**: K starts at 64 and doubles while the whole key width is still a
  multiple of twice the size; V is fixed at 64 and only applies to a quantised V
  whose head width is a multiple of 64; the K rotation is enabled for the
  lightning-indexer architectures when the full key width equals the indexer head
  size.

## Gate

| check | result |
|---|---|
| generator, orders 1–128 (2 000 draws) | 5 815 000 signs + orthogonality |
| rotation sizes, 4 000 draws | 16 000 checks |
| total | 5 819 576 checks / 0 failures |

Orthogonality is sampled every 7th row against every 5th row, on the reconstructed
matrix (`H_int / sqrt(n)`), so a wrong sign shows up as a non-zero dot product even
where the pattern is not obvious.

## Mutations

| mutation | failures |
|---|---:|
| corner starts negative | 5 627 084 |
| bottom-right quadrant is not negated | 2 952 579 |
| right quadrant is never written | 4 960 638 |
| doubling step advances by one | 2 952 383 |
| rotation size test uses the size without the factor two | 19 |
| rotation size starts at 32 | 997 |
| value rotation ignores the head-width multiple | 1 880 |
| indexer architectures rotate without the width match | 1 914 |

All eight are caught. The narrow one is the factor-two test: it only shows when the
key width is a multiple of the current size but not of twice it — 19 checks out of
four thousand draws, which is why the widths are drawn up to 4096 rather than a small
range.

## Files

- `source/host.goose` — the recursion and the policy plus a one-entry dispatcher.
- `tests/fixture.cpp` — transcription, sign comparison and orthogonality sampling.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
