# EXP-2026-11-15-704 — the two residuals attach to different tensors

Base: `1a5615c1`. No model run, no GPU.

## 1. Result

```
FG_LAYER_ORCH k=32 differs_ab=0 differs_wrong_postnorm=32 differs_wrong_input=32
              distinguishable=1 PASS
```

## 2. What was never checked before

Each block of a layer had its own proof: attention (EXP-694), recurrent (EXP-696), MoE
router and experts (EXP-697b/697c/698). The orchestration around them had none, and
`qwen35moe.cpp:186-232` is specific about it:

```
cur     = rms_norm(inpL, attn_norm)
cur     = attn(cur)
cur     = cur + inpSA          <- residual against the LAYER INPUT
ffn_res = cur                  <- saved BEFORE the post norm
post    = rms_norm(cur, attn_post_norm)
cur     = moe(post)
cur     = cur + ffn_res        <- residual against the PRE-NORM value
```

Two residuals, two different attachment points, and a post-norm that only the FFN branch
sees. Every one of those is invisible to a per-block gate.

## 3. Why the blocks are stand-ins here

The question is where results are added, not what they contain. Replacing attention and MoE
with a fixed linear map makes a wrong attachment show up as a large difference; using the
real blocks would make it show up as a difference in the noise of an already-approximate
path. The blocks keep their own proofs.

## 4. The check that stops this from being vacuous

Agreement between the two arms is not enough - any self-consistent wiring would agree with
itself. Two plausible wrong variants are computed alongside:

| variant | wiring | result |
|---|---|---|
| W1 | second residual against the **post-norm** value | differs from correct in 32/32 |
| W2 | second residual against the **layer input** | differs from correct in 32/32 |

Both are distinguishable, so the correct arm is not passing by accident. W1 is the mistake
of reusing the variable that is in scope; W2 is the mistake of treating both residuals as
the same connection. Either would leave the model running and quietly wrong.

## 5. What this does not prove

That a 41-layer stack with real blocks produces the reference tokens. It proves the glue
between proven blocks, which is the last thing that could be wrong without any block being
wrong.

## 6. Cost

Gate compiles only. No unit rebuild.
