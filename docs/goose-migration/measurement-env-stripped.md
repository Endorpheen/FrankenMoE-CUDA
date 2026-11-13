# My A/B Apex measurements were all the same configuration

Found while trying to measure EXP228's hot port. It invalidates a whole class of numbers I reported
today, and the cause is in the harness, not in the component under test.

## What the runner does

`run220.py` / `run227.py` / `run229.py`, lines 17-20:

    env = dict(os.environ)
    for k in list(env):
        if k.startswith('GGML_GOOSE_'): env.pop(k)            # <-- every Goose toggle is removed
    saved_env = json.loads(.../audit-exp150-omp-apex-32k128-environment.json)
    env.update(saved_env)                                    # <-- a frozen EXP150 snapshot is applied

So a toggle exported in my shell **never reaches the server**. The environment the server actually sees
is whatever that EXP150 audit file contains, plus whatever the script sets itself.

## Proof

`environment.json` recorded by the runs holds exactly this set of Goose variables, identical in every
run:

    GGML_GOOSE_ALLOC, COALESCE, LIFETIME, GRAPH_PLAN, MOE, TILED, TILED_GENERAL, ELEMENTWISE,
    MOE_QUANT, MOE_CONTROL, RMS, CPU_BACKEND, CPU_EXECUTOR, CPU_CONTROL, CPU_BACKEND_API,
    CPU_QUANT, CPU_POOL, ALLOC_VERIFY

None of the variables I have been porting against — `KV_CELL_EXT`, `KV_INPUT_IDXS`, `KV_N_KV`,
`CUDA_MMQ_Q8_1`, `TOKENIZER`, `PIECE`, `UNICODE_*` — is present. A run with `=0` exported produced
byte-identical Goose call counts to the run with `=1`:

    FG228E_KV_CELL_EXT calls=65814   in both
    FG229I_KV_INPUT_IDXS calls=292   in both

## What that invalidates

| stage | what it compared | status |
|---|---|---|
| EXP224 | same DSO, env `GGML_GOOSE_CUDA_MMQ_Q8_1` unset vs `1` | **void** — identical configuration |
| EXP225 | same DSO, same toggle | **void** |
| EXP226 | same DSO, same toggle, under the resource lock | **void** |
| EXP228/229 "off vs on" | same DSO, module toggles | **void** |

Every "pair" in those files is a repeat of one configuration. Their deltas, including the 6-9 point
swings, are pure run-to-run noise. **EXP226's verdict "the Goose MMQ route loses in both orders" is
withdrawn**: with the toggle stripped, the MMQ route is opt-in and therefore *off*, so both arms ran
native.

## What stays valid

Different questions, different evidence, none of it touched by this:

- **Correctness**: tokens, text and all 128 saved probabilities equal to `e68df05b` — that is a
  single-configuration check and holds.
- **Liveness**: witness call counts and the superset checks. They fire because every module I ported
  defaults to enabled, which is exactly the project rule.
- **Gates**: EXP227 5011 checks, EXP228 1024, EXP229 25893 — host-side, no harness involved.
- **Instruction counts**: from `nvcc`/`cuobjdump` directly, no server involved.
- **EXP213-220's interleaved pairs**: those compared two different **stage directories**, i.e. two
  different DSOs. The env stripping is irrelevant there because the difference was the binary itself.

## The correct method from here

An A/B on this harness needs **two DSOs**, not two environment values. Concretely: build a variant whose
bridge makes the route opt-in, put it in its own `runtime/expNNN/run`, and compare stage directories the
way EXP213-220 did. The kernel-level harness (`runtime/exp180/build/timing`) is unaffected — it spawns
its own process and reads the variable directly, which is why EXP222/223's `=1`/`=0` results stand.

## What I should have checked first

The label in the log said `arm=goose` for both arms of EXP224 and I did not ask why. Reading the
recorded `environment.json` takes one `json.load` and would have caught this before three experiments
were spent. The project already learned this lesson once — "check what actually ran, not what you
asked for" — and the harness had been faithfully recording the answer all along.
