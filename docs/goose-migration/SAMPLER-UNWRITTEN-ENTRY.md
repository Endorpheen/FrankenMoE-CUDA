# The invalid token is an unwritten entry, not a wrong id

## The evidence, from the run that failed

```
FG_SERVER_INVALID_TOKEN tok=9359756 n_vocab=248320 n_prompt=21329 n_gen=3383 count=1
```

Three things the number itself says:

- it is **above** the vocabulary, not negative: an id that no token has;
- `0x008ED18C` splits into `142` and `53644`, both valid ids, and as a float it is the denormal `1.3e-38` - the shape of
  memory that was never written rather than a token chosen wrongly;
- it appeared after **3383 generated tokens** on a 21 329-token prompt that had already been reused across tasks, so it is
  not a first-token or a load-time condition.

## Where an unwritten entry can exist

`common_sampler::set_logits` fills the candidate array from one of two places:

| source | size of the array | where the ids come from |
|---|---|---|
| device candidates (`llama_get_sampled_probs_ith != nullptr`) | `llama_get_sampled_probs_count_ith` | `llama_get_sampled_candidates_ith` |
| full logits | `n_vocab` | the loop counter, which cannot be wrong |

The full-logits path cannot hold an unwritten id: every entry is written from the counter. The device path can, if the
count reported is larger than the number of candidates actually written - and nothing checked that before
EXP-2026-11-19-1437. The array is then handed to the chain, our ported top_p sorts it **by logit**, and a denormal entry
whose bits read as a large integer sorts near the front: `kept=1` in the log is exactly that.

## What now happens instead

- every id in the device set is checked against the vocabulary; one bad id takes the full-logits path for that step
  (`FG_CANDIDATES_INVALID`);
- the ordinary sampling path prints which array it used and the selected entry when an id is still invalid
  (`FG_COMMON_SAMPLE_INVALID ... from_device=`);
- the server ends the generation with the model's EOS instead of aborting the process.

## What is still unknown, and the exact data that decides it

Whether the device count over-reports or the chain corrupts an entry afterwards. The next run answers it with one line: if
`FG_CANDIDATES_INVALID` appears, the device set is the source and the fallback already handled it; if
`FG_COMMON_SAMPLE_INVALID from_device=0` appears, the full-logits array is intact and the corruption is inside the chain.

## Why the previous two explanations were withdrawn

The first blamed the ported top_p door for overwriting logits; a six-element CPU probe showed the door is correct
(EXP-2026-11-19-1436). The second blamed the client's long prompt; the failing token came three thousand tokens into the
generation. Both were mechanisms that fitted one observation and did not survive the next one.
