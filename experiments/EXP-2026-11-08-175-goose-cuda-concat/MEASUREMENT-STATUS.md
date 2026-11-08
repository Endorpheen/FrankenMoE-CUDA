# Measurement status EXP175: unresolved speed discrepancy

The CONCAT transfer itself is committed (`52011a5`) with passing local gates and
byte-exact model output. What is NOT established is the absolute
prefill/decode speed, because the same unchanged binaries now report numbers well
below the recorded EXP174 baseline.

## Facts, not conclusions

| Source | Machine state | Prefill | Decode |
|---|---|---:|---:|
| EXP174 original run (recorded) | load ~1.8-2.3 | 2663.00 | 27.86 |
| EXP175 same-DSO pair, native arm | load 22.6-28.5 | 2150.66 | 18.31 |
| EXP175 same-DSO pair, Goose arm | load 14.7-24.4 | 2210.01 | 19.03 |
| Interleaved A/B, old EXP174 arm #0 | load 18.9 | 2189.82 | 18.70 |
| Interleaved A/B, new EXP175 arm #1 | load 16.1 | 2231.25 | 19.39 |
| Interleaved A/B, new EXP175 arm #2 | load 21.1 | 2251.26 | 17.10 |
| Interleaved A/B, old EXP174 arm #3 | load 25.9 | 2222.58 | 18.86 |
| EXP174 re-run with the original runner | load ~2 | see receipt | see receipt |
| Independent re-run reported by the owner | load 1.7-2.7 | 2467.91 | 25.10 |

The `runtime/exp174/run/libggml-cuda.so` used by the old arms is byte-identical to
the hash recorded at the original 2663/27.86 measurement
(`0b187bc1cca2b4077a076845fcfc099dc68183dbc55b3b3011d765fe18a47117`), and its
modification time predates every re-run. All arms reproduce exactly the saved
128 token ids, response text and all returned top-5 probabilities.

An independent re-run of the same unchanged code elsewhere recovered 25.10 tok/s
rather than 18.7-18.9, so a slow machine is not the whole story and no single
cause is proven.

## What was ruled out by comparison, and what was not

Ruled out as the *explanation of the delta*, since they were identical across
arms:

- server flags: the 56 flags are byte-identical to the audited profile
  (`plan-apex-native-argv.json`); only the extra `--alias` appears, which the
  owner's list omits and which does not affect inference;
- request: 32768 prompt tokens, 128 predictions, seed 123, temperature 1,
  top_p 0.95, top_k 20, min_p 0, n_probs 5, cache_prompt false, ignore_eos true;
- `LD_PRELOAD` empty in both, `GGML_EXPERT_HOTSTORE=0`, same model file.

Not established:

- whether my earlier sampling harness perturbed the measurement. It called
  `nvidia-smi` four times per second (about 80 process spawns per request), while
  the original runner samples once every five seconds. That difference is a
  plausible self-inflicted perturbation and is why the original runner is now
  used unchanged;
- CPU-side facts during decode: per-thread affinity, per-core frequency,
  utilisation and steal time were not captured in the earlier runs. The owner
  notes that mask `0f0f` addresses eight distinct physical cores on this machine,
  that the Apex process had zero swap of its own, and that `powersave` governor
  with `amd-pstate-epp`, `balance_performance` and boost enabled is not by itself
  evidence of a forced low frequency;
- GPU idle clocks say nothing about inference speed and are not used as evidence.

## Owner corrections recorded

1. Mask `0f0f` spans eight distinct physical cores here, not four.
2. The Apex process itself had zero swap; system-wide swap occupancy does not
   prove the cause.
3. `powersave` with amd-pstate-epp and boost is not proof of a forced low clock.
4. GPU idle frequencies are irrelevant to inference speed.
5. The independent re-run had load about 1.7-2.7 and still did not return to
   27.86.

## Next measurement, when needed

Use `run_apex_recheck.py`, which is the original EXP174 runner with only the
output directories changed, so the sampling cadence stays at one `nvidia-smi`
call per five seconds. It additionally records the actual argv, the full
environment, resolved library hashes and the Cpus_allowed_list of every thread,
plus CPU topology, so the next comparison has evidence rather than inference.
Capture per-thread CPU time and core frequency during the decode window before
attributing the discrepancy. Do not change production, system settings or other
processes.
