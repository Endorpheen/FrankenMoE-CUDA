# APEX-47: эталонный комплект, 47,64 токена/с

**Historical reference.** This 47.64 tok/s checkpoint belongs to the earlier hybrid runtime. See [current results](docs/CURRENT_STATE.md).

Зафиксирован рабочий запуск FrankenMoE, показавший лучший результат проекта. Всё необходимое для воспроизведения
лежит в одном каталоге и не ссылается на внешние экспериментальные деревья.

## Результаты

| запрос | decode, ток/с | prefill, ток/с |
|---|---|---|
| 1 | 47,34 | 2317,0 |
| 2 | 47,32 | 2515,0 |
| 3 | **47,64** | 2508,6 |

- SHA-256 ответа: `c2ff42cc1219` (правильный эталонный ответ)
- VRAM во время работы: 11 317 МиБ из 12 282
- Предыдущий рекорд: 45,68 ток/с

## Расположение

```
/home/end0/AI/LlamaServer/apex-47-reference/
  bin/llama-server
  lib/   libllama.so.0, libggml.so.0, libggml-base.so.0, libggml-cpu.so.0,
         libggml-cuda.so.0, libllama-common.so.0, libmtmd.so.0,
         libllama-server-impl.so
  logs/server.log
  responses/t1-1.json, t1-2.json, t1-3.json
  SHA256SUMS
```

Объём 236 МБ. Символических ссылок нет — только реальные файлы, проверено `find -type l` = 0.
Зависимости вне каталога: только системные (`libc`, `libm`, `libstdc++`, `libgcc_s`, `libssl`, `libcrypto`,
`libgomp`, `libcudart.so.12`).

## Хеши

| файл | SHA-256 (первые 16) | размер, байт |
|---|---|---|
| bin/llama-server | `badbfc5cf63895a9` | — |
| lib/libllama.so.0 | `77c3f17b37bb0bfa` | 5 638 456 |
| lib/libggml.so.0 | `9a9ea86ac80e725b` | 80 624 |
| lib/libggml-base.so.0 | `e213fa19e1697a37` | 1 192 112 |
| lib/libggml-cpu.so.0 | `63ad4984881220e5` | 1 971 176 |
| lib/libggml-cuda.so.0 | `219af97578109e62` | 220 696 544 |
| lib/libllama-common.so.0 | `69edbc5fc5e2a646` | 6 537 800 |
| lib/libmtmd.so.0 | `099bdb69066ce52d` | 2 089 400 |
| lib/libllama-server-impl.so | `8c63bc5df6f9cb14` | — |
| модель Apex-35B-V1.0-Q4_K_M.gguf | `c3b92d94cd03a773` | 21 ГБ |

Модель **не входит** в бэкап: 21 ГБ, лежит отдельно, хеш записан выше.
Полный список контрольных сумм всех 13 файлов — в `SHA256SUMS`.

## Профиль

```
контекст 262144   MoE-кэш 4992 МиБ   batch/ubatch 4096/4096
потоки -t 8 -tb 16   --cpu-moe   KV q4_0   -fa on   -ngl all
```

## Обязательное условие запуска

Профилю нужно около 9 ГиБ свободной видеопамяти на старте. Проверка перед запуском:

```
nvidia-smi --query-gpu=memory.used,memory.total --format=csv,noheader
```

Если занято больше ~4 ГиБ — прогон упадёт с `cudaMalloc failed: out of memory` при резервировании буфера графа,
причём **уже после загрузки модели**, что выглядит как ошибка кода, но ею не является. Так было потрачено несколько
часов на ложную диагностику. При 3 466 МиБ занятых прогон прошёл и занял 11 317 МиБ.

## Запуск одной обычной командой

```bash
nvidia-smi --query-gpu=memory.used,memory.total --format=csv,noheader
ss -ltn | grep -E ':9101' || true

LD_LIBRARY_PATH=/home/end0/AI/LlamaServer/apex-47-reference/lib \
/home/end0/AI/LlamaServer/apex-47-reference/bin/llama-server \
  -m /home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf \
  --alias Apex-35B-V1.0 \
  -c 262144 -ngl all -fit off --cpu-moe -fa on -ctk q4_0 -ctv q4_0 \
  -b 4096 -ub 4096 -t 8 -tb 16 \
  --cpu-mask 0f0f --cpu-mask-batch ffff --cpu-strict 1 --cpu-strict-batch 1 \
  --poll 0 -np 1 --cache-ram 0 --ctx-checkpoints 32 --load-mode mmap \
  --spec-type none --jinja --temp 1 --top-p 0.95 --top-k 20 --min-p 0 \
  --host 127.0.0.1 --port 9101 --moe-cache-mib 4992
```

Рабочий каталог значения не имеет: все пути в команде абсолютные.
Один сервер за раз. Второй запуск на занятом порту даёт OOM, потому что делит видеопамять с первым.

Запрос для проверки: `POST http://127.0.0.1:9101/completion` с телом `/tmp/req512.json`
(32768 входных токенов, 512 предсказанных). Ожидаемый SHA ответа — `c2ff42cc1219`.

## Что не проверено

- Ответ записан как `c2ff42cc1219`, это значение вычислено из полей `content` трёх ответов;
  независимая проверка полного текста не проводилась.
- Воспроизведение с нуля после перезагрузки машины не выполнено.

## Проверка целостности бэкапа

```bash
cd /home/end0/AI/LlamaServer/apex-47-reference && sha256sum -c SHA256SUMS
```
