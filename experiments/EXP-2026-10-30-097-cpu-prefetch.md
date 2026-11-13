# EXP-2026-10-30-097: bounded CPU expert prefetch

Status: REJECTED (`kind=real-model-qualification`). Control EXP090; independent unchanged DeepSeek patch prepared before application. Reference project f2f4eef5cab408b30aa2312c5fec897ee16d1410. New>=5% goal remains active.

Hypothesis: bounded lookahead for Q4_K/Q6_K MoE CPU rows hides some memory latency without changing arithmetic. Distinct from prior OS madvise and IQ2_S kernel changes. Primary: repeat single-response decode>=5%, numerical identity and clean request resources. No gain estimate is treated as measured. Build separately, retain other eight EXP090 components, screen real Apex cold32768/repeat up to128 outputs with fixed profile/seed123/top5/EOS; stop on failed qualification rather than tune or run formal pairs after failure.

One candidate startup/two requests, reusing the fresh EXP096 control to avoid duplicates. Selected IDs/ordered top5/probabilities exact for all256 tokens per arm, delta0. Candidate cold decode29.34245 vs29.41098tok/s control; repeat26.57359 vs28.50864tok/s. Candidate process swap/major faults0, cold swap-out0, repeat10 pages; reused control cold152 swap-out pages. Contaminated windows are not clean speed evidence, and a causal slowdown is not established. VRAM4428MiB, no observed saving. No>=5% gain accepted; reject current qualification, skip formal same-binary pairs and additional gates.

EXP090 restored and verified (health, nine mapped hashes,120 registered expert ranges). Artifacts and limitations in results/exp097-cpu-prefetch/. The emergency disk cleanup occurred after measurement and candidate shutdown. No default or working runtime changed.
