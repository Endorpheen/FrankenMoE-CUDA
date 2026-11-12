# P1-07 closure: pack/trait round trips against the artifact, and what "matmul matches native" means here

Criterion: "Each registered pack/trait layout round-trips and matmul matches native; raw/repack/fuse."

## Round trip, against the artifact

For every type whose quantizer is exported, the check quantizes with the **native** encoder and compares the **native** decoder against
the **Goose** decoder on exactly those bytes -- artifact against port, with no transcription in between. Fourteen types,
thirty-seven thousand comparisons, zero failures (EXP345). The four types whose quantizer is not exported (IQ2_XXS, IQ2_XS, IQ1_S,
IQ1_M) are covered only decoder-versus-decoder, which is stated as the weaker check it is.

## Matmul: identity, not a comparison

The CPU dot families are native SIMD kernels, recorded as outside the Goose compiler's current scope in P1-11 and counted as audited
primitives in the P2-18 link map. Both lines execute the same kernels, so "matmul matches native" holds by construction rather than by
measurement -- there is no second implementation to diverge. The Goose work in this area is on the CUDA side (EXP173, EXP185, EXP186,
EXP235 implement device-side vector products), which is where a differential is meaningful and where those experiments already stand.

## raw/repack/fuse: absent in this substrate

The six repack types (`Q4_0_4_4`, `Q4_0_4_8`, `Q4_0_8_8`, `IQ4_NL_4_4`, `IQ4_NL_4_8`, `IQ4_NL_8_8`) are marked `REMOVED, use ...
with runtime repacking` in the type table and have no codecs of their own, as established while correcting the P2-13 inventory. There is
no raw/repack/fuse surface to port here; runtime repacking is a property of the CPU buffer layout, not of a type.

## Verdict

Every clause is either measured (round trip, fourteen types against the artifact) or explained (matmul identity, repack absent). Nothing
in the criterion is silently dropped, and the two weaker spots -- the four unexported quantizers and the identity argument -- are named.
