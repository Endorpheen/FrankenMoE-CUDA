# EXP-2026-11-18-999: The shipped bridge and the canonical unit agree

Status: proven equal, and the measurement that matters turned out to be a different
one - **the shipped library contains none of the unit's functions.** 38 308 checks.

## The question I meant to ask

EXP998 proved `sm_select` equals the native sampler chain. But the shipped library calls
`fg386s_pick`, a frozen bridge built from `experiments/EXP-2026-11-11-386`, not the
canonical unit. So the honest question was: do the two agree?

They do. With the same seed and the same draw reproduced through
`std::generate_canonical<double, 53>`:

| differential | result |
| --- | --- |
| index chosen, 4 seeds | identical |
| probabilities written, 4 seeds | max difference < 1e-6 |
| bridge enabled by default | yes (rule 10) |

## The question I should have asked first

Looking for the bridge, I measured what is actually in the shipped `libllama.so.0`:

    $ for s in hl_open rs_status_line tz_source btl_load bt_bpe tp_apply tns_apply dry_apply gr_parse
      do nm -D libllama.so.0 | grep -c " T $s"; done
    0 0 0 0 0 0 0 0 0

    $ grep -cE " T fg[0-9]+" libllama.so.0
    336

**Zero of the canonical unit's exports. 336 frozen experiment bridges.** The library was
built 2026-11-16 23:09; every module written on 2026-11-18 is absent from it.

The cause is structural, not an oversight: `runtime/reconstructed/build.py` compiles the
`_GOOSE_PAIRS` list as **per-module bridges from frozen experiment directories**, and it
composes from current `.goose` sources only the `_CARRIER_UNIT` - the tokenizer path and
three sampler filters. Adding a module to `tools/build-main-runtime.sh` makes it visible
to the gates and to the standalone runtime; it does not make it visible to the shipped
artifact.

That distinction was never measured before, so "145 modules in the unit" could be read as
progress on the artifact when it is a fact about a directory. The report now says it:

    unit exports in shipped lib  0 of the header's functions (the shipped
                                 artifact is built from per-module bridges, not from the unit)

## Why this changes the next step

EXP998 ended with "the next step is wiring the proven chain into the shipping path". That
step is not a matter of calling `sm_select` from `llama-sampler.cpp`: the object that
would provide `sm_select` is not in the link line at all. `_CARRIER_UNIT` is the only
place that compiles from current sources, and it holds 14 modules. Extending it is the
real next step, and it needs a decision I should not take alone:

- the shipped library already provides these decisions through 336 bridges, several of
  which may now be older versions of what the unit holds;
- replacing a bridge with the unit's export changes what Apex runs, so it needs the full
  suite plus a measured A/B, which is a run and needs approval (rule 1);
- two of the pairs disagree in shape: `fg386s_pick` takes raw pointers and a `double`,
  `sm_select` takes slices and draws from an `i64[:]` state. The bridge is what makes the
  call possible today, and every bridge is debt by rule 17.

## Result

| check | result |
| --- | --- |
| shipped-vs-unit | 6 checks, 0 failures |
| whole suite | **38 308**, 0 failures (12 gates) |
| unit exports in the shipped library | **0** - now printed by the report |

## Not claimed

Agreement on one configuration is not agreement on the chain: the bridge does softmax and
cut, `sm_select` additionally carries top_k, top_p, min_p and temperature, and the gate
compares the reduced path. The filters are compared against the native samplers by
EXP959-961 and 998, not against the bridge.

No bridge was replaced, no library was rebuilt. The 0-of-N measurement is a fact about
the artifact as it exists, not a claim about what a rebuild would produce.

No model was started, no benchmark was run.
