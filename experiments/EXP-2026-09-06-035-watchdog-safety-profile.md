# EXP-2026-09-06-035 — Watchdog safety profile (R1)

Status: ACCEPTED (kind=correctness), committed in `7e8b819`. Static audit + synthetic tests + lifecycle + model smoke (single request, senior helper in Codex) all done. This experiment is closed and must not be repeated.

## Hypothesis (one)

The external launcher monitor and the internal server watchdog can have identical units/thresholds,
a correct lifecycle, and be explicitly enabled in a future benchmark profile, so both performance
arms share one working safety profile.

## Baseline

- Launcher: `scripts/run_qwen38_server.sh`; the verified fix is committed in `7e8b819`.
- Internal watchdog: `work/llama.cpp-integration/tools/server/server-swapwatchdog.{h,cpp}`, started
  from `tools/server/server.cpp` only when `params.swap_watchdog` is set (`common/arg.cpp`
  `--swap-watchdog`, default off, env `LLAMA_ARG_SWAP_WATCHDOG`).
- Static/synthetic stage ran with no model: all tests used fake `llama-server`/`nvidia-smi` in
  `/tmp/exp035-watchdog`; the live 8081 server (`-c 196608 -np 1`) was not touched. The model smoke
  came later in a separately approved block (senior helper in Codex); its server was shut down
  cleanly after exactly one request.

## Static findings (source audit)

1. Units bug (external hard threshold). `sys_rate` in the launcher is already MiB/s
   (`(dpswpin+dpswpout)*4/1024`), but the hard check is `sys_rate > 2097152` — that value equals
   2 TiB in pages and is unreachable in MiB/s units. The internal watchdog default hard threshold
   is `2048` MiB/s (`server-swapwatchdog.cpp:28`). Aligned value must be `2048`.
2. VmSwap diagnostic unit bug in the launcher message: `proc_burst` is KiB and the message prints
   `proc_burst*4/1024` — a leftover page factor, overstates the reported MiB by 4x. Correct
   conversion is `/1024`. The internal watchdog prints `burst/1024` (correct). `/proc/vmstat`
   pswpin/pswpout are pages (4 KiB); there the `*4/1024` factor is right. Units kept separate.
3. Threshold parity (intended vs found):
   - proc VmSwap burst: internal 256 MiB/sample (`256*1024` KiB) == external 262144 KiB -> equal.
   - system rate burst: internal `>512` MiB/s for `>=5` consecutive samples == external
     `>512`/`>=5` -> equal; strict `>` in both, exactly the threshold never trips.
   - hard system rate: internal 2048 vs external 2097152 -> BROKEN external (finding 1).
4. `--swap-watchdog` is absent from the launcher, EXTRA_ARGS defaults empty, and neither
   `LLAMA_ARG_SWAP_WATCHDOG` nor the `SWAP_WATCHDOG_*` env test hooks are set by any script/doc:
   the internal watchdog is currently disabled in every documented launch path.
5. No `pkill` anywhere in `scripts/`; both monitors signal only their own recorded PIDs.
6. Dead env doc: launcher header mentions `SWAPS_PER_TOK`, the variable is never used.

## Lifecycle audit (original launcher, no change applied)

- Server exit 0 -> launcher exit 0; exit 7 -> launcher exit 7; monitor stops by itself when the
  server exits (loop condition), no `pkill`, no orphans.
- SIGINT/SIGTERM delivered to the launcher PID only: handler runs, escalates INT->TERM->KILL to the
  server and kills its own monitor; launcher exits with 130/143. Original launcher is correct on
  signals; no launcher lifecycle change needed.
- Test-harness lessons recorded: background jobs in a non-interactive session inherit SIGINT=ignored
  (`SigIgn` bit 1), so signal tests must reset dispositions before exec (`signal-driver2.py`);
  `kill(pid,0)` on own child passes for zombies, use `Popen.wait()`; unique per-run fake-server
  pidfile with readiness wait.

## Synthetic boundary tests (extracted monitor logic, vector-fed vmstat)

`synthetic-rate-boundary.sh` runs the extracted prologue+loop of the current launcher verbatim
(only substitutions: fake vmstat path, faster sleep, iteration-counter sync) and the same with
`2097152 -> 2048`:

| case | original (2097152) | fixed (2048) |
| --- | --- | --- |
| hard-rate 2049 single sample | no-trip (unreachable hard rule) | trip |
| hard-rate 2048 / 2047 single | no-trip / no-trip | no-trip / no-trip |
| burst 5 consecutive >512 | trip | trip |
| burst 4 consecutive >512 | no-trip | no-trip |
| 4x600, normal reset, 4x600 | no-trip | no-trip |
| rate exactly 512 x8 | no-trip | no-trip |

Proc VmSwap burst parity: 255/256 MiB -> no-trip, 257 MiB -> trip, identical in both codebases
(strict `>` against 262144 KiB).

## Accepted diff (applied and committed in `7e8b819`)

`results/archive/EXP-2026-09-06-035/audit/launcher-swap-watchdog.candidate.patch`
(`git apply --check` clean; see `apply-check-results.txt`; 1 file, +6/-2):
hard threshold `2097152 -> 2048`; VmSwap message `proc_burst*4/1024 -> proc_burst/1024`; opt-in
`SWAP_WATCHDOG=1` launcher switch appending `--swap-watchdog`. The fix is now present in the working
`scripts/run_qwen38_server.sh` (default profile unchanged: without `SWAP_WATCHDOG=1` the flag is
absent). The internal C++ watchdog is not modified (no proven defect; only the launcher constant
and message were wrong).

## Gates

- Threshold/unit findings reproduced by tests, not just read from source: DONE.
- Lifecycle cases (exit 0 / nonzero / INT / TERM) leave no orphan and preserve the server exit code:
  DONE — original launcher 4/4 PASS, fixed launcher 6/6 PASS (`lifecycle-results.txt`);
  no live test process-group members left.
- Synthetic boundary suite: original 7/7 PASS, fixed 7/7 PASS (`synthetic-results.txt`).
- Candidate diff applies cleanly and the working launcher contains it; default profile behavior
  unchanged (no `--swap-watchdog` without `SWAP_WATCHDOG=1`): DONE.
- Model smoke (activation visible, no false trip): DONE by the senior helper in Codex in a separate
  approved block: one server, one `/v1/chat/completions` request, clean shutdown. Activation line
  `swap watchdog: monitoring VmSwap and system swap I/O` present; load ~9 s; request 48+35 tokens,
  total 6305.08 ms; MTP acceptance 24/24 (1.00000); no CUDA error/OOM/watchdog trip. Compact result:
  `results/archive/EXP-2026-09-06-035/audit/model-smoke-results.txt`; raw log LOCAL_ONLY
  `/tmp/exp035-model-smoke-server.log` (sha256 `c1bd4bb2c074f29f10d30afc79e28dcb5568f3d14a595debf5da49603261d353`).
  Limitation: formal VmSwap=0 not reached — ~150 MiB swap predated the request and decreased by
  9 108 KiB during it (no growth, no false trip). Acceptable for the functional gate; NOT a
  performance baseline.

## Verdict

- Проверено: статический аудит + синтетические границы порога/берста (orig 7/7, fixed 7/7) +
  lifecycle (оригинал 4/4, исправленный лаунчер 6/6) + модельный smoke на живой модели
  (один запрос, активация watchdog видна, ложных срабатываний нет).
- Изменено: `scripts/run_qwen38_server.sh` (исправленный hard-порог 2048, верное KiB->MiB в
  сообщении, opt-in `SWAP_WATCHDOG=1`), карточка, EXPERIMENTS.md, ROADMAP.md, артефакты в
  `results/archive/EXP-2026-09-06-035/`. Внутренний C++ watchdog не менялся.
- До → после: функциональный гейт, не производительность. В smoke VmRSS вырос с 5 608 140 KiB до
  23 391 028 KiB (загрузка модели+KV); VmSwap 153 608 -> 144 500 KiB (был до запроса, сократился,
  роста не было).
- Вывод: внешний hard-pорог был нерабочий (единицы), сообщение VmSwap завышало в 4 раза,
  внутренний watchdog нигде не включался — все три исправлены и доказаны; lifecycle корректен;
  включённый watchdog на живой модели активен и не срабатывает ложно.
- Ограничение: формальный VmSwap=0 не достигнут (~150 MiB swap существовали до запроса). Допустимо
  для functional gate R1; эти данные НЕ являются performance baseline.
- Решение: ACCEPTED (kind=correctness), закоммичен в `7e8b819`; R1 закрыт.
- Далее: R2/EXP-036 отменён как `NOT_RUN_DUPLICATE`; следующий новый эксперимент — R3/EXP-037,
  offline-анализ сохранённых traces.

## Artifacts

- This card; `results/archive/EXP-2026-09-06-035/audit/` (candidate patch, `apply-check-results.txt`,
  `synthetic-results.txt`, `lifecycle-results.txt`, `model-smoke-results.txt`),
  `results/archive/EXP-2026-09-06-035/tests/` (`run_synthetic_boundary_suite.sh`,
  `run_lifecycle_suite.sh`).
- Tests ran in `/tmp/exp035-watchdog/`; raw model smoke log LOCAL_ONLY at
  `/tmp/exp035-model-smoke-server.log`; `work/llama.cpp-integration` untouched; no build touched.
