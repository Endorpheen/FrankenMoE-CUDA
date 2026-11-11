# Preserved checkpoints (survive /tmp cleanup)

| checkpoint | what it holds | verification |
|---|---|---|
| `exp186-cuda-matvec-v61-20261111` | integrated CUDA backend (Q6_K matvec, FG196P policy, FG195V pool arithmetic, FG180Q MMQ quantizer), its modified sources, the v61-generated host/device C for both Q6_K modules, extracted policy blocks and the harness | accepted profile `e68df05b`, equal probabilities, restore ok, 636 Q6_K sub-calls, FG180Q paths=0x3, FG196P host_plan=2 |
| `exp412-cpu-q6k-v61-20261111` | CPU backend built from flint (native) and its Goose variant, matching base library, link recipe | gate 240/240 bit-exact, CPU bench run identical (`d13cf4cf`) |

Every directory has `manifest.json` with a SHA256 and size per file. Artifact `RUNPATH`s were rewritten from `/tmp/...`
to `$ORIGIN/` and their base libraries copied alongside, so `ldd` reports no `/tmp` entry and the checkpoints do not
depend on a directory that can be wiped.

Rebuilding from source remains possible via `experiments/EXP-2026-11-08-186-goose-cuda-mmvq-q6k/build_integrated.sh`.
Large `.so` files are stored in these directories (git ignores them); their hashes are in the manifests.
