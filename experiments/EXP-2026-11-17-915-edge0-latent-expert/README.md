# EXP-2026-11-17-915: real-weight Edge0 latent expert graph

Status: **ACCEPTED COMPONENT; NOT A MODEL LAUNCH**.

## Hypothesis and fixed gate

The accepted typed Goose graph builder can express the pinned Edge0 routed
expert contribution without new C/C++ runtime policy or a second engine:
`U * (down * (SiLU(gate * ((D*x) * inverse_scale)) *
(up * ((D*x) * inverse_scale))))`.

The gate/up weights already contain column scales. Apply their reciprocal
exactly once after D, before quantized gate/up consumers. Do not move the scale
after the nonlinear activation or apply it a second time. This follows the
developer's `expert_layout.cpp` lines 590-620 at revision
`f6bfa5546bcf89919f86946ec669bcc7f4cd526d`.

Control: independent native GGML constructors, loaded with independently derived
file offsets and shapes. Candidate: Goose graph policy using the accepted
EXP912 typed builder and EXP909 descriptors. Compare every materialized stage,
dtype, shape, operation, graph edge and finite F32 payload **bitwise**. Test
layers 0/47, groups 0/3/7, Q4_0/Q8_0 D/U, token batches 1/3, fixed deterministic
F32 inputs. The test is not a comparison with Strata activation quantization.
Check invalid inputs before graph allocation, buffer guards, immutable weights,
scale validation, and mutations of scale placement, activation and D/U ordering.
Do not weaken equality after observing a failure.

The isolated fixture uses four CPU threads, no server, no GPU, no full-file
mapping or conversion. Target context arena is 64 MiB per case; data reads are
bounded to one expert and one D/U group. Limit a numerical gate to three minutes.
Keep old outputs/logs and rerun only affected stages when an input changes.

## Scope boundary

This is one selected routed expert with its group projections, before route
weighting/group reduction. It does not implement router top-10 selection,
shared-expert combination, PLE hashing/history/convolution, full qwen4exp
backbone, loader-owned lifetimes, CUDA scheduling or HTTP inference. It must not
be presented as a running Edge0 model. Native GGML math, allocation and CPU
execution remain dependencies; no runtime coverage percentage is inferred.

Pinned local model revision: `dab5ce727d494e2018626fe8896bfdc774215b17`.
Large-file integrity evidence from EXP908 is reused; the fixture records hashes
of every actual bounded payload it reads. No model files are modified/downloaded.

## Launch feasibility

Naive resident pack: 82.68 GiB before working buffers, larger than physical RAM.
Bounded PLE I/O and expert caching are required. Initial full-model diagnostic
should use a short context and disable MTP; its exact peak RAM/VRAM and speed
remain unknown until loader, graph and state integration have passed.

The accepted Apex bundle remains immutable. The incomplete EXP914 main Goose
work remains separate and is not included in this experiment.

## Results

All 24 cases passed: layers 0/47, groups 0/3/7, Q4_0/Q8_0 D/U, batches 1/3.
The gate compared 192 stages and 430080 F32 values bitwise, including nonzero
finite outputs. All 288 invalid graphs were refused before allocation and
without workspace writes. Native packet-read interposition had a positive
legacy control and zero reads from the candidate. Weights and scale inputs
were unchanged; output/workspace guards held. The pack's sizes/inodes/mtimes
were unchanged across the gate. Every actual weight segment has a recorded SHA256.

Five semantic mutations were rejected: reciprocal scale, omitted scale,
wrong activation, gate/up order and group offset. They failed assertions, not
crashes or compiler failures. Two inherited redundant-cast warnings remain.

Peak component RSS: **26216 KiB** (25.60 MiB), from `/usr/bin/time`; this is
not full-model RAM. GPU allocation/runs: zero. No throughput claim is made.
Artifact directory: `/home/end0/AI/LlamaServer/exp915-edge0-latent-expert-20261117`.
Only about 4.52 MiB of new generated/test artifacts were written, including
the five mutant libraries. No weights, C/C++ runtime bodies or new adapters
were added, copied or removed. The existing GGML substrate remains required.

The initial gate passed; a revised fixture added the substantive nonzero-stage
check and output hashes. Only that fixture and its dependent gate/mutations
were rerun. The original passing module check, library and logs were reused.
`e0_inverse_scales` permits disjoint input/output slices or exactly in-place
conversion; callers must not pass partially overlapping buffers. All handles
and slices obey the caller-owned lifetime contract of EXP912.

## Selective reproduction

Use `tests/build.py --out <new-LlamaServer-directory> --support
/home/end0/AI/LlamaServer/franken-recovery-20261116/support --pack
/home/end0/AI/LocalAI/Models/Edge0-120B-q3k --step <stage>`.
Stages are `check`, `build`, `fixture`, `gate`, `mutations`, in dependency order.
The accepted fixture is `--fixture-name gate-v2`. Existing logs and executable
files are deliberately never overwritten. Changed module/backend inputs require
a new artifact directory; fixture-only changes use a new fixture name.

Next: connect these accepted pieces to loader-owned providers and route/shared
composition. The requested full-model launch remains pending; no fake command
or unsupported `--edge0-pack` flag is offered as a runnable server.
