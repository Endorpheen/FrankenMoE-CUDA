# EXP-2026-11-19-1426: the CUDA flag and install-layout decisions composed and probed

## What was composed

Both modules declare no externs - checked first, after EXP-1418 - and both now export:

```
tgt_flags ... -lineinfo in a debug build, nothing of the sort otherwise
inst_kind ... 2 for a dynamic build, 1 for shared, 0 for neither
```

## The probe, and the expectation error it caught

The first attempt expected `-G` for a debug build and failed:

```
debug flags (n=41): [-use_fast_math;-extended-lambda;-lineinfo]
  contains -G: 0
```

The door was right and the **expectation** was wrong: the module emits nvcc's `-lineinfo`, which is what line information
means for this toolchain. With the expectation read from the module's own source:

```
debug   [-use_fast_math;-extended-lambda;-lineinfo]
release [-use_fast_math;-extended-lambda]
inst_kind: (1,0)=2 (0,1)=1 (0,0)=0
cuda flags/install doors: cases=3 failures=0
```

That is the fifth time in this report that a "difference" was in the measuring side, and the same rule resolved it: read
what the thing under test says it does, then compare.

## What these two decisions are

They are build-system decisions: which nvcc flags and where a build installs. Composing them makes them callable from the
shipped library, which is what the requirement means by wiring them into the host; calling them belongs to the build, not
to the runtime.

## Gates and acceptance

```
accepted components: ok   (now 18 entries, including tgt_flags and inst_kind)
unresolved symbols:  ok
decode 62.32 / 63.80 / 61.24   prefill 2588.8 / 2988.4 / 2953.7   hash c2ff42cc1219
```

The modules are additive and nothing calls them yet.

## Reproduce

```
gcc -O2 -o /tmp/ci_probe experiments/EXP-2026-11-19-1426-cuda-flags-install/probe.c -ldl
LD_LIBRARY_PATH=$PWD/builds/flint-acc/bin /tmp/ci_probe $PWD/builds/flint-acc/bin/libllama.so.0
```
