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
