# EXP239: batch entry classification in Goose — ACCEPTED

The block at the top of `llama_batch_allocr::init` (llama-batch.cpp:57-98) that classifies each batch entry
and decides whether the batch is legal at all. It runs once per batch, before anything else.

## What it preserves

    for each entry: is_tok = id != NULL; is_emb = has_embd
        if (!is_tok && !is_emb)              -> error 1, IMMEDIATELY
        n_tok_only += is_tok && !is_emb
        n_embd_only += is_emb && !is_tok
        n_both     += is_tok &&  is_emb
    if (n_both > 0 && n_both != n_tok)        -> error 2
    mixed = n_tok_only > 0 && n_embd_only > 0
    if (mixed && !allow_mixed)               -> error 3
    has_token = n_tok_only > 0 || n_both > 0
    has_embd  = n_embd_only > 0 || n_both > 0

The counting is trivial. **The order of the checks is not.** Native returns on the first "neither" entry
*before* reaching the both-entries test, so a batch containing both a junk entry and a mixed token+embd
entry fails with reason 1, not 2. Reporting the other code points an error message at the wrong cause.

## Gate

    67640 checks, 0 failures

8 named cases plus 20000 random. The named ones pin each error code, and one specifically constructs the
precedence case — a "neither" entry **before** a both-plus-token pattern — which must return 1, not 2.

Mutation that caught it: swapping the both-entries check ahead of the neither check — **2819 failures**.

A first mutation attempt did *not* fail, and that is worth recording: I replaced the immediate return with
"set err, keep scanning", which is semantically identical because the `err` test is hoisted above the later
checks. An equivalent mutation passing is not a weak gate; it is a mutation that did not change behaviour.

## Live

    FG239B_BATCH_CLASSIFY calls=146
    prefill 2484.4  decode 24.64
    tokens True  probabilities True  sha e68df05b

146 calls, one per batch. Witness superset vs `exp238`: no missing, `new FG239B`. All three batch routes
verified present in the object with `nm -u` before linking — `fg237s_ fg238q_ fg239b_` — which is the check
that caught my own EXP238 regression last stage.

## Performance

Not measured as a pair. **Прогон выполнен при параллельной нагрузке. Показатели производительности могут
быть искажены.** 2484.4 / 24.64, in the usual band. 146 calls over `n_tok` entries each, so the boundary is
amortised; that is reasoning, not a measurement.
