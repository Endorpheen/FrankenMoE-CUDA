# EXP-2026-11-26-2110: Inkling 2 cumulative CUDA checkpoint

This development checkpoint combines packed Q8 prefill and fused decode projections,
batched CUDA convolution, the resident convolution-to-DepthMix-to-MoE chain,
full-prefill Flash tile softmax, and exact 16-token n-gram verification with the
original sampler. Subsequent Inkling experiments use this frozen executable
and profile as their direct control. EXP-1969 remains the historical production
reference. Development checkpoint persistence does not qualify a release.

| RTX 4070, 32K/512 | Prefill tok/s | Decode tok/s |
| --- | ---: | ---: |
| Historical control, EXP-1969 | 287.763 | 21.089 |
| Cumulative checkpoint | 309.460 | 30.321 |
| Instrumented repeat | 309.048 | 30.313 |
| Objective | 600 | 35 |

All 512 token IDs, response text and stop status match. The cumulative checkpoint
recorded 14231788 KiB peak RSS, zero process swap at the end and zero additional
system swap-out pages. End board usage was 11662 MiB; peak board usage was not
collected. The 32K fixture repeats heavily and all sampled proposals matched;
actual sampler rejection after partial acceptance remains an open release gate.
The runs were not paired. The Apex regression gate is also open.

The local executable, request, environment and raw evidence are frozen in
build/inkling2-checkpoints/2110. The source patch is relative to the EXP-1969
checkpoint and two explicitly hashed historical headers. The CUDA image
headers retain the exact images from the measured build; the accompanying
OpenCL files record newly developed kernel sources. Rebuilding requires the
historical hybrid worktree and its EXP-1880, EXP-1953 and EXP-1969 build inputs,
the original compiler and the Fried Goose compiler used by the frozen link.
From this directory, run:

```bash
bash source/build.sh /path/to/hybrid-worktree /path/to/new-build-directory
```

The build recipe checks the patch inputs and rebuilt executable hashes. This
checkpoint targets Inkling 2 only. The next decode candidate should focus on
short-batch MoE, measured at 8.91 s in the instrumented repeat, with attention
at 5.60 s and logits at 0.93 s.
