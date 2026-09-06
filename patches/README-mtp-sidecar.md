# MTP sidecar patch — применение

`mtp-sidecar.patch` добавляет split-MTP режим (draft-модель только с MTP head, trunk без MTP-весов) к expert-tier базе. Он не самодостаточен: применяется поверх интеграционного патча и его drift-дополнения.

## Порядок

```sh
git clone <upstream-llama.cpp-expert-tier> work/llama.cpp-expNNN
git -C work/llama.cpp-expNNN checkout 4aaad5d318a790a42c2197975ec8fadbad42602b
git -C work/llama.cpp-expNNN apply ../patches/expert-tier-integration.patch
git -C work/llama.cpp-expNNN apply ../patches/integration-drift.patch
git -C work/llama.cpp-expNNN apply ../patches/mtp-sidecar.patch
```

Каждый этап проверять `git apply --check`. Эксперименты — только в изолированной копии; рабочие деревья `work/llama.cpp-integration` и `work/llama.cpp-exp023` не изменять.

## Происхождение

- `integration-drift.patch` — расхождение опубликованного интеграционного патча с рабочим состоянием на момент форка (уточнение EHS autofit в `common/common.cpp` и `src/llama-expert-hotstore.cpp` плюс перевод комментариев). Без него autofit-пути не совпадают с действующим сервером.
- `mtp-sidecar.patch` — форк `work/llama.cpp-exp023`, ветка `exp023-mtp-sidecar`, коммиты `aaff9b3d5` (порт MTP sidecar из cafe-llama.cpp) и `c40681659` (полностью CPU draft KV). Пользовательские comment edits не включены.

## Проверка эквивалентности (EXP-034, PASS)

`base + integration + drift + mtp` побайтово совпадает с рабочим деревом `work/llama.cpp-integration`, кроме одной пустой строки в `src/llama-context.cpp:487` (нелогическая правка, намеренно не доставляется). Сборка конфигурацией из `benchmarks/manifests/exp034-provenance.json` проходит; `llama-server --help` идентичен живому бинарнику.
