# EXP-2026-09-09-046 — fused IQ2_S при MTP verify (decode-кандидат)

Статус: `PLANNED` — план без запуска модели; исполнение только после approval Игоря (правило 1).
Кинд: `kind=performance` (будущий A/B), основная метрика — decode tok/s.

## Гипотеза

При MTP verification target-модель обрабатывает verify-батч из 1+1..2 токенов (sampled + draft,
обычно 3 строки). В CPU `MUL_MAT_ID` строки группируются по выбранному эксперту (`cne1` — число
строк одного эксперта в вызове). Если на одного IQ2_S-эксперта приходятся 2–3 строки, распаковку
его весов можно сделать один раз для всех строк — экономия на dequant-cost этих групп сокращает
время target verification и повышает устойчивую decode tok/s. Prefill не является целью.

## Новизна (почему это не повтор EXP-028)

- EXP-028 (`experiments/rejected/EXP-2026-09-05-028-fused-iq2s-prefill.md`) проверял PREFILL на
  дереве до pinned ring. Согласно EXP-030, bulk prefill выполняется CUDA MMQ, то есть CPU fused
  путь в том эксперименте почти не исполнялся (только 4-токенный checkpoint-хвост). Отказ EXP-028
  относится к prefill и не является отказом decode.
- Полноценного paired decode-испытания этого пути никогда не было. Медианы 22,17 (control) vs
  23,90 (fused-retry) tok/s из `results/archive/EXP-2026-09-06-034/raw/exp028/` — побочный эффект
  16-токенной генерации без парного протокола; ускорением не объявляются.
- В decode текущего приняттого runtime fused путь исполняется на каждом verify-шаге:
  `--cpu-moe` держит экспертов на CPU при батче < 32 (EXP-043), средняя длина принятия MTP 2,42
  (логи EXP-023/024) — verify-батч обычно 3 строки, то есть группы 2–3 возможны каждый шаг.
- EXP-021 (`results/exp021-fused-bench.txt`): микроускорение IQ2_S fused при Ny=2 — 1,36x,
  Ny=4 — 1,84x, maxdiff=0.0. Это основание кандидата, не доказательство серверного выигрыша.

## Ожидаемая польза

≥5% decode tok/s на одинаковой нагрузке (при условных 22 tok/s — около 23,1). Критерий полезности,
не прогноз. Модель, качество, MTP n_max=2, checkpoint и принятый профиль сохраняются.

## Baseline

- Текущий runtime: `build/exp045-default-runtime` (default-on pinned ring + two-thread gather),
  лаунчер `scripts/run_qwen38_server.sh`, THREADS=12, `--cpu-moe`, MTP draft-n-max=2, greedy temp=0.
- Сохранённые decode-ориентиры (несопоставимы напрямую, разная конвенция и бинарники):
  EXP-040 short-prompt decode median 16,0924 tok/s (ring OFF) / 15,6159 (ring ON);
  EXP-028 archive 22,17 (control) — 16 токенов, не парный.
- Свежий control A берётся в самом A/B (правило R2: отдельного baseline-эксперимента нет).

## Единственная переменная

Fused IQ2_S путь для групп из 2–3 строк на пути CPU `MUL_MAT_ID` (target verify), env-переключатель
`GGML_CPU_IQ2S_FUSED` (один binary, OFF = поведение принятого runtime). Не меняются: IQ4_NL
(ffn_down_exps), число threads, MTP-параметры, offload, quantization, checkpoint, ring/gather.

## Область кандидата (минимальная)

- Источник: новый каталог `work/llama.cpp-exp046-iq2s-verify` = свежий клон + цепочка
  `expert-tier-integration + integration-drift + mtp-sidecar + pinned-ring + pinned-ring-default-on +
  two-thread-gather` (база 4aaad5d3), поверх — port fused-хунков EXP-028
  (`ggml_compute_forward_mul_mat_id_iq2s_one_chunk` + `ggml-cpu-iq2s-grid.h`).
- Сборка: новый каталог `build/exp046-iq2s-verify`; принятые сборки, launcher и
  `work/llama.cpp-integration` не трогаем. Сомнительные объектники старых сборок не используем.
- Guard: `type == GGML_TYPE_IQ2_S && cne1 >= 2 && !ggml_is_numa()` (как в EXP-028) плюс
  ограничение фазы `src1->ne[1] >= 2 && src1->ne[1] <= 3` (размер ubatch verify при n_parallel=1;
  checkpoint-хвост prefill с ne[1]=4 тем самым исключается). Env читается один раз (в EXP-028 был
  `getenv` на каждый вызов — порт это исправляет). Однострочный путь (cne1==1) остаётся stock
  vec_dot.
- Draft исключён по типу: у MTP-draft (models/qwen38/MTP/mtp-Qwen3.8-Flash-Next-Q4_K_M.gguf)
  эксперты blk.48 — Q4_K/Q8_0, не IQ2_S; guard по типу не пускает в fused ничего из draft-контекста,
  включая catch-up проходы батчем целиком. Ограничение по размеру батча не отождествляется с фазой:
  атрибуция verify подтверждается счётчиком (шаг 3 Correctness) по ne[1] и сверкой с числом
  verify-шагов в логе слота.
- Применимость по покрытию: IQ2_S = ffn_gate_exps + ffn_up_exps (24,08 GiB из ~46 GiB весов
  экспертов); ffn_down_exps — IQ4_NL (21,6 GiB) — вне кандидата; gate/up слоя 2 — IQ3_S — вне
  кандидата. Guard по размеру батча не отождествляется с фазой verify: фаза подтверждается
  счётчиком (см. Correctness), а не guard'ом.

## Проверка переноса (результат сопоставления EXP-028 ↔ текущий runtime)

- Region в `ggml-compute_forward_mul_mat_id` в текущем дереве идентична базе EXP-028 (hook
  `g_expert_ready_hook` есть в обоих) — хунки портируются без конфликта.
- Strides/row mapping: fused использует то же `(i11 + i12*ne11)*row_size` и `MMID_MATRIX_ROW`,
  что и stock-путь при конвертированном в Q8_K contiguous `wdata` (для IQ2_S src1 f32 всегда
  конвертируется — ветка strides не нужна). Запись `dst_col[ir0]` со stride 1 — то же допущение,
  что у stock (dst mul_mat_id contiguity assert).
- Scratch: дополнительных аллокаций нет; wdata layout не меняется; атомарные chunk-счётчики
  переиспользуются (`atomic_current_chunk + cur_a`); fused делит только nr0 (у gate/up ne01=640,
  40 чанков на 12 потоков) — совместимо с текущим worker scheduling.
- Численный порядок accumulation отличается от stock vec_dot (один float-аккумулятор на ряд
  против по-subблочных); EXP-021 дал maxdiff 0.0 на тех же shape. Гейт корректности требует
  совпадения с контролем и исследования любого расхождения (не списывать на near-tie).
- Взаимодействия с pinned ring нет: ring стейджит CUDA-экспертов; CPU-эксперты читают mmap GGUF
  напрямую.

## План Correctness (до performance)

1. Модель-свободный unit-гейт на реальных Qwen shape (ne00=2560, ne01=640, 512 экспертов,
   top-k=10, rows=1/2/3): sparse/repeated expert IDs (включая id=0 и id=511), сравнение ON vs OFF
   output (точное совпадение; иначе — разбор), при 1 и 12 threads; NUMA-guard проверить на текущей
   машине.
2. Сверка значения `iq2s_grid` из `ggml-cpu-iq2s-grid.h` с массивом в `quants.c` (byte-identical).
3. Env-gated счётчик вызовов fused + гистограмма `cne1` (1/2/3+) по фазам; один короткий прогон
   (запрос 395 токенов, n_predict=64, temp=0) подтверждает попадание именно в verify-фазу
   (decode-шаги), отдельно от checkpoint-хвоста prefill. Одновременно это минимальное измерение
   доли подходящих verify-групп — единственное недостающее наблюдение (гистограмма EXP-029
   относится к 4-токенному checkpoint-хвосту и не переносится на decode).
4. Модельная детерминированность: одинаковые запросы, temp=0, одинаковый seed; ON vs OFF —
   совпадение токенов ответа и MTP acceptance/rollback-счётчиков; расхождение исследовать.

## План decode A/B (после готовности кандидата и гейтов; запуск только после правила 1)

- Один binary, OFF/ON; одинаковые prompt, cache policy, sampling (temp=0), MTP n_max=2.
- Прогрев отдельно. Новый фиксированный input `benchmarks/inputs/exp046-decode.json`
  (probe: ≥256 фактических токенов без раннего EOG; иначе запрос заменить).
- Ориентир 256 фактически полученных токенов; ранний EOG/пустой ответ — проба невалидна.
- Одна предварительная пара; число дальнейших пар согласовать по её результанию (для ACCEPT — не
  менее 5 пар по правилу roadmap).
- Основная метрика: decode tok/s по конвенции actual token counts
  (`tokens_generated / (predicted_ms/1000)`), одинаковой для обеих рук. Дополнительно: decode time,
  generated tokens, draft/accepted counts, CPU, RSS, VRAM, VmSwap, sha256 ответов. Prefill —
  записать отдельно, в основной выигрыш не включать.
- Измерения вручную, строго по одному; профилируемые времена не использовать как performance.

## Критерии

- ACCEPT: медиана decode ≥ +5% по парным данным (≥5 пар, большинство пар в пользу ON), корректность
  пройдена (unit-гейт + совпадение ответов/acceptance), регрессий памяти/swap нет.
- REJECT (в т.ч. без модельного A/B): если гистограмма шага 3 даёт долю IQ2_S строк в группах ≥2
  ниже ~20%, либо unit-гейт/mикроуровень не подтверждает экономия, либо расходы (счётчик, ветка)
  съедают выигрыш. Автоматического перебора других настроек нет.
- Изменение default runtime — отдельное решение Игоря.

## Риски

- Verify-батч 3 строки даёт группы ≥2 только при совпадении маршрутов соседних токенов; доля
  подходящих групп в decode неизвестна (данных нет — закрывается шагом 3 до производительностного
  A/B).
- CPU-эксперты в decode частично bandwidth-bound: экономия ALU (1,36x на dot при Ny=2) может не
  конвертироваться в wall-time.
- Различие порядка accumulation → расхождение токенов при greedy; исследовать обязательно.
- Наследие EXP-028: CUDA-graph OOM на tight VRAM — fused не затрагивает CUDA-путь, но полный
  correctness-прогон профиля обязателен.
- 2–3-токенный prompt/resume-ubatch формально совпадает по shape с verify — попадание в fused
  численно безвредно (гейт точного совпадения), но фазовая атрибуция держится только на счётчике;
  при расхождении счётчика с числом verify-шагов кандидата не принимать.
