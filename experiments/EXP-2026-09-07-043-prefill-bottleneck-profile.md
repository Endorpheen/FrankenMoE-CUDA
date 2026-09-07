# EXP-2026-09-07-043 — что ограничивает prefill после pinned ring (ПРОТОКОЛ, PLANNED)

Статус: `PLANNED` — протокол диагностики. Запуск сервера/модели/profiler НЕ одобрен;
после этого документа следует отдельный блок согласования (цель, длительность, число
запусков) по правилу 1 AGENTS. Сам протокол ничего не измеряет и не ускоряет.

## Вопрос

После принятия pinned ring (EXP-038..041, default-on) часть времени prefill всё ещё
уходит на обслуживание множества отдельных H2D-передач или на что-то ещё. Нужно
различить три возможных ограничения на текущем рантайме EXP-041 для сохранённого
395-token запроса:

1. CPU занят копированием весов в pinned slots (gather `memcpy` в `expert_ring_slots`).
2. GPU ждёт постановки передач/событий или освобождения slot (ожидание `cudaEventSynchronize`,
   проселлы GPU из-за отставания submission).
3. Передачи упираются в пропускную способность PCIe (H2D near ceiling, headroom нет).

Решение определяет действие: если заметную задержку даёт обслуживание МНОЖЕСТВА передач
(случай 2, привязанный к числу вызовов) — проверяем limited-gap A (≤512 KiB, EXP-042).
Если преобладает работа с байтами (случай 1 gather или случай 3 PCIe) — limited-gap
остаётся припаркованным (он добавляет байты и gather). Если задержка в другом месте —
получаем конкретную цель следующей оптимизации.

## Пред-флайт (уже выполнен offline, без сервера/модели)

Проверено на модель-free харнессе EXP-041 (`results/archive/EXP-2026-09-07-041/tests/test-exp041-ring`,
реальный вызов `set_tensor_async_pinned_ring`) под `nsys profile -t cuda --sample=cpu`:

- nsys CUDA tracing ВИДИТ: H2D `Memcpy HtoD` (`CUPTI_ACTIVITY_KIND_MEMCPY`, 52 копии),
  API-события `cudaMemcpyAsync`/`cudaEventRecord`/`cudaEventSynchronize`/`cudaStreamSynchronize`
  с длительностями и счётчиками. Значит случаи 2 и 3 различимы по GPU-таймлайну.
- nsys `--sample=cpu` НЕ дал таблицы CPU-сэмплов (пусто). Причина: `perf_event_paranoid=4`
  (`/proc/sys/kernel/perf_event_paranoid`). Тот же барьер ломает и `perf`
  (`Failure to open any events`). Значит CPU gather (`memcpy` в slot) профилями CPU-sampling
  в этом окружении НЕ измерить. Это ответ на требование «выяснить до модельного запуска».

Вывод пред-флайта: для GPU-стороны берём nsys; для CPU gather нужен либо минимальный
диагностический счётчик (рекомендуется), либо снижение `perf_event_paranoid` (системная
правка, требует согласия Игоря). Профиль выбран корректно до любых запусков.

## Инструменты и что снимать

A. nsys CUDA tracing (GPU-сторона; подтверждено что работает):
   `nsys profile -o exp043prefill -t cuda --force-overwrite true bash scripts/run_qwen38_server.sh`
   затем одиночный сохранённый 395-token запрос (`.../exp032-h2d-ts/request.json`, sha
   `0b7524b3…`), SIGINT. Экспорт `nsys export --type sqlite`. Поля внутри окна запроса:
   - H2D: count, sum bytes, `union` длительность GPU `Memcpy HtoD` ->
     `ACHIEVED_BW = bytes / union_busy`; сравнить с потолком PCIe 4.0 x16 (~31,5 GB/s raw,
     практически ниже). EXP-037 (старый pageable) дал ~13,3 GB/s — не насыщение.
   - GPU idle: `WINDOW - union(вся GPU-активность)`; распределение проселлов.
   - API host union по именам: `cudaMemcpyAsync`, `cudaEventRecord`, `cudaEventSynchronize`
     (slot-wait), `cudaStreamSynchronize`.

B. Диагностические счётчики (CPU gather; минимальный патч в отдельный build `build/exp043-diag`,
   рабочий билд не трогается): в `ggml_backend_cuda_set_tensor_async_pinned_ring` накапливать
   на контексте `gather_ns` (`clock_gettime` вокруг `memcpy` в слот), `submit_ns` (вокруг
   `cudaMemcpyAsync`), `slot_wait_ns` (вокруг `cudaEventSynchronize` при переиспользовании слота);
   плюс существующие `calls`/`chunks`. Сброс по маркеру начала запроса (request-scoped), вывод
   в лог на teardown. Это прямой замер случая 1, недоступный профилю.

C. Кросс-чек CPU gather без изменения кода (только если Igor разрешит системную правку):
   `sudo sysctl kernel.perf_event_paranoid=1` затем `nsys profile ... --sample=cpu` либо
   `perf record -g` по потоку scheduler; доля `__memmove*`/`memcpy` в CPU времени потока
   подтверждает случай 1. По умолчанию НЕ применять (paranoid=4).

## Отделение прогрева от исследуемого запроса

- Профилировать сессию целиком, окно взять офлайн по burst'у экспертных H2D, ограниченному
  idle-разрывом (метод EXP-037: `WS`=первая H2D in-request burst'а после idle-разрыва,
  `WE`=последний kernel/copy). Warmup-граф-захват и загрузка модели — отдельные ранние
  burst'ы, отсекаются этим окном.
- Identity-check: число экспертных H2D в окне ≈ 17 886 (совпадение с EXP-032 побайтно),
  иначе окно неверно.
- Альтернатива для host-счётчиков: сессия = запуск, один warmup-запрос, сброс счётчиков по
  маркеру, затем ровно один измеряемый запрос; teardown печатает request-scoped итоги.

## Метрики и пороги (решение)

| Признак | Случай | Связь с limited-gap A |
|---|---|---|
| GPU H2D busy ≈ окно, `ACHIEVED_BW` ≳ 0,8·потолка PCIe, мало idle | 3 (PCIe) | A добавляет байты -> вреден |
| `gather_ns` доминирует в host-пути, GPU idle коррелирует с занятостью CPU `memcpy` | 1 (gather CPU) | A добавляет gather -> вреден |
| GPU idle велик; host не в `memcpy`; велик `cudaEventSynchronize`/slot-wait; много мелких вызовов | 2 (submission/events/slot) | A может помочь (меньше вызовов) |
| задержка вне H2D (например, MMQ kernels, dense, router) | прочее | новая цель, A не при чём |

Порог полезности: ориентир 3-5% prefill (с 7,24 с это ~0,22-0,36 с; 10% ~0,72 с). Цена
кандидата A — +12,57% байтов при прежнем ring 32 MiB; возможен обратный эффект.

## Правила запуска (будущий прогон, по правилу 1)

- Один профиль = один 395-token запрос после прогрева; сессии по одному, строго последовательно.
- Не смешивать clock domains (как EXP-037): nsys ns vs серверный относительный лог только
  по порядку burst'ов, не числовым слиянием.
- Артефакты в `results/archive/EXP-2026-09-07-043/`: `*.nsys-rep`/sqlite (локально, не коммитить
  бинарные), SQL-запрос окна (по образцу `EXP-037/analysis.sql`), `offline-results.txt`,
  логи счётчиков, `PROTOCOL.md`.

## Артефакты пред-флайта (offline, этот коммит)

- Профиль харнесса собран во временном каталоге (не в репозитории): подтверждение видимости
  H2D/event API и отсутствия CPU-сэмплов при `perf_event_paranoid=4`.

## Следующий шаг

Один блок согласования прогона (цель/длительность/число запусков) для EXP-043. Запуск не
предлагается одобрять вслепую — сначала этот протокол.
