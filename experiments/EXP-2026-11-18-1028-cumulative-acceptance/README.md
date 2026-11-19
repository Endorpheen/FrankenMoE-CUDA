# EXP-2026-11-18-1028 — cumulative baseline acceptance (32K/512 vs EXP906)

Cumulative build: build/candidate-run2 (P1-12 Goose Q4_K tiled + P4-13 Goose allocator).
Reference: EXP906 (apex-independent-fast-20261116/run, untouched).
Profile: -c 262144 -b 4096 -ub 4096 -ngl all -fit off --cpu-moe --moe-cache-mib 4992; request /tmp/req512.json (32768 in, 512 out).

| pair | sha ref/cum | pred | ref prefill/decode | cum prefill/decode | ref VRAM/RAM | cum VRAM/RAM |
|---|---|---|---|---|---|---|
| 1 | c2ff42cc1219 / c2ff42cc1219 | 512/512 | 2655.5 / 53.83 | 2666.5 / 54.18 | 11469 / 46482716 | 11402 / 45968260 |
| 2 | c2ff42cc1219 / c2ff42cc1219 | 512/512 | 2566.3 / 49.74 | 2589.5 / 52.80 | 11405 / 45953600 | 11271 / 45878140 |
| 3 | c2ff42cc1219 / c2ff42cc1219 | 512/512 | 2659.6 / 53.88 | 2691.6 / 53.43 | 11293 / 45763416 | 11132 / 46121576 |

Medians: prefill 2655.50 -> 2666.53 (+0.4%), decode 53.83 -> 53.43 (-0.7%), VRAM 11405 -> 11271 MiB (-1.2%), RAM unchanged.
Token and answer correctness: identical sha in all six runs.
Historical references at this profile: decode ~55.52, prefill ~2625 — the cumulative build is at 53.43/2666.53 in this session.

Presence of previously accepted components: tools/check-cumulative-baseline.py -> CUMULATIVE-BASELINE PASS.
ok P1-12 Goose Q4_K tiled kernel dispatch
ok P1-12 Goose kernel reference
ok P4-13 Goose allocator primitive
ok Goose kernel object present
CUMULATIVE-BASELINE PASS
