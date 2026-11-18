# EXP-2026-11-18-967/968: the samplers checked against the live library, and a port bug found

Status: two live differential gates. One found a real bug in the ported DRY filter.

## Hypothesis fixed before changes

EXP959-961 gated each sampler against a **transcription** of the native function.
That answers "did I read the source correctly twice", not "does the port agree
with the library". EXP966 proved the difference for typical (202 checks through
the real `llama_sampler`). This extends that method to top-n-sigma and DRY.

## 967: top-n-sigma, 12515 checks

`llama_sampler_init_top_n_sigma` + `llama_sampler_apply` on 300 seeded cases
(2..48 candidates, sigma 0.25..3.75). Two comparisons per entry:

- **the mask decision** per id, native `-INFINITY` against the module's most
  negative finite float - the two sentinels differ by construction, because Goose
  has no infinity literal;
- **bit equality** for every entry neither side masked.

Result: **12515 checks, 0 failures**. The sentinel difference is real but never
changes a decision, and the gate says so per entry rather than asserting
equivalence in prose.

## 968: DRY, and two real findings

`llama_sampler_init_dry_testing` takes token-id breakers and needs no vocab, so the
live path was reachable without a model: construct, `llama_sampler_accept` the
history, `llama_sampler_apply`, compare every logit bit for bit.

### Finding 1 - a bug in the port (fixed)

The gate failed immediately with a wrong penalty and then a segfault. The wrong
penalty was the real one:

    history 1 2 3 1 2 3 1 2 3, penalty on id 1
    native: id 1 -> -4e11   (34 tokens of repeat believed)
    module: id 3 -> -4e11   (wrong token entirely)

Cause: `ring_buffer::rat(i)` answers **"i tokens back from the newest"**
(`data[(first + size - i - 1) % capacity]`), while `dry_rat` indexed forward from
the oldest. Every repeat was attributed to the token at the wrong end of the
history. Fixed by reversing the mapping in `dry_rat`.

**The old gate did not catch it, and could not have:** EXP961's oracle contained
the same forward indexing, so the port and its oracle agreed on a wrong answer for
the whole 2440-check corpus. Two copies of one mistake are not a differential
test. After the fix, EXP961's oracle was corrected too, and both gates pass.

### Finding 2 - a defect in the native testing helper (not ours)

`llama_sampler_init_dry_testing` segfaults when `dry_multiplier == 0`, with or
without breakers:

    llama_sampler * s = llama_sampler_init_dry_testing(0.0f, 1.75f, 2, 64, {});
    -> SIGSEGV in the library, before any of our code runs

`llama_sampler_init_dry` returns `llama_sampler_init_empty(...)` at multiplier 0,
and the testing wrapper then writes breakers into `(llama_sampler_dry *) result->ctx`,
which the empty sampler does not have. The gate therefore covers the empty case
through `dry_apply` alone and records why. The library was not modified.

## Result

| gate | checks | failures |
| --- | --- | --- |
| 967 top-n-sigma, live | **12515** | 0 |
| 968 DRY, live | **2701** | 0 |
| 961 DRY, corrected oracle | **2440** | 0 |
| 959 typical, after the DRY fix | unaffected | 0 |
| 966 typical, live | 202 | 0 |

## Not claimed

Nothing is wired into the shipping chain: replacing a `llama_sampler_i`
implementation needs a generation run to accept, which needs Igor's approval. So
P6-10 stays PARTIAL.

The DRY bug means the previous claim "ported and gated" was too strong for that
filter: it was gated, but against a copy of itself. The correction is recorded in
P6-10 rather than quietly overwritten.

No model was started; no GPU work beyond the library's own init.

## Reproduction

    # 967
    g++-13 -O1 -I OUT -Ivendor/flint/include -Ivendor/flint/ggml/include \
        experiments/EXP-2026-11-18-967-goose-top-n-sigma-live/tests/gate.cpp \
        OUT/libgoose.so <accepted>/libllama.so.0 ... -o OUT/gate && OUT/gate
    # 968
    g++-13 -O1 -I OUT -Ivendor/flint/include -Ivendor/flint/ggml/include \
        experiments/EXP-2026-11-18-968-goose-dry-live/tests/gate.cpp \
        OUT/libgoose.so <accepted>/libllama.so.0 ... -o OUT/gate && OUT/gate
    # the native defect, isolated:
    llama_sampler_init_dry_testing(0.0f, 1.75f, 2, 64, {})   # SIGSEGV
