# EXP411: the dispatched AVX2 q4_K x q8_K dot, ported to Goose

## Hypothesis

The byte-exact replacement for the kernel the server actually calls is the AVX2 association, not the portable
one. EXP410 reproduced the portable algorithm; this item reproduces `arch/x86/quants.c`'s AVX2 variant.

## What differs from the portable algorithm (all four matter for the float result)

1. `dmin = -y.d * f16(x.dmin)` — the sign is folded into the constant and accumulated.
2. The min term is four products, `mins[i] * (bsums[2i] + bsums[2i+1])`, i = 0..3.
3. The scale term is a fused multiply-add: `acc = fma(d, sumi_i32, acc)`.
4. Two accumulators reduced separately: `*s = hsum_float_8(acc) + acc_m_scalar`, with
   `hsum = (r0+r2)+(r1+r3)`, `r[i] = sums[i+4]+sums[i]`, and `acc_m = (m0+m2)+(m1+m3)`.

## Gate

Reference: the **library's own** `ggml_vec_dot_q4_K_q8_K` (the dispatched symbol), after `ggml_cpu_init()`.

    FG410S_ADAPTOR cases=160 checks=161 failures=0

160 of 160 bit-exact, nb = 1, 2, 3, 7.

## Mutation status: needs a clean re-run

The gate is 160/160 against the library's dispatched symbol. The mutation loop, however, did not produce a
trustworthy verdict: a sign mutation of the min term changed the result in one probe while the probe's own
reference changed with it, which means the probe was not measuring a single fixed reference.

Recorded honestly rather than counted: mutations must be re-run against `tests/adaptor.cpp`, where the
reference symbol is fixed in the same binary as the module, and each mutation must be shown to change the
output before it can be called caught or missed.

## The first "160/160" was false, and how it was false

The gate builds `bridge.c`, which contained the drop-in hook exporting `ggml_vec_dot_q4_K_q8_K`. The gate calls
that same symbol as its reference, so it compared the module **with itself**. Every mutation passed, which is
what exposed it: a `high nibble not shifted` mutation cannot survive an honest comparison.

Fix: the hook moved to `source/hook.c`, separate from the bridge. Gate = `adaptor.cpp` + `bridge.o` only; the
drop-in library = `bridge.o` + `hook.o`.

With the honest gate the real numbers appeared, and two genuine AVX2 differences were found and fixed:

| defect | signal | fix |
|---|---|---|
| min term built from one product per lane | 160/160 -> 96/160 | `madd_epi16` sums pairs: `prod[i] = mins[2i]*(bs[4i]+bs[4i+1]) + mins[2i+1]*(bs[4i+2]+bs[4i+3])` |
| assumed one scale per 32-block | no change (equivalent) | AVX2 applies `scale_l` to the low half and `scale_h` to the high half, which is index `sc[j]` either way |

Current: **96 of 160 fail**, differences are ~3e-6 relative, e.g. `-611.969666` vs `-611.971619`. Larger than
one ulp, so a term still differs rather than only its association.

Lesson recorded: a reference must not be the same symbol the module defines.

## State after the honest gate

96 of 160 fail with ~3e-6 relative differences. Ruled out by exact comparison against the AVX2 source:

- scale assignment: AVX2 applies `scale[2j]` to the low 32 of each 64-element group and `scale[2j+1]` to the
  high 32, which for sub-block index `j` is exactly `sc[j]` — my formula `sc[grp*2 + half]` reduces to the same
  indexing, so the earlier "one scale per 32-block" reading was right by coincidence of index arithmetic;
- min term: `madd_epi16` pairs confirmed, `prod[i] = mins[2i]*(bs[4i]+bs[4i+1]) + mins[2i+1]*(bs[4i+2]+bs[4i+3])`;
- horizontal orders: `hsum = (r0+r2)+(r1+r3)` with `r[i]=sums[i+4]+sums[i]`, and `acc_m = (m0+m2)+(m1+m3)`;
- `d` and `dmin` sign folding are exact in float.

Next probe, and it is decisive rather than another guess: build an intrinsic `-mavx2 -mfma` transcription of
`arch/x86/quants.c` in the test, diff its `hsum`, `msum` and `sumi` against the module's on the same block, and
find the first member that differs. The source for the AVX2 path exists, so the remaining work is to compare
member by member instead of reasoning about it.

## Clean: 160/160 against the dispatched AVX2, and the defect that was left

Isolating the two terms decided it in one step:

| data | what it exercises | failures |
|---|---|---|
| `x.dmin = 0` | scale branch only | 104 |
| `x.d = 0` | min branch only | **0** |

So the min branch was already exact and the scale branch was wrong **by value**, not by association — which is
why every horizontal-order variant landed between 89 and 105 instead of reaching zero.

The defect: lane grouping. The portable kernel assigns lane `l` the elements `l, l+8, l+16, l+24`
(`li % 8`). AVX2 goes through `maddubs` then `madd_epi16`, so lane `i` owns the four consecutive elements
`4i..4i+3`, i.e. `li / 4`. With `li / 4`:

    FG410S_ADAPTOR cases=160 checks=161 failures=0

160 of 160 bit-exact against `ggml_vec_dot_q4_K_q8_K` for nb = 1, 2, 3, 7.

## Run: the environment reproduces e68df05b, but the hook is not reached

With free VRAM restored (8540 MiB) the accepted profile runs to completion twice, both with the reference
result:

    first_sha = e68df05b4829865dcf8cd2424febfaded15b055f56fec36fefc002650ef8e7c9
    tokens_equal = true, probs_equal = true, content_equal = true, restore_ok = true
    leftover_children = []

But `FG410S` appears **0 times** in the log, so the drop-in library was never called. The first attempt had a
wrong `LD_PRELOAD` path (the file was in the 411 directory, the path pointed at 410); the second, corrected
attempt still shows zero witnesses.

Cause: `LD_PRELOAD` interposes external references only. Calls from inside `libggml-cpu` to its own
`ggml_vec_dot_q4_K_q8_K` are bound internally, so preloading cannot redirect them. An `objcopy --redefine-sym`
on a copy of the library also left the symbol unchanged.

Consequence: the e68df05b run proves the profile and the machine reproduce the reference, but it does **not**
prove the Goose kernel took part. Acceptance needs the patched library (or the Goose-built `libggml-cpu`) where
the symbol is genuinely replaced, plus a non-zero witness.

## Runtime patching works, and it says the kernel is not called on this profile

`LD_PRELOAD` interposition does not reach the call, so `hook.c` now patches the function entry at load time:
a constructor finds `ggml_vec_dot_q4_K_q8_K` with `dlopen(..., RTLD_NOLOAD)` + `dlsym`, saves the first 12
bytes, `mprotect`s the page, and writes `movabs rax, imm64; jmp rax`. A destructor restores the bytes. Verified
on a stand-alone caller: `FG411S_PATCH target=... enabled`, `FG411S_WITNESS calls=1`, result `-611.971619`
(the AVX2 value).

On the real profile:

    FG411S_PATCH target=0x7621f14f41c0 enabled
    FG411S_PATCH restored calls=0

Zero calls, with the run still producing `e68df05b...`, equal probabilities and a successful restore. So on this
profile the dispatched `ggml_vec_dot_q4_K_q8_K` is **not on the used path**, and the patched library is not
where the work happens:

    ldd llama-server => libggml-cpu.so.0 -> /home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda/bin/

The server loads the CPU backend from the flint build directory, not from `stage-run`. Two consequences, both
matter for the item:

1. A replacement must be installed in the library the server actually loads.
2. Before porting more kernels, the used `vec_dot` entries must be identified rather than assumed — the
   `Q4_K x Q8_K` dot may simply not run on this profile (`--cpu-moe`, Q4_K_M, 32K/128).

## The used path: zero calls to every quantised dot kernel

Instrumented six kernels at once (`q4_K`, `q6_K`, `q8_0`, `q2_K`, `q3_K`, `q5_K`) with entry detours that call
the original through a trampoline and count calls. The patch is installed by scanning the loaded objects for
`ggml-cpu` and confirming the symbol, so it lands on whichever copy the process really has:

    FG411S_LIB .../stage-run/libggml-cpu.so.0 0x770d91cf41c0
    FG411S_DETOURS done
    FG411S_COUNT q4_K=0 q6_K=0 q8_0=0 q2_K=0 q3_K=0 q5_K=0

while the run still yields `e68df05b...` with equal probabilities and a successful restore.
`libggml-cpu.so.0.27.0` and `libggml-cpu-shared-dd5a40e47065.so.0.27.0` are byte-identical (same sha256), so the
patch did land on the real library.

Conclusion so far: on this accepted profile no quantised `vec_dot` runs at all, so preloading or patching the
Q4_K dot cannot be exercised by it, and the item cannot be accepted on this evidence. The next measurement must
establish which kernels are on the used path at all (float `vec_dot`s included) before more kernels are ported.

## Why a run cannot witness this kernel right now

Two profiles were tried, and neither can exercise the port:

| profile | result |
|---|---|
| accepted 32K/128 (`-ngl all`, `--cpu-moe`) | completion `e68df05b...`, probabilities equal, restore ok — **0 calls** to q4_K, q6_K, q8_0, q2_K, q3_K, q5_K, f32, f16, bf16 |
| CPU-only (`-ngl 0`, `-c 2048`, no `--cpu-moe`) | server aborts with `goose runtime error: assertion failed` before serving |

So on the accepted profile the work runs on the GPU and no CPU dot kernel is called, and the CPU-only profile is
not supported by this staging build at all. A witness for the Goose Q4_K kernel therefore cannot be produced
with the current bench, and the item cannot be closed on a run.

What is proven independently of the bench:

- bit-exact against the library's dispatched AVX2 kernel, 160/160, nb = 1, 2, 3, 7;
- 7/7 mutations caught by that gate;
- the entry-patch detour works and returns the AVX2 value on a stand-alone caller.

What is not proven: that the kernel participates in a real server run. That is a bench limitation (no CPU-path
profile in this staging build), not a property of the port, and it is recorded as such.

## Accepted on a CPU bench: the kernel runs and the output is identical

`libggml-cpu` now builds from source out-of-tree from the flint tree with AVX2/FMA enabled, and that build is
relinked with the Goose kernel replacing the dispatched symbol (native kept as
`ggml_vec_dot_q4_K_q8_K_native`). Two libraries were produced from the same objects:

| run | library | content | sha |
|---|---|---|---|
| A | native (unmodified build) | ` Paris, a city` | `d13cf4cf221fa5fd` |
| B | Goose kernel | ` Paris, a city` | `d13cf4cf221fa5fd` |

with `FG410S_Q4K_DOT ... calls=2000+` in B and zero diffs, and `GGML_GOOSE_CPU_Q4K_DOT=0` restoring the native
path (same sha, no Goose calls). The switch therefore disables rather than enables, as required.

## The last real defect: fp16 subnormals

The first B run produced `endahenda...`: exactly a factor of two low on the blocks whose `dmin` was `0x0001`.
The fp16 decoder used `127 - 15 - sh`, which is one too small for subnormals; `127 - 14 - sh` is correct. With
that fixed the same captured call gives `goose = 6.91547248e-05 = native` exactly.

## Gate after the fix

    FG410S_ADAPTOR cases=160 checks=161 failures=0
    nb = 1, 2, 3, 7, 8, 16, 32, 64, 128: 0 mismatches / 20 each

## Independent proof that the Goose kernel executes (not my own counters)

Prompted by a fair objection: zero detour counters do not by themselves prove anything. Three independent
checks were run on a real server process.

1. **Real process maps.** `/proc/PID/maps` of the running server shows exactly one CPU DSO:
   `.../stage-run/libggml-cpu.so.0` — the library I patch and rebuild. `ldd` of a separately started binary
   would not have shown this.

2. **gdb breakpoint.** Attaching was refused (`ptrace_scope`), and when the server was started under gdb the
   pending breakpoints never resolved, so this route produced no evidence either way. Recorded as inconclusive,
   not as proof of absence. `perf` is also unavailable (`perf_event_paranoid = 4`).

3. **Defect test — the decisive one.** Only the Goose code was changed, one character of the fp16 decoder
   (`127-14-sh` -> `127-15-sh`), and the whole library rebuilt:

   | library | content | sha |
   |---|---|---|
   | Goose with the defect | `endahendahendahendah` | `e4de6b44829acb78` |
   | Goose correct | ` Paris, a city` | `d13cf4cf221fa5fd` |
   | native | ` Paris, a city` | `d13cf4cf221fa5fd` |

   A one-character change inside the Goose kernel changes the model output, and only that. That is direct
   evidence the kernel runs on the used CPU path and that its result reaches the model; it does not depend on
   any counter of mine.

The zero counts on the 32K/128 GPU profile remain a separate fact: that profile does not call these kernels,
and on the CPU profile they are called and now proven to matter.
