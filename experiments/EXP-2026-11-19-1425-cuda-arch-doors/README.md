# EXP-2026-11-19-1425: the CUDA architecture policy is composed and answers on known cases

## What P10-06 asks

Its first sub-item is to wire the CUDA architecture decisions into the host: version comparison, the supported-arch
default, the rewrite of a user list, and the install layout. `goose/cuda/` holds those decisions as modules; none of them
was in a build set.

## What was done, and the check that came before

`cuda-arch-policy.goose` declares **no externs**, which was verified before composing it - the lesson of EXP-1418, where
composition without a provider made the library unloadable. Composed, two doors appear and the gate stays green:

```
check-unresolved-symbols: ok
arch_resolve, arch_rewrite exported: 2
```

## The probe, on cases whose answers are known by hand

```
ver_ge(12.0 >= 12.0) = 1
ver_ge(12.0 >= 12.1) = 0
ver_ge(11.8 >= 12.0) = 0
ver_ge(13.1 >= 12.4) = 1
arch_resolve("75;86;89", "89") = 8 -> [75;86;89]
cuda arch doors: cases=5 failures=0
```

The four comparisons cover equal, one-minor-short, one-major-short and one-major-over; the resolve returns the user's list
unchanged when a list is given, which is what the policy's own comment says it should do.

## What is not done

The wiring itself. These decisions are taken at **configure time**, when CMake chooses `CMAKE_CUDA_ARCHITECTURES`; a call
site therefore lives in the build system, not in the runtime, and that is a different kind of step from the ones this
migration has been doing. Naming it matters: composing the module makes it callable, not called.

## Acceptance

```
decode 61.97 / 63.85 / 64.14   prefill 2559.0 / 2989.9 / 2983.7   hash c2ff42cc1219
```

The module is additive and nothing calls it, so the numbers are the baseline within its spread.

## Reproduce

```
LD_LIBRARY_PATH=$PWD/builds/flint-acc/bin /tmp/ca_probe $PWD/builds/flint-acc/bin/libllama.so.0
```
(compile from `probe.c` in this directory)
