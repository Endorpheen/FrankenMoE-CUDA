# Шаг 1 к единому Goose GPU-рантайму: композитный артефакт из пяти канонических наборов

Дата: 2026-11-15
База: commit 29a930e6 (`tools: compose runtime with the schema converter`)
Рабочий каталог: `/tmp/opencode/sub-621` (репозиторий не менялся)
Копия базы для сборки: `canon/` (получена `git archive 29a930e6`, чтобы ушедший
вперёд HEAD не влиял на результат)

Инструменты: `goose runtime 1e714e6146bc`
(`/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose`), `gcc 15.2.0`,
`python3 3.14`, GNU binutils (`objcopy`, `nm`, `ldd`).

## Итог

| показатель | значение |
|---|---|
| наборов собрано | 5 / 5 |
| shared objects | 5 наборов + 2 вспомогательных (`rt`, `quantprims`) + 1 единый |
| duplicate exported symbols после скоупинга | **0** |
| duplicate definitions до скоупинга | 2 (`goose_init` ×5, `json_parse` ×2) — объяснены ниже |
| smoke-host | 5 / 5 наборов, 0 падений, `rc=0` |
| dlopen каждого набора | 5 / 5, `rc=0` |
| неразрешённых символов (`ldd -r`) | 0 |
| единый артефакт `libgoose_unified.so` | линкуется с `-Wl,--no-undefined`, 305 экспортов |
| production runtime | не тронут, модели не запускались |

## Как устроена сборка (`build.sh`)

Для **каждого** набора:

1. `tools/compose-goose.py` — композиция канонических модулей в один
   translation unit (тот же композитор и тот же порядок модулей, что в
   `tools/check-composition.sh` базы).
2. `goose --standalone -O2 -o gen/<set>.c --header gen/<set>.h src/<set>.goose`
   — C-код и заголовок экспортируемого ABI (`build/gen/<set>.h`).
3. `gcc -O2 -std=c11 -fPIC -c` — PIC-объект.
4. Скоупинг символов: экспорт `goose_init` переименовывается в
   `goose_init_<set>` (у каждого набора свой рантайм), экспорт, уже
   заявленный предыдущим набором, становится локальным (`objcopy
   --localize-symbols`). Тела копий идентичны — это гарантирует композитор,
   и это проверено хешами (см. ниже).
5. `gcc -shared ... -Wl,--no-undefined` — `.so` набора без собственных
   неразрешённых ссылок.

Затем:

6. `gcc -shared` из всех скоупингованных объектов + вспомогательных
   (`rt_hooks`, `quant_prims`, `native-tables`) — **один** артефакт
   `libgoose_unified.so`; линковка с `--no-undefined` сама по себе является
   проверкой отсутствия конфликтов определений.
7. Аудит дубликатов: `nm -g --defined-only` по объектам (до скоупинга) и
   `nm -D --defined-only` по `.so` (после скоупинга), плюс `ldd -r`.
8. Smoke-host линкуется с единым артефактом и вызывает по одной функции из
   каждого набора; режим `--dlopen` отдельно загружает каждый наборный `.so`
   через `dlopen(RTLD_NOW|RTLD_LOCAL)` и разрешает его вход и представителя.

## Наборы

| набор | модулей | строк C | `.so`, байт | экспортов |
|---|---:|---:|---:|---:|
| server | 6 | 38 979 | 306 800 | 80 |
| quant | 6 | 20 344 | 113 984 | 25 |
| gguf | 4 | 11 292 | 73 360 | 29 |
| p9-06 | 5 | 12 428 | 85 448 | 18 |
| jinja-schema | 2 | 417 692 | 1 245 088 | 99 |
| **итого** | **23 файла / 21 модуль** | **500 735** | **1 824 680** | **251** |

Состав (в порядке композиции):

- server: `server/jsonparse`, `server/mcp-core`, `server/mcp-stdio`,
  `server/decision-core`, `server/server-schema`, `server/tools-pure`;
- quant: `model/quant-types`, `quant-allow`, `quant-rows`, `quant-rows2`,
  `quant-dequant`, `quant-dequant2`;
- gguf: `model/gguf-metadata`, `gguf-writer`, `gguf-tensors`, `gguf-split`;
- p9-06: `cli/cli-protocol`, `chat/thinking-control`, `chat/parse-params` плюс
  две копии `jsonparse` (`cli/`, `chat/`), которые композитор сводит к одной;
- jinja-schema: `jinja/runtime`, `json/format-grammar`.

Колонка «экспортов» включает вход рантайма `goose_init_<set>`; чисто
прикладной API набора на единицу меньше. `json_parse` входит в API только у
server: в p9-06 его копия локализована (см. ниже).

Единый артефакт: `libgoose_unified.so`, 1 769 000 байт, 305 экспортов =
251 API наборов + 6 хуков `gs_*` + 48 примитивов/таблиц quant. Пять
независимых рантаймов сосуществуют в одном `.so`: в нём ровно пять
`goose_init_*` и ровно один `json_parse`.

## Duplicate symbols

Композитор снимает дубликаты внутри набора. Между наборами остались ровно две
группы, обе разобраны скоупингом:

| символ | где определён | решение |
|---|---|---|
| `goose_init` | все 5 наборов | переименован в `goose_init_<set>`; наборы — независимые рантаймы |
| `json_parse` | server и p9-06 | экспортируется из server; в p9-06 копия локализована |

Копии парсера байт-идентичны, поэтому локализация не меняет поведение:

```
ee56f4e479dd05b65f57589ca4916767d923d13e73187087a95ab27416bfd70e  goose/json/json-parser.goose
ee56f4e479dd05b65f57589ca4916767d923d13e73187087a95ab27416bfd70e  goose/server/jsonparse.goose
ee56f4e479dd05b65f57589ca4916767d923d13e73187087a95ab27416bfd70e  goose/cli/jsonparse.goose
ee56f4e479dd05b65f57589ca4916767d923d13e73187087a95ab27416bfd70e  goose/chat/jsonparse.goose
```

Проверки (`build/log`, `build/gen`):

- `build/gen/raw-duplicates.tsv` — дубликаты **до** скоупинга: 2 строки
  (перечислены выше);
- `build/gen/dyn-symbols.tsv` — динамические символы всех наборных `.so`;
  дубликатов: **0**;
- `build/gen/*.scoped` — что именно локализовано (только `json_parse` в p9-06);
- линковка `libgoose_unified.so` с `-Wl,--no-undefined` прошла без
  multiple-definition;
- `ldd -r` по каждому наборному `.so`: неразрешённых символов нет.

## Smoke-результаты

`./build/smoke` (через единый артефакт), вывод дословно:

```
server: mcp_config_lines -> 59 bytes
server: alpha|alpha-srv|/tmp|30000||K=V
zeta|zeta-srv||1500|--a,b|
quant:  qz_blck(Q4_0)=32 qz_type_size=18 qz_row_size(256)=144
quant:  qz2_row_size(Q4_K,256)=144 qz_dequant_block_size=18 qz_row_size(Q8_0,256)=272 qd_dequant_elems=0
gguf:   s2_write(version=3, alignment=32, kv=0) -> 32 bytes, magic=GGUF
p9-06:  cli_read_line rc=0 kind=0 id=7 n_predict=16 think=1 prompt=hello
jinja:  json_schema_convert("string") -> 135 bytes
jinja:  char ::= [^"\\\x7F\x00-\x1F] | [\\] (["\\bfnrt] | "u" [0-9a-fA-F]{4})
jinja:  role_to_string(role_from_string("user")) -> 4 bytes: user
smoke:  5/5 sets ok through libgoose_unified.so
```

`./build/smoke --dlopen`, вывод дословно (адреса у каждого запуска свои):

```
dlopen: server        ok  0x... mcp_config_lines=0x...
dlopen: quant         ok  0x... qz_row_size=0x...
dlopen: gguf          ok  0x... s2_write=0x...
dlopen: p9-06         ok  0x... cli_read_line=0x...
dlopen: jinja-schema  ok  0x... json_schema_convert=0x...
dlopen: 5/5 sets loaded
```

Что именно вызвано: разбор MCP-конфига (server), геометрия блоков Q4_0/Q8_0
и Q4_K (quant), запись пустого GGUF-заголовка v3 (gguf), чтение строки
session-протокола (p9-06), конвертация JSON-схемы `{"type":"string"}` в GBNF и
обратное отображение имени роли (jinja-schema). Ни одной модельной или сетевой
операции.

## ABI-швы, которые пришлось закрыть

Канонические наборы рассчитаны на хост, который даёт им примитивы (в гейтах это
C++-фикстуры). Для единого артефакта эти примитивы собраны в две маленькие
библиотеки:

| библиотека | символов | что даёт |
|---|---:|---|
| `libgoose_rt.so` | 6 | `gs_json_strtod`, `gs_json_fmt_e`, `gs_json_fmt_g`, `gs_frexp_mant`, `gs_to_string_f64`, `gs_strftime_epoch` |
| `libgoose_quantprims.so` | 48 | load/store, `fabs32/sqrt32/log2f/floorf/lroundf/ldexpf`, `bits/of_bits`, `nearest`, scratch, `sort16`, IQ grid/map/neighbour и `ggml_abort` |

Таблицы IQ не литералы: они строятся нативным `ggml_quantize_init`. Они
извлечены тем же скриптом, что и в принятом гейте
(`experiments/EXP-2026-11-14-611-p7-04-quant-rows2/tests/make-tables.py`) из
`ggml/src/ggml-quants.c` эталонного дерева
(`/home/end0/AI/LlamaServer/llama.cpp-flint-cuda`, не
`work/llama.cpp-integration`), файл `build/gen/native-tables.c`.

Это соответствует разделу Canonical composition в
`docs/goose-migration/P7-08-COVERAGE.md`: шов `arena_gbnf -> json_schema_convert`
и слияние jinja `value/builtins` остаются отдельными шагами и здесь не
затрагивались.

## Как запускать

```bash
cd /tmp/opencode/sub-621

bash build.sh pin 29a930e6   # при необходимости: заново зафиксировать базу в ./canon
bash build.sh                # полная сборка + аудит дубликатов + smoke
./build/smoke                # smoke через единый артефакт
./build/smoke --dlopen       # загрузка каждого наборного .so
bash build.sh clean          # удалить build/
```

Переменные: `FM_ROOT` (каноническое дерево, по умолчанию `./canon`, иначе
репозиторий), `GOOSE`, `CC`, `OUT`, `FG_LLAMA` (дерево с `ggml-quants.c`),
`MAKE_TABLES`.

Время чистой сборки на этой машине: 5 мин 09 с (почти всё — компиляция
`jinja-schema.c`, 417 692 строки). Повторная чистая сборка даёт те же размеры и
те же хеши.

## SHA-256 артефактов

```
f1dbfd36b99325be0dcabf23bc15193399601da643470bde5ef843492887759a  libgoose_gguf.so
290884db29eab3e6dcabdebeb4d6ffca71ce82f7474f4aa8f0631ffc8b0bce9a  libgoose_jinja-schema.so
06767183edc1be8583e2b71b5c6e0a1dedab5f66c8e99cf83a5834f10bd27f0d  libgoose_p9-06.so
d894d2e3c062f32e521067f6fa2bf4c8cb70f3fcc4b6b0a4ff06c046fe89e8a3  libgoose_quantprims.so
42863ae129bbd783b281e44d45a6c176dadf50586b0826d4a2653b1e8bc379d8  libgoose_quant.so
88e4e9140e220f52ab34d499f85bc5bb530fea4c11789b81ebb5e9f07f81d8e9  libgoose_rt.so
1d2272ad238270d81ce1218d90b7b20206d28a7a67b972b0d515628d18589add  libgoose_server.so
5c32592f2d44857a0402189b5925e7a770b0e6aedf6e229900f06f7e5b1bfce3  libgoose_unified.so
```

Оговорка о воспроизводимости: сгенерированный C содержит строку
`gs_file0 = "<OUT>/src/<set>.goose"`, поэтому хеш `.so` зависит от пути `OUT`.
При том же `OUT` (`/tmp/opencode/sub-621/build`) сборка побайтно повторяема:
чистая пересборка дала те же восемь хешей (`build.final.log`). Хеши выше — от
сборки в этом каталоге.

Совместимость с текущим деревом: во время работы репозиторий ушёл вперёд
(HEAD d8d59ecc). Все 21 канонический модуль в базе и в HEAD байт-идентичны, а
прогон скрипта против рабочего дерева HEAD тоже прошёл: 5/5 наборов, 0
дубликатов, те же счётчики экспортов. Побайтовые хеши там отличаются, потому
что композитор HEAD дополнительно переименовывает часть локальных
идентификаторов (`fail`, `cp_escape`); экспортируемый ABI при этом не меняется.

SHA-256 скомпонованных исходников (`build/src/*.goose`):```
00c4f967b41bd1e74c1ca3db279c74c660e0e5c0fc3ad27e69abbbbafd8c662b  server.goose
d13f558b78947cd06f56586ca8a57f7e27d473c28a1b6ce745b76d6a5c54b453  quant.goose
5a462e698f8e53b4d6d9d5c88b851e22ed4892158167d774831166366d1de20f  gguf.goose
0ffca2dd085c6e71de2b56bd8f9f01745c15a2036567042246f2b5332f18850e  p9-06.goose
7396a6dd18f5de4a5d126569b12323f9cba854e9f59f63bc42d9630b86d0655e  jinja-schema.goose
```

## Что проверено и что нет

Проверено: композиция всех пяти наборов (и `goose check`, и `--standalone`),
сборка в `.so`, линковка единого артефакта, отсутствие дубликатов и
неразрешённых символов, загрузка и вызов по одной функции из каждого набора,
побайтовая воспроизводимость при фиксированном пути.

Не проверено и не заявляется: производительность (никаких замеров), корректность
quant-путей за пределами геометрии строк (таблицы IQ собраны и слинкованы, но
smoke их не гоняет — это работа отдельного шага), слияние jinja
`value/builtins`, шов `arena_gbnf`, поведение production-рантайма, работа с
моделью.

## Файлы

```
build.sh                 сборка, скоупинг, единый артефакт, аудит, smoke
src/rt_hooks.c           6 хуков gs_*
src/quant_prims.c        48 примитивов quant (+ ggml_abort)
src/smoke_server.c       вызов mcp_config_lines
src/smoke_quant.c        вызовы qz_* / qz2_* / qd_*
src/smoke_gguf.c         вызов s2_write
src/smoke_p9-06.c        вызов cli_read_line
src/smoke_jinja-schema.c вызовы json_schema_convert и role_*
src/smoke_main.c         хост: единый артефакт + режим --dlopen
canon/                   дерево базы 29a930e6 (git archive)
build/                   генерённый C и заголовки, объекты, .so, smoke, логи
build/inventory.tsv      таблица наборов
build/gen/raw-duplicates.tsv  дубликаты до скоупинга
build/log/SHA256SUMS     хеши артефактов
```
