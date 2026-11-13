# EXP-086 artifacts

The batch6144 candidate was rejected at context initialization. There is no candidate response or speedup result.

- `qualification.json`: one control request, resource snapshots and the repeated candidate allocation failure.
- `runtime-integrity.json`: unchanged EXP-083 runtime/library hashes.
- `summarize_pairs.py`: Apex's comparator for native llama.cpp `/completion` SSE with 32768 evaluated prompt tokens, 128 generated tokens and top-5 records.
- `paired_stats.py` and `test_paired_stats.py`: Apex's generic paired-time statistics and regression cases.
- `helper-review.json`: independent acceptance and the exact-command issue found by Apex.
- `raw-evidence-manifest.json`: sizes/hashes of retained private raw evidence; raw contents and helper conversations are not included.

Run the statistics tests from the repository root:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 -B -m unittest discover -s results/exp086-batch6144 -p test_paired_stats.py -v
```

Do not append a period as a command argument. No benchmark runner or production engine opt-in was introduced.
