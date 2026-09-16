# INVALID run: swapped environment

This batch is not usable as evidence.

Every run in it registered non-zero process swap while it ran, including the ring-only arms:

| run | max process swap (KiB) | max RSS (GiB) |
| --- | ---: | ---: |
| A1 | 456960 | 45.4 |
| A2 | 1659648 | 45.2 |
| A3 | 823040 | 42.9 |
| A4 | 873984 | 43.3 |
| B1 | 752128 | 43.5 |
| B2 | 675584 | 44.7 |
| B3 | 2023680 | 42.5 |
| B4 | 805376 | 39.6 |

One async run (B2) also stopped answering in the middle of the session (requests 10 to 12 returned
no response) and the process was gone before it could be interrupted.

Root cause is environmental, not the candidate. At the time of the batch the machine had 24.9 GiB of
system swap already in use, held by desktop applications (gnome-shell 5.35 GiB, Telegram 1.89 GiB,
several opencode sessions, ghostty, chrome), leaving about 44 GiB of MemAvailable while a server run
needs about 45 GiB of resident memory before any pinning. The kernel therefore evicted server pages,
which both violates the project's swap-zero requirement and contaminates every latency in the batch.

The pinning configuration itself was not the cause: the ring-only arms, which pin nothing, swapped
too. Re-measure only after memory is freed (a reboot is the dependable way) and gate every run on
process swap staying at zero.
