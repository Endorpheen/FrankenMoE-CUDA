# EXP256: MMA-dot coefficient arithmetic, host gate first

The shared MMA dot has exactly one piece of float arithmetic outside `mma()` itself:

    float2 dmA = __half22float2(x_dm[i*sram_stride + k0/QI8_1]);
    float2 dsB = __half22float2(y_dm[j*MMQ_TILE_Y_K + k01/QI8_1]);
    sum[...] += dmA.x*dsB.x*C.x[l];
    sum[...] += dmA.y*dsB.y;

## Gate

    FG256S_GATE cases=20000 failures=0

Twenty thousand random bit patterns for both operand pairs, compared bit for bit against `float*float` in C.

## The mistake worth recording

This module is **host-side**, and the code it is meant to replace runs in a **device** function. A host function cannot
be called from there. So this gate proves the arithmetic, but it cannot be wired in as written: the same body has to
be emitted as device code, exactly as EXP255 did, before substitution.

That is not a wasted step -- the arithmetic is now pinned down and its gate exists -- but pretending it is integrated
would be false. Next: emit the same body as a device unit and call it from `mmq-vec-dot.cuh`.
