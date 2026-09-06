# FrankenMoE roadmap — пересмотр 2026-09-06

Аудит: [docs/AUDIT-2026-09-06.md](docs/AUDIT-2026-09-06.md).
Последовательное ТЗ: [docs/QWEN-IMPLEMENTATION-TASK.md](docs/QWEN-IMPLEMENTATION-TASK.md).
История: [experiments/EXPERIMENTS.md](experiments/EXPERIMENTS.md), отдельные EXP-028–033 и `benchmarks/history.jsonl`. Старые решения сохраняются; позднейшее уточнение имеет приоритет над старым «Next experiment».

**Roadmap требует существенного обновления.** Основная цель — patched expert-tier server. `bmoe-cli` остаётся самостоятельной исследовательской веткой. Их baseline, I/O и H2D нельзя смешивать. Ничего из очереди ниже при аудите не реализовано.

## Статусы фаз P0–P11

| Фаза | Текущий статус и основание |
| --- | --- |
| P0 — воспроизводимость | Частично выполнена: EXP-000/004 и серверные измерения. Открыты provenance MTP, сохранность артефактов, единый профиль и корректность async transfer. |
| P1 — SSD/RAM/GPU | Старые варианты CPU-decode prefetch закрыты: EXP-001/002/003/012/025/027. Bulk CUDA prefill открыт отдельно по EXP-030/031/032. EXP-033 single-buffer packed staging отклонён. |
| P2 — hot cache | Базовые RAM LRU и VRAM banks существуют в bmoe-cli; серверный EHS activation отклонён EXP-010. Новый cache не планируется без reuse/memory evidence. |
| P3 — router prefetch | Поздний madvise и warmer отклонены. Router следующего слоя ещё неизвестен. Только доказуемый overlap внутри слоя может стать отдельным экспериментом; предсказание маршрутов отложено. |
| P4 — контейнер/чтение | Адресный split-GGUF/O_DIRECT уже есть в bmoe-cli. Переупаковка модели и новый I/O engine отложены. Точное объединение соседних H2D expert ranges уже есть в server scheduler. |
| P5 — kernels | EXP-019 micro-tuning закрыт по текущему приоритету, EXP-021 microbench выполнен, EXP-028 CPU fused integration отклонён. Bulk prefill реально CUDA MMQ; повтор CPU fused не запланирован. |
| P6 — speculative | EXP-022 ngram и EXP-023 CPU-only draft отклонены; EXP-024 split MTP принят исторически (+13,6%, одна пара). Доставляемость patch и более широкая корректность пока не подтверждены. `n_max=2`; больший лимит не повторять. |
| P7 — memory planning | RAM startup gate и EHS safe autofit реализованы EXP-008/009; цельный planner отложен. Открыта проверка действующего watchdog/launch profile. |
| P8 — mixed quant | Отложено: нет репрезентативного quality baseline и бюджета качества. |
| P9 — REAP | Отложено по той же причине; модель и top-k не менять. |
| P10 — batching | Отложено до устойчивого single-request пути и проверки lifetime при нескольких запросах. Aggregate tok/s — отдельная метрика. |
| P11 — API/autotuning | OpenAI-compatible server уже работает. Автотюнинг и UI не являются следующим шагом. |

## Очередь выполнения

ID ниже резервируют смысл этапов, дата EXP назначается при старте. Не выдавать их за выполненные опыты.

1. **R0 / EXP-034 — provenance и воспроизводимый пакет**. Status: PASS (ожидает коммит-аппрувал). Зафиксированы root/nested trees, живой сервер (включая перезапуск в ходе снятия), хэши бинарников/библиотек/шейдов модели, артефакты EXP-023–033 и reference status EXP-000–033. Цепочка `integration + integration-drift + mtp-sidecar` от пина `4aaad5d` доказанно воспроизводит рабочее дерево (кроме одной пустой строки) и собирается; `--help` совпадает с живым бинарником. `experiments/EXP-2026-09-06-034-provenance.md`, `benchmarks/manifests/exp034-provenance.json`, `patches/README-mtp-sidecar.md`.
2. **R1 / EXP-035 — работающий safety profile**. Отдельный correctness этап: единицы внешнего watchdog, включение внутреннего watchdog в явном benchmark profile, корректное завершение monitor. Не провоцировать настоящий swap/OOM. Gate: синтетические пороги/жизненный цикл, затем неизменная inference-конфигурация.
3. **R2 / EXP-036 — актуальный baseline и correctness fixtures**. Один выбранный 32K MTP profile, `n_max=2`, EHS=0, 12 threads. Фиксированные запросы, полный prefill без prompt-cache reuse, отдельный warm decode, единые raw artifacts. Это baseline для новой работы, не повтор состязания MTP/no-MTP.
4. **R3 / EXP-037 — offline transfer budget и dependency map**. Сначала использовать уже сохранённые EXP-031/032. Разделить GPU H2D duration, host API time, sync и gaps; проверить область запроса и доступность свободных staging slots. Не снимать повторно уже известную атрибуцию 22,16 GiB к экспертам.
5. **R4 / EXP-038 — условный bounded pinned ring без packing/scatter**. Только если R3 показывает измеримый host-side submission bottleneck и возможность готовить следующий chunk. Два ограниченных host slots, прямые копии в исходные offsets, прежний CUDA stream. Это гипотеза CPU-staging/H2D overlap; истинный H2D/compute overlap не обещать. При недостаточном ожидаемом эффекте — `NOT_RUN`, перейти к условному R5 либо завершить P1.
6. **R5 / EXP-039 — условный transfer/compute overlap внутри слоя**. Только при новой dependency/timeline evidence после R3/R4. Отдельный copy stream, события готовности/последнего потребителя, ограниченная дополнительная память. Не выдавать указатель на scheduler buffer за lifetime guarantee. Если безопасного независимого окна нет — `NOT_RUN`, направление `DEFERRED`.
7. **R6 — приёмка конкретного победителя**. Репрезентативный короткий/основной/длинный prompt, decode, повторные запросы, cancellation/checkpoint/shutdown. Только затем patch доставки и предложение изменения default. Нет победителя — сохранить baseline и закончить с отрицательным результатом.

Если R4 принят, сначала R6. R5 начинается отдельным циклом только после handoff и лишь при оставшемся измеренном bottleneck. R4 и R5 не реализуются одновременно.

## Правило решения

Один эксперимент, одна гипотеза, один baseline, одна основная переменная. Performance ACCEPTED требует ≥3% сокращения median prefill time, подтверждения парными результатами вне наблюдаемого шума и отсутствия неприемлемых регрессий. Меньше H2D calls, работающий код или меньше CPU spin сами по себе не являются успехом. Минимум пять последовательных пар для принятия; ранний отказ при явном провале допустим. Correctness/infrastructure принимаются по своим заранее указанным функциональным критериям без заявления ускорения.

Плохой результат сохранять в журнале, код не включать. `INCONCLUSIVE`, `BLOCKED`, `NOT_RUN` не превращать в ACCEPTED. Handoff обязателен после каждого этапа, включая отказ и блокировку. Запуски модели и коммиты регулирует [agents.md](agents.md); аудит не является разрешением на запуск.

## Старый roadmap (исторический снимок до аудита)

P0 был отмечен выполненным; P1: overlap SSD→RAM→GPU, aligned/coalesced reads, pinned RAM, double/triple buffering; P2: hot cache; P3: router prefetch; P4: addressed aligned container; P5: fused dequant+GEMM Ada/decode; P6: MTP/ngram/draft/combined; P7: residency planner; P8: mixed quant; P9: REAP; P10: multi-agent batching; P11: autotuning/API. Все P1–P11 имели незакрытые checkbox. Critical path был сформулирован как P0 → true H2D/compute overlap. Этот порядок заменён очередью R0–R6 выше; экспериментальная история не удалена.
