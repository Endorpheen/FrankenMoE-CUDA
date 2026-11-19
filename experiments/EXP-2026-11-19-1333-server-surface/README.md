# EXP-2026-11-19-1333: the shipped server's HTTP surface answers on every endpoint a client needs

## Result

```
  ok      health     200
  ok      props      200     contains model_path
  ok      models     200     contains Apex-35B-V1.0
  ok      root       200     the UI page
  ok      tokenize   200     returns tokens for a POSTed content
```

Five endpoints, each checked for a status **and** for content that proves the right handler answered.

## Three harness errors before the numbers meant anything

Each produced a red result that said nothing about the server:

1. `/tokenize` was probed with GET; it is a **POST** route, so 404 was my method, not a missing endpoint;
2. `/` answered 415 because the asset handler serves pre-compressed files and refuses a client that does not accept
   gzip - the body literally says `gzip is not supported by this browser`. The header was missing from my request;
3. the fix for (2) used `${5:+-H "$5"}` inside an unquoted expansion, which does not pass two words as a header and
   silently sent nothing. Rewriting `check` with a bash array made the probe do what it says.

The third is the same family as the pipeline-`$?` mistake earlier in this report: shell text that looks like it does
one thing and does another, discovered only because the number stayed wrong.

## What this establishes

The request surface a deployment depends on works in the shipped build: health, properties, model listing, the UI and
tokenization. It says nothing about throughput - the context here is 8 K and the profile is not the baseline's - and
nothing about the Goose ports, which are not on these handlers.

## Why it is worth having

P8-09 is about serving, and until now the only server behaviour verified end to end was `/completion` through the
benchmark harness. A surface check is cheap, and a client integration failure would have looked identical to a model
regression from the benchmark's side.
