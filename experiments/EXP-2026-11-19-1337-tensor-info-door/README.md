# EXP-2026-11-19-1337: the reader gains the tensor-descriptor accessor the loader needs

## The gap, closed

EXP-2026-11-19-1327 found that a host could find a tensor by name and not ask what it was: the records hold dtype,
dimensions, extents, byte count and data offset, and no door exposed them. That blocked the loader, which needs those
fields to replace its GGUF context at all.

`gr_tensor_info(state, tensors, index, out)` now returns them in one call - `[0]` dtype, `[1]` dimension count,
`[2..5]` extents, `[6]` elements, `[7]` bytes, `[8]` data offset - in the module's existing record layout, with the
same guard style as its neighbours.

## Verified on the real file

```
tensor info door: 753/753 equal
```

Type, dimension count and every extent match the native reader for all 753 tensors, and the data offset relative to
the data section matches as well. The check runs in the same harness that proved the records directly, so the door is
compared against native code rather than against the records it reads.

## A stale-build defect found on the way, and fixed

The first build after adding the door **did not relink**: the library still exported 14 `gr_*` symbols while the
generated header had 15. The reason is the one this project has recorded before - `vendor/flint/src/CMakeLists.txt`
listed only three modules in the custom command's `DEPENDS`, so editing a module that was composed later did not
trigger regeneration and the build kept a stale copy of the generated C.

All eleven modules the generator composes are now in `DEPENDS`, with a comment naming the two times this bit. After
that the door reaches the library: `15 gr_*` symbols.

## Why it was not noticed earlier

Because every previous composition change also edited `tools/build-sampler-chain.sh`, which *is* in `DEPENDS`, so
regeneration happened for the wrong reason. A change that only touches a composed module was the case that exposed it.
