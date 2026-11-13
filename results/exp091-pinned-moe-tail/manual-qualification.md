# Manual EXP091 qualification (do not execute automatically)

Run each launch in a fresh process; wait for ready, then send the two commands below one at a time. Stop that server normally before the other arm. The second command repeats exactly the same input. Client times and SSE are saved; no output budget, EOS or sampler change.

## Control requests

```bash
curl --silent --show-error --fail-with-body --no-buffer --max-time 120 --trace-time --trace-ascii /home/end0/AI/FrankenMoE-CUDA/results/archive/EXP-2026-10-30-091/raw/control-cold.trace --output /home/end0/AI/FrankenMoE-CUDA/results/archive/EXP-2026-10-30-091/raw/control-cold.sse -H 'Content-Type: application/json' --data-binary @/home/end0/AI/FrankenMoE-CUDA/results/archive/EXP-2026-10-30-089/raw/cold-request.json http://127.0.0.1:8081/completion
```

```bash
curl --silent --show-error --fail-with-body --no-buffer --max-time 120 --trace-time --trace-ascii /home/end0/AI/FrankenMoE-CUDA/results/archive/EXP-2026-10-30-091/raw/control-repeat.trace --output /home/end0/AI/FrankenMoE-CUDA/results/archive/EXP-2026-10-30-091/raw/control-repeat.sse -H 'Content-Type: application/json' --data-binary @/home/end0/AI/FrankenMoE-CUDA/results/archive/EXP-2026-10-30-089/raw/cold-request.json http://127.0.0.1:8081/completion
```


## Candidate requests

```bash
curl --silent --show-error --fail-with-body --no-buffer --max-time 120 --trace-time --trace-ascii /home/end0/AI/FrankenMoE-CUDA/results/archive/EXP-2026-10-30-091/raw/candidate-cold.trace --output /home/end0/AI/FrankenMoE-CUDA/results/archive/EXP-2026-10-30-091/raw/candidate-cold.sse -H 'Content-Type: application/json' --data-binary @/home/end0/AI/FrankenMoE-CUDA/results/archive/EXP-2026-10-30-089/raw/cold-request.json http://127.0.0.1:8081/completion
```

```bash
curl --silent --show-error --fail-with-body --no-buffer --max-time 120 --trace-time --trace-ascii /home/end0/AI/FrankenMoE-CUDA/results/archive/EXP-2026-10-30-091/raw/candidate-repeat.trace --output /home/end0/AI/FrankenMoE-CUDA/results/archive/EXP-2026-10-30-091/raw/candidate-repeat.sse -H 'Content-Type: application/json' --data-binary @/home/end0/AI/FrankenMoE-CUDA/results/archive/EXP-2026-10-30-089/raw/cold-request.json http://127.0.0.1:8081/completion
```

Require cold cache0/prompt32768 and repeat cache32764/prompt4, exact128 selected/ordered-top5 IDs and probability delta<=0.001 across the two arms. Stop on a mismatch; a timing alone does not qualify the candidate. Do not regard this one pair as five-pair acceptance.
