# CUDA_CONTROL: integration plan and first step done

The two mandatory routes are FG195V (VMM pool, device side) and FG196P (pinned-memory and host-registration
policy, host side). Their sources exist in `experiments/EXP-2026-11-08-195-goose-cuda-vmm-pool` and
`...-196-goose-cuda-pinned-pool`, so nothing has to be invented - only built into the backend the way Q6_K now is.

## FG196P first (host policy, no device code)

`source/shared-cuda.cu` replaces four functions of `ggml-cuda.cu`:

- `ggml_cuda_host_malloc`
- `ggml_backend_cuda_host_buffer_type_alloc_buffer`
- `ggml_backend_cuda_register_host_buffer`
- `ggml_backend_cuda_unregister_host_buffer`

Step done in the working copy under `/tmp/flint-goose`: all four originals renamed to `*_native` in `ggml-cuda.cu`
(verified: 4 renamings, and the internal call from `alloc_buffer` still routes to the Goose version by design).
The four Goose bodies are extracted verbatim from `shared-cuda.cu` into `/tmp/fg196_blocks.txt` (36, 23, 40 and 30
lines), ready to be dropped into a new `fg196p-policy.cu`.

Remaining for FG196P: generate its host unit with the pinned compiler, add the file to the CMake sources list, build,
then run the accepted profile and require `e68df05b` with the FG196P witness non-zero.

## FG195V second (device side)

Device module from EXP195; same route as Q6_K: rename the original VMM-pool entry to `*_native`, add the Goose
device code, build, verify with the harness and then on the profile.

Both are checked the same way as Q6_K: correctness against native, then a controlled defect test to prove execution,
then the accepted-profile completion.

## Status: FG196P done, FG195V next

FG196P is integrated and verified. The four replaced functions of `ggml-cuda.cu` carry the Goose bodies
(`*_native` keeps the originals), the Goose host unit is linked through `fg196p-bridge.c`, and the whole thing is
reproducible from scratch with `experiments/EXP-2026-11-08-186-goose-cuda-mmvq-q6k/build_integrated.sh`.

Measured on the accepted profile:

    FG196P_CUDA_PINNED host_plan=2 host_fail=0 fallback=0 reg_plan=0 reg_fail=0 unreg=0 native=0
    first_sha e68df05b, probs_equal, restore_ok, 636 Q6_K sub-calls

Two build details that cost time and are now in the script: the policy bodies must live inside `ggml-cuda.cu`
because they use file-static helpers (`ggml_cuda_env_flag`, the buffer-free helper), and two forward declarations
are needed before the buffer-type table.

Next: FG195V (device VMM pool), same route.

## Closed

FG196P: executes on the profile (`host_plan=2`), completion unchanged.
FG195V: embedded in the pool class, fixture 5606 checks / 0 failures; `alloc=0` on this profile because the VMM pool
is not the one this workload uses - the active runtime reports the same zero.

Both are in `build_integrated.sh`, so the whole CUDA_CONTROL state rebuilds from scratch, and the accepted profile
still gives `e68df05b` with 636 Q6_K sub-calls.
