# EXP208: typed input address (`const f32&`) for the MMQ Q8_1 quantizer

First verifiable stage of the typed-address port. Isolated copy of EXP180 with **one** change: the
input address `x` and its four loaders move from `u64` to `const f32&`. Nothing else was mixed in.

Compiler: `subset-73da093a6df7f26b` (typed references, const preserved through generated C).

## Result

| check | result |
|---|---|
| GPU fixture, both arms | `checks=8 failures=0` (goose and native) |
| byte equality vs native | **4/4 identical** (`direct-128/512/2048`, `scatter`) |
| compute-sanitizer memcheck | **0 errors** |
| kernel SASS | typed 488 instr vs u64 432 (+56) |
| kernel timing, A/B then B/A | u64 1848.8 / 1831.9 ns vs typed 1922.6 / 1958.4 ns |

**No speedup. The typed variant is 4-6.5% slower at kernel level.** That is the honest result of
this stage and it is reported as such; the compiler release itself claims no MMQ speedup either.

## What the port actually did

- `source/device.goose`: the four `ld_f4*` externs and all seven entry points now take
  `x: const f32&`. Everything else is byte-identical to EXP180.
- The generated C carries the real typed definitions, not a re-typed copy:
  `typedef const GG_UNALIGNED fg208qd_gg_unaligned_3 *fg208qd_gg_type_5;` with `GG_ALIGN1` and
  `GG_UNALIGNED` from the same emission. `emit_sources.py` now asserts a const-reference typedef
  exists before extracting bodies.
- The CUDA bridge keeps the type the whole way: the kernel parameter is
  `const float * __restrict__ x`, `const fg208qd_gg_type_5 X = x;`, and the loader adds the byte
  offset as `char*` without converting the address back to an integer. The single u64→pointer
  conversion left is at the host ABI boundary where the packet still stores an address.
- Checks kept: the generated natural-alignment guards at every foreign reference call, plus a
  16-byte check on the composed `base+offset` before the `float4` read (safety code 900).

## Why the +56 instructions

Measured by removing pieces one at a time:

- my 16-byte guard: 16 instructions, and removing it did **not** improve the time
  (1957.9 ns without it vs 1922.6/1958.4 with), so it is not the cost;
- the remaining ~40 are the four generated `alignof` guards and the typed-edge bookkeeping.

So the regression is not "typed pointers are slow"; it is that this stage pays four alignment
checks the u64 version did not have. Whether NVCC can hoist them once the typed pointer is proven
aligned is the next question, and it is the reason no speedup is claimed here.

## Defects found and fixed on the way

1. **EXP180's committed source does not parse with any compiler in the cache.** `iqs<96{` fails
   with `expected identifier` once a `>>` appears later in the same function; reversing to
   `96>iqs` parses. Verified against stage3-871aec9, stage3-f849, subset-84f3 and subset-73da --
   all four fail on the original. The EXP208 copy carries the reversed comparison. This means the
   EXP180 numbers came from a build whose source is not reproducible from the repository; flagged,
   not silently patched for EXP180 itself.
2. `typedef`s had to be emitted separately from the bodies (`device-types-generated.h`): the
   loaders need the typed name before the primitives, the bodies need the primitives before them.
3. `_Alignof` is C11; the device build is C++, so the extractor rewrites it to `alignof`.
4. The `const` typedef names needed the module prefix too, or the bodies and the typedefs disagree.

## Artifacts

`results/exp208/sass-A.sass` (u64) and `sass-B.sass` (typed) hold the dumped kernel SASS.
Timing binaries are `runtime/exp208/build/timing-180` and `timing-208`, both linked against their
own DSO from one shared object, run A/B then B/A.

## Switch

`GGML_GOOSE_CUDA_MMQ_Q8_1=0` still restores the private-renamed native quantizer.
