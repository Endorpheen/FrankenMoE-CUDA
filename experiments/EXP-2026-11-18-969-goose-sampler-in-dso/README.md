# EXP-2026-11-18-969: the sampler filters are in the shipping library

Status: build + live gate PASS. In the DSO and verified there; still not called by
the sampler chain.

## Hypothesis fixed before changes

EXP966-968 verified the three filters against the accepted library's live
samplers, but checked first: `nm` on the built DSO showed **none of the three was
in it**. They were unit members from EXP962 and absent from the shipping library -
the same gap EXP951 closed for the tokenizer and EXP962/963 closed inside the unit.

If they are added to the carrier composition, the shipping DSO must contain them
and the live gates must still pass against that DSO rather than against an
experiment-only build.

## What was done

`runtime/reconstructed/build.py` `_CARRIER_UNIT` gains
`misc/sampler-typical.goose`, `misc/sampler-top-n-sigma.goose` and
`misc/sampler-dry.goose`. They join the tokenizer unit rather than getting their
own object: one unit, one initialiser, no new adapter.

The gates then had to learn one integration fact the hard way: inside the DSO the
composed unit's initialiser is **renamed** to `fg950_goose_init`, so `goose_init`
does not exist and the gates aborted with "goose_init must be called before
exported functions". They now call `fg950_goose_init`, which is exactly what the
reader owner does when it registers a file.

## Result

| check | accepted | this build |
| --- | --- | --- |
| build exit code | - | **0** |
| `libllama.so.0` bytes | 5 766 688 | **5 993 496** |
| defined dynamic symbols | 6493 | **6584** |
| symbols removed | - | **0** |
| symbols added | - | **91** |
| `tp_apply` / `tns_apply` / `dry_apply` in the DSO | absent | **defined** |

The gates, run against **this DSO**:

| gate | checks | failures |
| --- | --- | --- |
| typical, live sampler | **202** | 0 |
| top-n-sigma, live sampler | **12515** | 0 |
| DRY, live sampler | **2701** | 0 |

So the ports are not only correct in an experiment build, they are correct in the
library that would ship.

## Not claimed

`llama_sampler_chain` still constructs the native filters. Being in the DSO means
the code travels; it does not mean it decides. Replacing a `llama_sampler_i`
implementation is the wiring step, and accepting it needs a generation run to
compare output - Igor's approval.

P6-10 stays PARTIAL: two of the four listed samplers (XTC, mirostat v1/v2) remain
native because Goose has no RNG, and none of the three ported ones is wired.

No model was started. The gates construct candidate arrays only.

## Reproduction

    python3 runtime/reconstructed/build.py --build-dir OUT \
        --runtime-libs <accepted>
    nm -D OUT/libllama.so.0 | grep -E ' T (tp_apply|tns_apply|dry_apply)'
    cp OUT/goose-obj/tokenizer-unit.h OUT/runtime.h
    g++-13 -O1 -I OUT -Ivendor/flint/include -Ivendor/flint/ggml/include \
        experiments/EXP-2026-11-18-966-goose-typical-live/tests/gate.cpp \
        OUT/libllama.so.0 <accepted>/libggml*.so.0 -o OUT/g966 && OUT/g966
    # the initialiser inside the DSO is fg950_goose_init, not goose_init
