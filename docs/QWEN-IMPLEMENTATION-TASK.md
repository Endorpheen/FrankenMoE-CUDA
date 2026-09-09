# Техническое задание локальному Qwen3.8 Flash Next

Дата подготовки: 2026-09-06; статус актуализирован после EXP-038. Исполнитель — локальный Qwen. Пользователь — Игорь. Общение и отчёты по-русски; code comments и commit messages по-английски.

## 0. Текущая точка продолжения — не повторять закрытые этапы

- R0/EXP-034: `ACCEPTED`, committed in `1cf489a`. Закрыт; не выполнять снова.
- R1/EXP-035: `ACCEPTED`, committed in `7e8b819`. Закрыт; не выполнять снова, включая synthetic/lifecycle/model smoke.
- R2/EXP-036: `NOT_RUN_DUPLICATE`. Не создавать и не запускать: необходимые baseline/transfer measurements уже находятся в EXP-030–033.
- R3/EXP-037: `ACCEPTED`, committed in `2c76446`. Закрыт; offline trace-анализ не повторять.
- R4/EXP-038: `ACCEPTED` (`kind=performance`), ожидает отдельного commit approval. Формальный A/B уже выполнен: median prefill `+13,16%`, 5/5 пар, decode `+5,23%`, correctness/memory gates PASS. Не пересобирать и не повторять A/B.
- R5/EXP-039: `NOT_RUN`/`DEFERRED`. Не начинать.
- R6: единственный следующий этап после отдельного handoff и согласования запусков с Игорем.

Разделы R0–R2 ниже сохранены только как историческое описание уже закрытого плана. Они не являются инструкцией к выполнению. При новом handoff всегда продолжать с текущего статуса в этом разделе и `ROADMAP.md`, а не с первого этапа документа.

## 1. Цель и порядок чтения

Уменьшить реальное время обработки prompt в patched expert-tier server при сохранении качества, текущего warm decode и ограничений RAM/VRAM. Воспроизводимость, safety profile, offline transfer-анализ и основной R4 A/B уже закрыты; текущая работа начинается с R6 acceptance coverage. Не ускорять произвольный microbenchmark вместо пользовательского сценария.

Прочитать в таком порядке:

1. `AGENTS.md` — правила пользователя о запуске/коммитах, общении и полный handoff.
2. `ROADMAP.md` — единственная актуальная очередь; фактическая точка продолжения R6.
3. `docs/AUDIT-2026-09-06.md` — находки и история 000–033.
4. Этот документ целиком; затем только записи, относящиеся к текущему этапу.
5. `experiments/EXP-2026-09-05-030-bulk-prefill-cuda-path.md`, EXP-031/032/033, rejected EXP-033 patch — перед R3/R4.

Не следовать старым «Next experiment», «P1 closed», «P6 closed» в историческом журнале, если они противоречат ROADMAP. Не переписывать отрицательные результаты под новую гипотезу.

## 2. Неподвижные условия

- Только один эксперимент за раз, без параллельных моделей, benchmarks и CPU/CUDA builds. Не запускать дочерних агентов для исполнения экспериментов.
- `work/llama.cpp-integration` и `build/expert-tier-franken-cuda` — пользовательские рабочие пути; не редактировать/пересобирать. Любой кандидат — отдельные `work/llama.cpp-expNNN-*` и `build/expNNN-*`.
- Не делать `git reset --hard`, `git clean`, force checkout, force push, массовый restore/revert, stash чужих файлов. Не удалять failed attempts. `git add -A` запрещён; stage только явный allowlist после просмотра diff.
- Не обновлять upstream, CUDA, compiler, model quantization, tokenization, chat template, top-k, EHS, context, batch sizes и thread counts как побочный эффект.
- EHS=0, OpenMP ON, 12 threads, MTP `--spec-draft-n-max 2`. Не пробовать больший draft limit, CPU-only draft, ngram, 24/32 threads, PASSIVE/SPINCOUNT, старый CPU fused, madvise/warmer или single-buffer packed upload.
- Не выполнять live eviction/madvise/DONTNEED для работающего CUDA server. Не создавать memory pressure/cgroup swap/OOM для тестирования watchdog. Не менять sysctl самостоятельно.
- Измерения запускать вручную последовательно; существующие batch benchmark scripts не выполнять. Разрешена offline обработка уже завершённых результатов и согласованная telemetry одного server. Это не разрешение на фоновый нагрузочный процесс.
- Перед стартом модели/сервера, включая tiny-model gate, сообщить Игорю цель, ожидаемую длительность и точное число запусков/запросов; дождаться явного «да», как требует `AGENTS.md`, Hard rule 1. Можно заранее согласовать целый конечный блок запусков; не спрашивать повторно в пределах неизменного согласованного блока. При расширении блока запросить новое согласование.
- Перед commit показать outcome, memory impact, correctness и список файлов; commit только после approval по `AGENTS.md`, Hard rule 2. Подготовить reviewable diff до вопроса. Не считать разрешение на аудит разрешением на модель или commit.
- Применять имеющиеся gitrules skills перед code comments/commit. Не менять языковые правила задним числом в пользовательских файлах.

## 3. Карточка эксперимента и протокол решения

До кода создать `experiments/EXP-<дата>-NNN-<name>.md` со статусом `PLANNED`, используя свободный номер не ниже 034. Номера roadmap — резерв смыслов: если заняты новой работой пользователя, сначала переименовать очередь документально.

Карточка обязана содержать:

- ровно одну проверяемую гипотезу и связь с предыдущим измерением;
- baseline source/patch/binary/libs SHA-256, CMake flags, argv/env, request SHA-256;
- allowlist файлов, фаза inference, условие включения/выключения;
- единственная основная переменная, метрика, заранее выбранный memory budget;
- количество warmups/measured runs, контроль cache state, stop/accept/reject criteria;
- какие результаты приведут к следующему этапу, какие закроют направление.

Статусы: `ACCEPTED` отдельно с `kind=performance|correctness|infrastructure`; `REJECTED` при отрицательном результате; `INCONCLUSIVE` при шуме/несопоставимости; `BLOCKED` при недоступной зависимости; `NOT_RUN` при провале design gate до запуска. Рабочий код сам по себе не основание ACCEPTED.

Для performance основной gate — **снижение median prompt_ms ≥3%** относительно свежего A из того же протокола. Использовать пять пар, порядок A/B, B/A, A/B, B/A, A/B; считать `gain_i=100*(A_ms-B_ms)/A_ms`, median gain и median каждого arm. Не смешивать gain latency с gain tok/s. Не менее 4 из 5 пар должны выигрывать, median paired gain ≥3%; bootstrap 95% interval paired gain должен быть выше нуля. При малой выборке interval — вспомогательный, не доказательство широкой обобщаемости. Если естественный разброс сопоставим с эффектом или интервалы не позволяют вывод — INCONCLUSIVE, baseline не менять. Не увеличивать выборку до случайной победы: один заранее согласованный дополнительный блок до суммарных 10 пар допустим, затем остановиться.

Границы регрессий для нового transfer-only пути:

- Warm decode median не хуже 2%; ухудшение >2% означает отклонение либо отдельную проверку шума до решения, не waiver.
- Для secondary prompt lengths median latency не хуже 3%; нулевая терпимость к silent fallback вместо заявленного coverage.
- VmSwap=0; нет CUDA errors, OOM, watchdog trips, partial copies, hangs, invalid output.
- Новые buffers имеют жёсткий cap; после request/shutdown нет роста невысвобожденных ресурсов. Для R4 host budget 2×16 MiB = 32 MiB, metadata ≤1 MiB, device staging 0. RSS прирост сверх budget +32 MiB допуска измерений расследовать; для R5 отдельный cap ниже.
- Согласованный baseline memory envelope остаётся одинаковым для A/B. Исторические 934 MiB headroom — наблюдение, не гарантия. Для новых allocations до performance определить доступный запас; если нехватка, остановиться, а не незаметно урезать context.
- Early stop: correctness failure/OOM сразу; >10% latency degradation на сопоставимой exploratory pair — REJECTED без обязательных пяти пар. Первый request с cold file cache сам по себе не сопоставим с warm.

Infrastructure/safety stages не обязаны ускорять модель: принимаются по точным функциональным gates. Изменения throughput не заявлять. Отказ сохраняет таблицу цифр и patch кандидата в rejected, если он полезен; working defaults остаются baseline.

## 4. R0 / EXP-034 — CLOSED, не выполнять повторно

Status: `ACCEPTED`, committed in `1cf489a`. Provenance, архив EXP-023–033, воспроизводимый MTP patch chain и isolated build уже проверены. Полная история находится в `experiments/EXP-2026-09-06-034-provenance.md`. Не повторять инвентаризацию, hashing, patch reconstruction или build.

## 5. R1 / EXP-035 — CLOSED, не выполнять повторно

Status: `ACCEPTED`, committed in `7e8b819`. Units/thresholds watchdog, launcher lifecycle, synthetic boundaries и model smoke уже проверены. Полная история находится в `experiments/EXP-2026-09-06-035-watchdog-safety-profile.md`. Не повторять тесты и не запускать модель.

## 6. R2 / EXP-036 — NOT_RUN_DUPLICATE, не выполнять

Отменён до запуска: EXP-030/031/032 уже содержат baseline и transfer measurements, EXP-033 — сопоставимый control и отклонённый candidate. Не создавать карточку, fixture, manifest или результаты EXP-036. Будущий performance-кандидат обязан иметь свежий control A внутри собственного A/B, но отдельный baseline experiment для этого не нужен.

## 7. R3 / EXP-037 — сначала существующие traces и dependency budget

**Гипотеза:** существенная часть bulk-prefill latency связана с host submission/подготовкой pageable expert ranges, и bounded staging может сократить её без изменения графа. Здесь гипотеза проверяется аналитически, без реализации оптимизации.

**Области чтения:** `ggml/src/ggml-backend.cpp::ggml_backend_sched_compute_splits`, ветка `copy_experts`; `ggml/src/ggml-cuda/ggml-cuda.cu::ggml_backend_cuda_set_tensor_async`, `ggml-cuda/common.cuh` (context/pool/stream), scheduler events/allocator. EXP-033 patch читать как rejected evidence, не применять.

**Выход:** offline report/JSON и при необходимости маленький parser сохранённых traces. Сначала SQL read-only для EXP-031 SQLite, CUDA API↔activity correlationId. Сверить time origin server log и profiler; нельзя автоматически считать их timestamp одинаковым.

Нужна таблица отдельно для in-request bulk, tail и startup:

- number/bytes copies, src memory kind по enum, stream IDs;
- host cudaMemcpyAsync API durations (sum и union), device copy durations (sum и union), kernel union, copy/kernel intersection;
- sync API durations, интервалы без CUDA work; не называть их CPU idle без CPU evidence;
- sizes p50/p90/max, consecutive range counts, actual selection coverage;
- места materialization IDs, allocator input reuse wait, copy enqueue, compute enqueue, last destination consumer.

Проверить, что exact adjacent ranges уже coalesced. Не делать новый «coalescing adjacent IDs» patch. Проверить предыдущие assertions про graph inputs на актуальном source: отследить реальную ветку, не полагаться на название tensor.

Предварительная оценка: суммарный GPU kernel time 0,439 s в 4,016 s request — даже полное скрытие этой суммы само по себе не обещает многократного ускорения. Добавленный CPU memcpy всех 21,64 GiB и memory bandwidth competition могут съесть весь эффект. Предсказать численно saved host critical time и added gather cost диапазоном, явно назвать неизвестные параметры. Cycle shares и timeline span не подставлять вместо wall critical path.

**Gate R4:** доказан pageable/host submission компонент, существует возможность подготовить chunk N+1 до completion N без снятия allocator barriers; ожидаемый net gain хотя бы 5% с запасом относительно порога 3%. Если saved trace не содержит параметра, разрешён только новый небольшой диагностический сигнал по отдельному согласованию: не повторять полную attribution EXP-031/032. Нет положительного бюджета — R4 NOT_RUN. **Gate R5:** карта реального независимого intra-layer window и ownership destination; без неё R5 NOT_RUN. Если оба gate отрицательны — P1 DEFERRED, завершить handoff без нового kernel/cache эксперимента.

## 8. R4 / EXP-038 — bounded host pinned ring, прямой upload

**Новизна:** EXP-033 делал whole-projection pack→device scratch→scatter и ожидал единственный host buffer. Этот опыт использует только два ограниченных host chunks и прямые async writes в прежние destination offsets. Никакого device scratch или scatter; число логических selected ranges не уменьшается как цель эксперимента.

**Allowlist isolated source:** `ggml/src/ggml-backend.cpp`, `ggml/src/ggml-cuda/ggml-cuda.cu`, при необходимости внутренний backend интерфейс `ggml/src/ggml-backend-impl.h`, точечный scheduler integration test. Если расширение optional interface нужно, явно инициализировать его во всех используемых backends; это часть совместимости, не разрешение менять их алгоритмы.

**Baseline:** текущий принятый split-MTP runtime и свежий control A внутри самого EXP-038; отдельного R2 baseline нет. A/B выполняется в одном candidate binary с flag OFF/ON для timing, перед этим OFF должен пройти parity с clean reference. Flag default OFF; значение `0` действительно OFF, проверять значение, а не только наличие env. Название нового flag записать в карточку, например `GGML_EXPERT_PINNED_RING`.

Алгоритм маленькими подэтапами, каждый с коротким handoff:

1. На CPU построить view списка уже имеющихся `copy_experts` ranges без новых IDs/router passes. Guard только проверенный bulk path (`MUL_MAT_ID`, host weights→CUDA, token dimension ≥32), не draft verify Ny≤3 и не four-token tail. Убедиться в смысле dimension на source/coverage.
2. В context конкретного backend, без process-global mutable state, создать максимум два pinned host slots по 16 MiB. Инициализировать лениво вне повторяемой timed steady-state части через warmup; capacity не расти. Проверять successful pinned allocation; pageable fallback нельзя считать pinned success.
3. Chunk range >16 MiB разбивать побайтно с сохранением адресов; сохранять исходные extra padding bytes `min(expert_size,512)` для не последнего expert. Не заполнять padding произвольными нулями вместо исходных bytes. Не делать операций quant/dequant и ID remap.
4. Для slot: `FREE → CPU_FILL → H2D_IN_FLIGHT → FREE`. Перед overwrite slot ждать только его recorded completion event. После async enqueue на **существующем stream** записать event. CPU может заполнить второй slot, пока DMA читает первый. Source mapping остаётся живым во время CPU copy; pinned slot жив до завершения DMA.
5. Не удалять scheduler sync/wait и не трогать allocator reuse. Все H2D chunks projection ставятся перед его MMQ consumer на том же stream. Не объявлять это H2D/MMQ overlap: на этом этапе проверяется host-fill/H2D overlap.
6. При невозможности выделить slots/создать events до submission — отключить candidate path и выполнить обычный путь с понятным reason/counter. При ошибке после частичной submission — корректно дождаться/abort согласно backend contract; не продолжать compute на частично загруженных данных. Unsupported backend/shape остаётся прежним путём.
7. В shutdown/reset/reallocation drain всех in-flight slot events до free. Нет host buffer reuse между независимыми contexts. Не использовать краткоживущий vector.data() для асинхронных метаданных без lifetime guarantee.

Correctness до performance:

- Test-backend-ops для фактической CUDA build и relevant quantized MUL_MAT_ID shapes; дополнительно scheduler test host weights→CUDA с ≥32 tokens и explicit hit counter нового path.
- Sparse/adjacent IDs, repeat IDs, expert 0/last, non-contiguous IDs strides, ranges на 16 MiB границе и больше, ≥3 slot cycles. Byte compare destination selected ranges/padding с reference после synchronize; sentinel guards вокруг destination, проверка source не изменён.
- Flag OFF/no CUDA/alloc failure fallback; cancellation и shutdown с in-flight upload; последовательные requests меняют selected sets и graph shape. Tiny fixture должен активировать ring, не только direct CUDA resident kernel.
- Одинаковый фиксированный 395+16 request в A/B, nonempty hash и tokens; MTP flags одинаковы. Расхождение — REJECTED/BLOCKED correctness, не «near tie» без исследования.

Затем согласованная exploratory пара с warmups; если gate не провален, пять formal pairs по разделу 3. На каждую leg fresh server, один warmup primary, один measured primary, один warmup decode, один measured decode; это 10 server starts и 40 requests на пять пар. Выполнять вручную, не параллельно. Если общий протокол изменён по времени согласования — заранее одинаково для обеих legs, новый manifest.

Сохранить prompt_ms, decode timings, raw outputs, actual bytes/calls, GPU/RSS peaks, faults/read_bytes, temperatures. Host slot waits/gather counters собирать aggregate в отдельном diagnostic pass либо с одинаковым минимальным overhead, не per-copy printf в timed run. Причинное объяснение проверять отдельно от unprofiled speed.

**ACCEPTED:** все gates и ≥3% end-to-end latency improvement. **REJECTED:** меньше calls или видимый overlap без gain, memory overrun, regressions. **После принятия:** R6 для данного кандидата. **После отказа:** сохранить patch/reason, вернуться к baseline; R5 только если собственный R3 design gate положителен, не как автоматическая попытка «ещё один buffer».

## 9. R5 / EXP-039 — отдельный intra-layer transfer/compute overlap

Этот этап условный. Не начинать только потому, что R4 готов или отвергнут. Если R3 не доказал окно, завершить `NOT_RUN` с описанием отсутствующей зависимости. Не разрабатывать cross-layer predictor.

**Гипотеза:** при уже известных IDs одного слоя можно передавать веса независимой следующей projection, пока GPU вычисляет текущую, и выигрыш превышает цену событий и дополнительной памяти. Доступность IDs не означает, что scheduler destination ещё свободен.

**Allowlist:** тот же scheduler/CUDA boundary; context-owned stream/events и tests. Не менять MMQ kernel, graph математически, router, CPU scheduler или MTP. R4 может стать новым baseline только после его R6 приёмки; иначе сохраняется последний принятый runtime и для каждого кандидата снимается свежий control A без rejected кода.

Обязательный design document до patch:

1. По конкретным node names/source edges показать A compute, B copy и почему A не читает/пишет destination B. Отдельно показать production order gate/up/down; не предполагать фиксированный порядок по имени.
2. Записать destination lifetime от allocator allocation до last consumer. Нельзя enqueue B в переиспользуемый buffer A. Если без новой выделенной памяти это невозможно, ограничить total дополнительную VRAM **128 MiB**; использовать chunks только если не требуется менять MMQ для их потребления. Если целая необходимая projection не помещается, gate FAIL → NOT_RUN, не увеличивать budget.
3. Producer pinned slot остаётся жив до `copy_done`. Compute stream ждёт `copy_done` через device event; copy stream ждёт `last_use_done` перед destination overwrite. Host slot и device destination имеют разные lifetimes. Pool allocations с предположением одного stream не использовать на втором без доказанного event-safe ownership.
4. Проверить CUDA graph capture/replay compatibility; нельзя сохранять в graph указатели на уже освобождённые slots или менять IDs binding между replays. Если корректность требует отключить CUDA graphs, это другая основная переменная — этот кандидат остановить, не подменять эксперимент.
5. Fallback/reset/cancel/error drain обоих streams до free; backpressure при полном bounded pool; нет global device synchronize на каждом range как скрытого «решения» гонки.

Сначала focused lifetime/race/byte tests из R4 плюс repeated CUDA graph replay и cancellation. Потом один отдельный diagnostic run доказывает реальное пересечение copy B и compute A на timeline, не только наличие второго stream. Нет overlap — REJECTED design. Потом unprofiled paired benchmark с тем же протоколом/gates. Выигрыш — R6; провал — закрыть эту реализацию и закончить P1 с baseline, не строить третий redesign самостоятельно.

## 10. R6 — принять только подтверждённый результат и доставить patch

Это завершение текущего performance эксперимента, не право объединить другие идеи. Primary five pairs уже сделаны, не повторять их без причины.

1. Пять последовательных пар secondary short/long fixtures и warm decode, если соответствующий five-pair decode ещё не выполнен. Утвердить число запусков заранее. Измерять actual evaluated tokens, TTFT/request wall и памяти; cache policy одинаковая. TTFT streaming измерять отдельно, если primary `/completion` nonstream response не предоставляет её напрямую.
2. Correctness последовательной сессии: short→long→short, prefix-cache path, MTP rejection/rollback и checkpoint restore, cancellation→next request, clean shutdown. Не выключать checkpoints ради результата. Concurrent aggregate benchmark P10 не запускать. Если новый state не проверен при нескольких slots, candidate остаётся scoped opt-in single-request до соответствующего gate, default multi-slot не менять.
3. Если secondary regression >порогов — REJECTED общего default. Узкий opt-in допустим только с заранее определённым shape guard и повторной проверкой именно этого guard; не вырезать неудобный prompt из таблицы post hoc.
4. Подготовить минимальный delivery patch поверх R0 package. Из новой isolated source проверить apply/build и focused correctness; не заменять пользовательский integration tree. Defaults до review сохраняются.
5. Обновить experiment report, compact JSON, manifest, roadmap status и handoff. ACCEPTED performance требует цифр, memory envelope и correctness, а не только test pass. Для rejected аналогично сохранить before/after и reason, приложить patch по необходимости.
6. Показать Игорю конкретный diff/list/results для commit approval. При ожидании approval статус реализации `VALIDATED_PENDING_COMMIT`, следующий эксперимент не начинать; не объявлять уже установленный daily default.

Если все performance-кандидаты отклонены/NOT_RUN, итог корректен: «baseline сохранён, измеримого улучшения не найдено». Не реализовывать автоматически P2/P4/P5/P8/P9/P10, чтобы обязательно показать изменение.

## 11. Формат данных и handoff

Каждый JSON содержит `experiment_id`, `kind`, `status`, `baseline_id`, source/patch/binary/libs identity, argv/env, model/head/request hashes, attempts и exclusions, все per-run values, medians/paired deltas/spread, memory/correctness, decision и next_id. Для durations единицы ms, byte counters bytes либо явно MiB; counters cumulative переводить в delta окна. Не складывать cumulative `graphs reused` разных request snapshots.

Raw имена уникальны, например `results/archive/EXP-.../pair-01-A-{server.log,response.json,monitor.csv}`. Старые results не перезаписывать. Не делить generated budget на время, если фактический count отличается; у llama timings проверить n vs n−1 convention. «Latency +63,4%» не равно «tok/s −63,4%».

После каждого небольшого подэтапа кратко:

```text
Проверено: <gate/coverage или причина NOT_RUN>
Изменено: <точные файлы и единственная переменная>
До → после: <метрики, единицы, n; если без benchmark — «не измерялось»>
Вывод: <что данные доказывают и чего не доказывают>
Решение: <ACCEPTED / REJECTED / INCONCLUSIVE / BLOCKED / NOT_RUN>
Далее: <один конкретный пункт R# и ближайшее действие>
```

После завершения эксперимента обязательно дополнить полным блоком «ПЕРЕДАЧА РАБОТЫ» из `AGENTS.md`: это требование файла, а не замена короткого отчёта. Обновить дату, branch/full SHA, dirty state, обязательные правила, current runtime, baseline, latest verdict/artifacts/hashes, последние commits, следующий experiment, risks, остаток P0–P11, ближайшее действие. Ни один новый этап не начинается из памяти диалога; только из сохранённого handoff и roadmap.
