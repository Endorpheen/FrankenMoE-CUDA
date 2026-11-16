# EXP-2026-10-31-121: measured physical VRAM balance

Status: DIAGNOSTIC (no optimization). Reference EXP101, Apex profile.

Idle 1600 MiB; after 4-token request 1614; after cold32768 3188; retained 3188.
Logical: CUDA0 model 1302.90, CUDA_Host output 0.95, KV logical 1440.00,
recurrent RS 62.81, compute reserve 5.16, CUDA_Host compute 3.56 MiB. At 32K
only ~190 MiB of KV is physically mapped.

The ~1588 MiB cold32K growth is ~190 MiB KV plus ~1370 MiB compute/mask/graph/
staging capacity that stays resident. The idle ~227 MiB is CUDA context/
modules/pool. KV release (EXP122) can only return the KV high-water; the 32K
peak is dominated by compute/mask/graph lifetime (EXP124/126) and responds to
ubatch size (EXP125). Artifacts: results/opencode-vram/EXP121/.
