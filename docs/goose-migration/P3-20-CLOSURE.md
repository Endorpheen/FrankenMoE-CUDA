# P3-20 closure: selected CUDA link and the restored generated object

Criterion: "Zero owned hand-written CUDA/C++ computation/orchestration in the
selected link; generated CUDA/C from Goose allowed and provenance checked."

## Selected link

The selected runtime is `runtime/reconstructed/build/stage-run`. Its CUDA
library is a self-contained `libggml-cuda.so.0.27.0` built from the flint
source set plus the Goose-generated integration object. Audit by the mechanical
`gg_function_N` marker:

| class | count | ownership |
|---|---:|---|
| upstream flint `.cu.o` kernels | 144 | not authored by this project |
| Goose-generated CUDA integration object | 1 | generated from `shared-cuda.cu` + the Goose service |
| project-authored hand-written CUDA computation/orchestration | **0** | none in the selected link |

The generated object was missing from the historical EXP160 build directory.
It has been rebuilt with the recorded nvcc command:

- `runtime/exp160/build/shared-cuda.o`
- size 505 488 bytes
- SHA-256 `3514aacb39497a0445c4bb62fbd1ea9d9a37f3781ff9edc129bb326d1fdab194`

That removes the "object absent, a relink would fail" integrity note for the
object itself.

## Historical relink boundary

The saved `shared-cuda-link` command still names an archived
`libggml-base-shared-70f7b64fec7d.so.0.27.0` from `runtime/exp160/run`, which is
not present. That is a historical artifact of the EXP160 staging, not the
selected runtime: `stage-run` is self-contained and its CUDA library loads and
runs the Apex baseline (41.62 tok/s, 32K/128, cache 3584). Reconstructing the
missing base DSO is archive work, not a source-ownership gap.

## Definition

The owner decision recorded for P2-16 applies here: **owned means authored by
this project**. Upstream flint kernels shipped unchanged are not owned. The
project's own CUDA surface is the Goose-generated integration object; the
remaining C++ in the model/server links is ABI glue, not CUDA computation or
dispatch policy.

Under the opposite reading ("owned = shipped in the candidate"), the 144
upstream kernels would be native executable logic and the clause could not
hold without porting kernels the compiler does not emit. That reading is
recorded as not selected.

## Verdict

P3-20 is closed under the selected definition. The generated object exists with
a recorded hash, the selected CUDA link contains no project-authored
hand-written CUDA, and the only open item is an archived base DSO from an older
staging directory, explicitly outside the selected runtime.
