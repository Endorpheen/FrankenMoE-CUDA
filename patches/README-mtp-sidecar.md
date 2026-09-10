# MTP sidecar patch — application

`mtp-sidecar.patch` adds the split-MTP mode (a draft model consisting of only the MTP head, the trunk without MTP weights) on top of the expert-tier base. It is not self-sufficient: it is applied on top of the integration patch and its drift supplement.

## Order

```sh
git clone <upstream-llama.cpp-expert-tier> work/llama.cpp-expNNN
git -C work/llama.cpp-expNNN checkout 4aaad5d318a790a42c2197975ec8fadbad42602b
git -C work/llama.cpp-expNNN apply ../patches/expert-tier-integration.patch
git -C work/llama.cpp-expNNN apply ../patches/integration-drift.patch
git -C work/llama.cpp-expNNN apply ../patches/mtp-sidecar.patch
```

Check every step with `git apply --check`. Experiments go only into an isolated copy; never modify the working trees `work/llama.cpp-integration` and `work/llama.cpp-exp023`.

## Origin

- `integration-drift.patch` — the divergence of the published integration patch from the working state at the moment of the fork (an EHS autofit refinement in `common/common.cpp` and `src/llama-expert-hotstore.cpp` plus a comment translation). Without it the autofit paths do not match the live server.
- `mtp-sidecar.patch` — a fork of `work/llama.cpp-exp023`, branch `exp023-mtp-sidecar`, commits `aaff9b3d5` (the MTP sidecar port from cafe-llama.cpp) and `c40681659` (fully CPU draft KV). The user's comment edits are not included.

## Equivalence check (EXP-034, PASS)

`base + integration + drift + mtp` matches the working tree `work/llama.cpp-integration` byte-for-byte, except for one empty line in `src/llama-context.cpp:487` (a non-logical edit, deliberately not delivered). A build with the configuration from `benchmarks/manifests/exp034-provenance.json` passes; `llama-server --help` is identical to the live binary.
