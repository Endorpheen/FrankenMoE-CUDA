# EXP-2026-11-26-2121: Produce Inkling loop messages at CUDA consumers

Build on EXP-2120. Store the bank and normalized previous-pass activation
in the existing attention fallback buffer while the controller graph uses
resident inputs. A guard rejects concurrent fallback staging. Materialize
one message as it is needed by each controller and reuse the output plane.
The bank is copied device to device at the rewind. The previous-pass
activation still uploads from the host once per rewind. Set
I2_DEVICE_LOOP_MESSAGES=0 to disable this path.

| RTX 4070, frozen 32K/512 | Prefill tok/s | Decode tok/s |
| --- | ---: | ---: |
| EXP-2120 checkpoint | 350.727 | 35.729 |
| EXP-2121 candidate | 386.999 | 35.603 |

All 512 token IDs, text and stop status match EXP-2120. The log shows 192
visits with all four messages consumed on CUDA. Peak RSS was 13987068 KiB
(244480 KiB below EXP-2120), process swap at end zero, system swap-out delta
zero, disk reads zero bytes and end board VRAM 11662 MiB. Peak VRAM not
collected. A single observed request improves prefill by 10.34% and
reduces decode by 0.35%. Paired and cross-model acceptance remain open.

The local executable and full evidence are in build/inkling2-checkpoints/2121.
Rebuild with the exact EXP-2120 source tree:

```bash
bash source/build.sh /path/to/2120/source /path/to/hybrid-worktree /path/to/new-build-directory
```

Profile the remaining transfer counts on EXP-2121 and remove previous-pass
activation round trips before closing the residency work.
