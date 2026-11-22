# EXP-1701 — Initialize sampler order in an ownership-free loop

EXP-1700 measures 1.158 s between model evaluator calls on the
default Apex profile. This change extracts the full-vocabulary
identity-index initialization from the mixed ownership context of
the sampler into a scalar, borrowed-slice helper. The order, bounds,
sampler math and RNG are unchanged. In the generated host C, the
new 472-byte helper body has one bounds-checked index and zero
ownership scope calls. The former loop was in the larger scoped
sampling function. Only `main.o` differs by archive-member hashes.

Both ordinary pairs use Apex 32K/512 with default API completion,
no probability rows, 4,096/4,096 batches, 6,290 MiB GPU SLRU and
full host pin. Each arm warms one request in the same process.
The second full request has no prompt reuse.

| Run | Prefill tok/s | Decode tok/s | Decode s | Client s |
|---|---:|---:|---:|---:|
| Forward control | 2,998.26 | 39.84 | 12.827 | 23.846 |
| Forward helper | 3,021.78 | 40.84 | 12.514 | 23.446 |
| Reverse control | 2,991.09 | 39.50 | 12.937 | 23.986 |
| Reverse helper | 3,035.23 | 41.06 | 12.447 | 23.331 |

All 512 token IDs and response bytes match on both cold and
warmed requests. All SLRU counters match between arms. Peak
process VRAM 8,766 MiB and process swap zero throughout.
Both reverse arms have zero system swap-out from launch.
Their peak RSS is 21,488,648 and 21,489,268 KiB respectively.
The forward control had 493 pages of system swap-out during
its lifecycle. The forward candidate had zero.

Accept the helper's clean 3,035.23 / 41.06 sample as the new
default-API checkpoint. The previous checkpoint is preserved.
This result does not claim the 71.17 tok/s decode target.

The executable, raw pairs, generated source and original
request/options with checksums are in the worktree under
`build/exp1701-sampler-order`. Binary SHA-256:
`ee78155c23ad25fdb17e54727c531e7a4a24d82ccd9548228fe41a9faf454209`.
Apply the patch to the EXP-1695 composed host unit and replace
its `main.o` using the same compiler and link flags. The CUDA
images and all other archive members remain byte-identical.
