# Технический аудит архитектуры FrankenMoE — отчёт о чтении кода (строго чтение)

## 0. Методология

Аудит выполнен только инструментами чтения (`ReadFile`, `shell` с `git log`, `grep`, `ls`). Ни один файл не модифицирован, сборки, тесты, коммиты и серверы не запускались. выполнено 15 отдельных операций чтения и поиска по `goose/`, `runtime/`, `tools/`, `integration/`. Все пути и названия функций приведены как в репозитории. Текущий commit в дереве: `9c22de02`.

## 1. Общая структура

Репозиторий — это два параллельных.runtime, соединённых через сгенерированный C и FFI-мосты:

- `goose/` — 931 canonical-файл (4266 зафиксированных записей MANIFEST, включая `_archive` из 3335 файлов). Модули сгруппированы по подсистеме, а не по эксперименту: `goose/sampling/`, `goose/model/`, `goose/kv/`, `goose/server/`, `goose/cpu/`, `goose/cuda/`, `goose/misc/`. `goose/misc/runtime-facade.goose` — это переписанный на Goose `runtime.cpp` (`run()`, `session_config_from`); он показывает, что хост-слой всё ещё существует как эталон для сопоставления.
- `runtime/reconstructed/` — восстановленный C++-хост: `src/llama-*.cpp` (59 файлов) plus `build.py`, который до сих пор содержит `'llama-sampler.cpp'` в списке объектов.
- `tools/build-main-runtime.sh` — механизм накопления: список `MODULES` (118 модулей) компилируется в один переводной единиц (`build/goose-main/{main.goose,main.c,main.h,main.o}`) на gcc-13. Он не линкует хост и не запускает модель. `docs/goose-migration/BUILD-SET.md` — сгенерированный рекорд того же списка.
- `integration/modules/` — живые мосты типа `EXP-2026-11-11-384-goose-sampler-softmax-live`, подключающие Goose-решения к старым C++-путям.

Сборка построена на Policy: семь SONAME (`llama`, `llama-common`, `mtmd`, `ggml`, `ggml-base`, `ggml-cpu`, `ggml-cuda`), регистр: 307 экспортов, 165 деклараций, 948 импортов — всё зафиксировано в `goose/misc/soname-exports.goose`.

## 2. Токенизатор — полностью на Goose на уровне решений

Цепочка: `bpe-load` → `bpe-tokenize` → `tokenizer-decisions`.

`goose/model/bpe-load.goose` (`btl_load`, экспорт) — единственное место, которое соединяет чтение GGUF с движком: находит три ключа (`tokenizer.ggml.tokens/.merges/.token_type`), разбивает массив пар, вызывает `bt_build_impl`, затем проход атрибутов, `bt_special_order_impl`, `bt_eog_ids_impl`. Он никогда не аллоцирует: всё caller-owned, а стадии сбоев報告руются через `stats[6]`.

`goose/model/bpe-tokenize.goose` содержит весь движок: `bt_bpe` (экспорт, точка входа с фиксированными рабочими буферами `ids/sym/heap/hc/info`), `bt_word` (построение цепи символов, очередь слияний — бинарная куча, упорядоченная по `(rank, left index)`, эмит ID), `bt_token_id` и `bt_merge_rank` (отдельные хеши `bt_hash`/`bt_pair_hash`, открытая адресация, `bt_eq2` сравнивает по содержимому). Два поведенческих правила зафиксированы как инжиниринг: `token_to_id` — последний ID выигрывает; `bpe_ranks` — первое определение выигрывает, последующие игнорируются. `bt_special_hits` находит (start, end, id) тройки, `bt_attrs` выполняет четыре прохода атрибутов в native-порядке (FIM-классы → "unused" → EOG-тексты → gpt-oss override).

`goose/model/tokenizer-decisions.goose` переносит решения из `src/llama-vocab.cpp`: `tk_norm` (marker 202, escaping space as U+0120), `tk_dp_step` (score-path DP), `tk_wpm_pick` (marker 201), `tk_spm_merge` (marker 203), `tk_resegment` (marker 204), `tk_utf8_byte` (восстанавливает payload bits, а не просто маркеры).

Риск: `bt_bpe` — host-allocated fixed-size work arrays (`sym` needs `n*4`, `heap` needs `(nwords+1)*5`): переполнение невозможно только если хост считает их правильно; ошибка размера возвращается как -1 с `info[4]` стадией. Также зависимость порядка слияния от exact heap comparator — при изменении toolchain нужно пере-валидировать.

## 3. Сэмплер — два уровня: решения на Goose, тело всё ещё C++

`goose/sampling/sampler-chain-full.goose` — самый объёмный модуль (1634 строки). Он реализует полную цепь от `sm_select`: MT19937 (`sm_mt_seed`/`mt_twist`/`sm_mt_next`, two 32-bit draws combined as `(r1 + r2*2^32)/2^64`), `sm_partial_sort` — точная копия heap primitives libstdc++ (`__push_heap`/`__adjust_heap`/`pop_heap_to`/`sort_heap` с тем же comparator `logit > logit`, потому что layout heap-select решает, какие equal-logit candidates выживают), `softmax_in_place` (f32 `expf`, f64 accumulation), temperature, и `dist` — final selection.

Отдельные шаги (`sm_apply_top_k`, `sm_apply_top_p`, `sm_apply_min_p`, `sm_apply_temp`, `sm_apply_typical`, `sm_apply_xtc`, `sm_apply_penalties`, `fg_sampler_dry` — с `sm_dry_rep_limit`, `sm_dry_repeat_counts` (reverse Z-algorithm) и `sm_apply_dry`) экспортированы как двери с marker-контролем: `fg_sampler_top_p` (marker 171), `fg_sampler_xtc` (172), `fg_sampler_adaptive_p` (174), `fg_sampler_mirostat` (175), `fg_sampler_dist` (176), `fg_sampler_dist_words` (177), `fg_sampler_dist_pick` (178), `fg_sampler_temp_ext` (179). Marker — это защита от вызова не той функции через FFI: `if marker != X { return -1; }`. Также здесьportирован grammar automaton (`g_advance`, `g_reject`, `g_reject_multi`, `g_match_char`, `g_match_partial`) — порт из `llama-grammar.cpp`, gating controlled в EXP-2026-11-18-1110.

**Главный артефакт аудита:** `runtime/reconstructed/src/llama-sampler.cpp` до сих пор компилируется (`build.py:55`) — реализация `llama_sample_dist`, `llama_sampler_temp_impl`, `llama_sampler_softmax_impl`, `llama_sampler_top_k_impl` и т.д. есть и на C++. Кто вызывает — кого: это решается на уровне хоста (`integration/modules/` bridges + `tools/build-main-runtime.sh` module list). Если оба скомпилированы в один бинарь, решает symbol resolution, а не намерение. `runtime/reconstructed/src/llama-context.cpp:354-359` и `:2090`, `:2705` всё ещё вызывает `llama_sampler_chain_*`/`llama_sampler_backend_*` — обёртка вокруг native sampler interface. Правило 25 (correctness gate per port) здесь особенно актуально: два работающих тела существуют одновременно, и сравнение их чисел без указания активного тела — это и есть та ошибка, которая уже стоила дня охоты за несуществующей регрессией.

Отдельные вызовы: `goose/sampling/goose-sampler-*.goose` (chain-live, dist-pick-live, filters-guard-live, guards-live, thresholds) — дополнительные gate'ы, а `sampler-chain-full.goose` — compose-able decision.

## 4. KV-кэш — решения на Goose, host-слой на C++

`goose/kv/goose-kv-update.goose` — перенос `llama_kv_cache::update` (P5-09). Уникален тем, что не имеет своего FFI, а работает через packet с `extern "fg329k_get/put/ld_i64/stw/fail"` — доступ к GPU-памяти на уровне инструкций backend'а. Op 0 — stream copies (K/V на каждый layer), затем shift pass, контролируемый `can_shift` (Step35 и multi-position models — hard abort, эмулированный как return code -7). При fail'е alloc/compute shift graph'а возвращается текущий флаг `updated` — намеренно, host должен интерпретировать. Op 1 — can_shift query, op 2 — has_shift query. records — (kind,a,b) triples in a host buffer.

Вокруг этого лежат policy-модули: `goose-kv-dsv4-comp-plan.goose`, `goose-kv-find-slot.goose`, `goose-memory-hybrid-idx-*.goose` (context, init, inputs, kpool, ops, poolstate, stale, state), `goose-recurrent-*.goose` (state read/write, seq ops, removal, clear), `goose-kv-cpy-plan.goose`, `goose-kv-pos-maintenance.goose`.

C++-сторона: `runtime/reconstructed/src/llama-kv-cache*.h/.cpp`, `llama-memory-hybrid.cpp`, `llama-memory-recurrent.cpp` — RAII, state model, device allocation. Т.е. C++ всё ещё держит "обёртку и состояние", а Goose — "решения и операции". Потенциальный риск: packet layout — контракт, который обе стороны понимают как int64 slots; Goose не проверяет границы packet'а, поэтому любая ошибка на C++-хосте при заполнении packet'а проявляется как false success/false failure, а не corruption.

## 5. Серверный runtime — гибридный

`goose/server/` (65 файлов): `decision-core.goose` (2997 lines — маршрут, limits, SSE, batch decisions), `server-schema.goose` (1436), `jsonparse.goose` (1074), `tools-pure.goose` (1052), `console-editing.goose` (927), `hot-reload.goose` (724), `download-selection.goose` (810), `mcp-stdio.goose` (819), `mcp-core/mcp-integration`, `http-listen/http-request/http-response`, `sse-framing`, `stream-manager`, `model-manager`, `route-table`, `prerouting`, `query-transport`, `request-errors`, `required-fields`, `response`, `api-key-policy`, `cors-policy`, `asset-etag`, `http-url-policy`, `mime-table`, `hf-cache-naming`, `limits`, `log-policy` (702 lines), `shutdown-policy`, `startup-policy`, `goose-checkout-policy`... — всё это всё ещё компилируется *внутри* C++-хоста как часть composed unit'а.

`goose/misc/runtime-facade.goose` — самое важное связующее: `rts_config` (12-field RunConfig → SessionConfig projection), `rts_n_batch` (n_ctx), `rts_ubatch`, `rts_plan` (5 step codes), `rts_request`, `rts_result`. Это доказывает, что каждый шаг `run()` воспроизведён и может быть вызван независимо. Аналогичные doors существуют для `llama-bench report`, `tokenize-cli`, `fit-params-line` (`goose/cli/`).

Правило 33 (один кумулятивный бинарь) проявляется здесь строже всего: `check-accepted-components.py` должен находить символы/строки каждой accepted component'ы в собранной библиотеке, потому что скомпилировать можно оба бинаря по-разному (именно это и вызвало путаницу с decodable ~10 tok/s difference в истории).

## 6. C++-мосты, которые остаются

1. `llama-sampler.cpp` — body for the sampler still compiled; the decision is host-side.
2. `llama-context.cpp` — sampler wiring, state get/set, `needs_raw_logits`.
3. `llama-kv-cache*.cpp/h`, `llama-memory-hybrid/recurrent` — state model + allocation.
4. `llama-graph.cpp`, `llama-batch.cpp`, `llama-arch.cpp`, `llama-adapter.cpp`, `llama-chat.cpp` — model/graph/batch host layer (`vendor/flint/src` vs `runtime/reconstructed/src` split — must stay multi-architecture, rule 17).
5. Server HTTP layer — still C++ `llama-server.cpp`-style host; Goose holds decisions only.
6. Unicode tables и common strings — `patch_unicode_modules.py` в `runtime/reconstructed` всё ещё патчит C++-источники, а не заменил их Goose-данными (rule 27).

## 7. Потенциальные риски, увиденные в коде

- **Два тела для одной возможности.** sampler, tokenizer, KV-update — всё имеет и C++ body, и Goose body. Кто активен — решается externals. до `build.py`/module list. При сходе двух symbol names in one link the linker, not the architecture, decides.
- **Marker-protected FFI.** `marker != X → return -1` — это вся защита типа. Она работает, но только если каждый caller проверяет return code; Ignoring it means calling a door for the wrong function and silently getting -1, then misinterpreting it.
- **Heap-exactness coupling.** sampler port Mirrors libstdc++ `__adjust_heap` internals. Это работает с gcc-13 сегодня; при смене toolchain or libc++ the whole chain needs re-validation, because the heap layout is a behavioral invariant, not an API.
- **Один composed TU.** 118 modules → main.c. Any duplicate symbol breaks the entire unit; a per-module build is the failure mode the rules explicitly forbid. compile times grow with each added module.
- **Packet FFI.** `fg329k_get/put` — no bounds checking on either side. The packet layout is the only contract.
- **Fixed host buffers.** `bt_bpe` requires the host to size `sym/heap/hc` correctly; `btl_load` requires all tables provisioned before anything is written (stats[6] names the stage).
- **Shift-graph failure semantics.** on alloc/compute failure the update returns "updated unchanged" — the host must not treat a false return as success.
- **Кумулятивный бинарь.** Правило 33: каждый rebuild must carry all previously accepted components; check via symbols/strings/flags (STRINGS that only the feature emits, env/CLI flags that disable it).

## 8. Вывод

Архитектура — это два слоя: C++-хост (`runtime/reconstructed`, 59 files) держит RAII/state/allocation/HTTP, а `goose/` — все решения (токенизация, сэмплер, KV-update, роутинг, MCP, CLI). Взаимодействие идёт через compose-able Goose modules, сгенерированный C и marker-guarded exports. Степень готовности: 115 DONE / 23 PARTIAL / 14 TODO из 152 требований (174/174 slice front). Критическая дыра сегодня — `llama-sampler.cpp` до сих пор скомпилирован и конкурирует с Goose sampler'ом; разделение ответственности (`llama-sampler.cpp` → dead code) — это следующий шаг, который должен быть проверен gate'ом до удаления.
