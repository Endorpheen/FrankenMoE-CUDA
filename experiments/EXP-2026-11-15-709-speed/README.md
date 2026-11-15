# EXP-2026-11-15-709 — the speed gap was the wrong libggml-cpu binary

Base: `81ff45e3`. Follows EXP-708, which ran the integrated Goose runtime at 855.71 prefill /
18.21 decode and was 60% short of the reference.

## 1. Result

| run | libggml-cpu sha256 | prefill tok/s | decode tok/s | wall |
|---|---|---:|---:|---:|
| EXP-708 | `ab6f61ea793d` | 855.71 | 18.21 | 45.3 s |
| this run | `63ad49848812` | **2052.42** | **33.62** | 19.8 s |
| reference 2026-11-13 | `63ad49848812` | 2228.76 | 41.62 | - |

prefill is now **92.1%** of the reference and decode **80.8%**, up from 38.4% and 43.7%.
Content sha256 is unchanged: `9d90a010...1ba7560`, identical to the reference.

## 2. What was wrong

The stage-run directory holds two different CPU libraries under names that differ only in
their full version suffix:

```
libggml-cpu.so.0          ab6f61ea793d   1966936 bytes   <- was being loaded
libggml-cpu.so.0.27.0     63ad49848812   1971176 bytes   <- the reference build
```

The dynamic linker resolves `libggml-cpu.so.0` by SONAME, so the plain `.so.0` file won
regardless of which one is the accepted build. `loaded-library-hashes.json` from the
reference run names `63ad4984...` as the library that produced 2228.76 / 41.62, so the
reference ran the `.0.27.0` file and our run did not.

## 3. How the difference was found

Comparing `objdump` call sites over the 300+ Goose entry points in each library:

```
304 distinct fg* targets in ab6f61, 306 in 63ad49
158c158  <  2 fg143_executor_enabled   >  3
287c287  < 17 fg149_enabled            > 19
304a305 <                             >  6 fg160_fusion_enabled
                                        >  1 fg160_query
```

The reference build calls the **fusion** module seven times; ours does not call it at all.
`fg160_fusion_enabled` and `fg160_query` are undefined in the CPU library and provided by
the base allocator/buffers modules, so the fusion decision is real and was simply absent.

This also explains the earlier, smaller discrepancy: the reference log showed
`FG148_QUANT calls=482400` and ours showed none. That count belongs to the `.0.27.0` build.
In this run FG148 is present again.

## 4. The isolated rig, and why it is legitimate

`lib/` in this experiment is a directory of symlinks to the stage-run libraries, with only
`libggml-cpu.so.0` replaced by the accepted `.0.27.0` file. Nothing in the stage-run tree was
modified, so the reference artefacts are untouched and the change is reversible by deleting
the directory.

Verified during the run by reading `/proc/<pid>/maps`:

```
loaded: experiments/EXP-2026-11-15-709-speed/lib/libggml-cpu.so.0
sha256: 63ad49848812
```

## 5. What this means for the migration

This is a **packaging** defect, not a Goose coverage gap: the accepted Goose fusion route
exists, compiles, and is called - it was just not the file the loader picked. The fix for the
real runtime is to make the accepted build the one `libggml-cpu.so.0` points at, which is a
staging change and requires no code.

Remaining gap to the reference: 7.9% prefill, 19.2% decode. Both are now in the range the
baseline document attributes to the patch set of the reconstructed tree.

## 6. Cost

Two server runs, one request each. No rebuild.
