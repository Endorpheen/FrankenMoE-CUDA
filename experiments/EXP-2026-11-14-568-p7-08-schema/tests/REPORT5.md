# REPORT5 — additionalProperties и интеграционный патч

Дата: 2026-11-14
База: commit `3c88b136`
Рабочий каталог: `/tmp/opencode/sub-573/`
Тяжёлых сборок не было: C, `goose --standalone`, fixture и тесты не
запускались. Выполнены только `goose check draft.goose` и
`goose check integration/format-grammar.goose`.

## 1. `additionalProperties` закрыт

Да, ветка закрыта полностью: `absent`, `false`, `true` и схема.

Нативная семантика:

1. Объектная ветка включается при `properties` **или** при
   `additionalProperties != true` (cpp 945-947). Поэтому:
   - `true` без `properties` уходит в object primitive (cpp 1071-1072);
   - `true` с `properties`, `false`, схема и отсутствие — строятся
     `_build_object_rule`.
2. В `_build_object_rule` (cpp 737-749):
   - значение: схема -> `visit(additional, "<name>-additional-value")`;
     `true` -> primitive `value`;
   - ключ: без свойств -> primitive `string`; со свойствами ->
     правило `_not_strings(prop_names)` под именем `<name>-additional-k`;
   - kv: `<name>-additional-kv ::= <key_rule> ":" space <value_rule>`;
   - в список optional добавляется `"*"` последним.
3. `get_recursive_refs` для `"*"` (cpp 765-796):
   - первый optional: `( "," space <kv> )*`;
   - после именованного optional: `<kv> ( "," space <kv> )*`.

Портировано:

- `object_branch` больше не отвергает `true`/схему;
- `object_from` получает `addk` и узел схемы (`meta[7]`), строит
  additional-value, additional-k/kv и добавляет `*` в optional-список;
- `opt_chain` знает индекс `*` и выдаёт обе нативные формы;
- `_not_strings` — trie-правило по именам свойств, с сортировкой символов,
  `char+` для конца имени, `[^"..."] char*` для reject-ветки и завершающим
  `?`, когда пустое имя не является концом;
- `char` primitive переиспользуется, ключ без свойств — primitive `string`.

Пары: cases.md №37-41 (additional schema; true; required+additional;
optional+additional; required+optional+additional; строки 924-1078).
Схемы фикстуры: добавлены `additional-notstrings` и `additional-true-props`,
итого **30**.

## 2. Интеграционный патч

Каталог `integration/`:

| файл | назначение |
|---|---|
| `base/format-grammar.goose` | свежая копия канона `goose/json/format-grammar.goose` (commit `3c88b136`) |
| `format-grammar.goose` | канон + только converter-блок |
| `format-grammar.patch` | unified diff базы и результата |
| `EXPORTS.txt` | список экспортов до/после |

Что перенесено:

- блок `json-schema-to-grammar` из `draft.goose`: конвертер, primitive-таблицы,
  pattern, bounds, allOf, `$ref`, union/type-array, tuple, format,
  `additionalProperties`, `_not_strings`;
- приватные хелперы блока.

Что **не** дублируется:

- `app`, `appc`, `appd`, `eqb`, `s_at` — переиспользованы из канона; локальные
  `append_s/append_c/append_dec/eq_bytes/str_at` удалены из блока;
- `grammar_text` — переиспользован для сортировки и печати; локальный `emit`
  удалён;
- `I64_MIN/I64_MAX` — взяты из канона, повторные объявления удалены;
- константы видов переименованы в `FG573_*`, чтобы не конфликтовать с
  каноническими `K_*` (у канона `K_TUPLE = 12`, у блока свой tuple-kind).

Проверка отсутствия дублей: 92 функции в итоговом файле, 0 повторяющихся имён.

### Экспорты до/после

До (4):

```
schema_build
grammar_text
conversion_failed
conversion_warning
```

После (5):

```
schema_build
grammar_text
conversion_failed
conversion_warning
json_schema_convert
```

### Совместимость export-сигнатур

Обнаружено ограничение Goose: export-функция, у которой одновременно есть
`const` slice-параметр и mutable slice-параметр, не специализируется при
вызове. Это воспроизведено на самом каноне: вызов `conversion_failed` или
`grammar_text` из другого export даёт
`could not resolve one concrete function signature`. Локальная копия той же
функции без `export` работает.

Поэтому в патче:

- `json_schema_convert` объявлен полностью с mutable slice-параметрами;
- у канонического `grammar_text` четыре `const i64[:]` и `const u8[:]` заменены
  на mutable; тело только читает эти срезы, поведение не меняется.

Проверка: копия итогового файла с дополнительным export-вызовом
`json_schema_convert` проходит `goose check` (265 специализаций). Без этой
правки вызов был бы невозможен, как у `grammar_text`/`conversion_failed`.

### Что осталось для сквозной интеграции

Патч переносит именно converter-блок. `json_schema_convert` принимает
документированный контракт `meta/props/vals/strs/sdir/...`, а канонический
`schema_build` заполняет другой набор массивов (`nkind`, `ncf`, `ncc`, ...).
Связка «выход `schema_build` -> вход `json_schema_convert`» — отдельный шаг;
патч её не выдумывает и не дублирует schema reader.

## 3. Покрытие

**22 / 22 ключей** покрыты, включая `additionalProperties` (`true`, `false`,
схема) без прежней оговорки. Пар: **41**. Схем: **30**.

Осталось только интеграционное: сквозная связка дерева `schema_build` с
контрактом конвертера (см. выше). Функциональных непокрытых ключей нет.

## 4. Проверка

```
$ goose check draft.goose
typechecked ok: 171 specialization(s), 2 struct/0 enum instance(s);
optimized -O1: 10 inlined, 0 base case(s), 22 folded, 617 propagated,
3 tail loop(s), 0 reassociated
bce: elided 309/767 index and 6/79 slice checks

$ goose check integration/format-grammar.goose
typechecked ok: 264 specialization(s), 2 struct/0 enum instance(s);
optimized -O1: 19 inlined, 0 base case(s), 84 folded, 681 propagated,
3 tail loop(s), 0 reassociated
bce: elided 387/1082 index and 6/88 slice checks
```

Версия Goose: `goose-2026.3-string`. Предупреждения — только лишние `as`.

## 5. Риски слияния

1. **Export-сигнатуры.** Любая export-функция с mixed `const`/mutable slices
   не вызывается. При слиянии не возвращать `const` в `json_schema_convert` и
   `grammar_text`; иначе снова `could not resolve`.
2. **Контракт `meta`.** Конвертер ждёт плоское дерево `meta/props/vals`, а не
   канонические массивы `schema_build`. Нужен адаптер; это главный оставшийся
   шов.
3. **Константы.** Блок использует `FG573_*`; при подключении к каноническим
   видам нужна таблица соответствия, особенно `K_TUPLE` (12 в каноне, 19 в
   блоке) и новые `K_PATTERN`/`K_FORMAT`/`K_STRING_LEN`/`K_INT_BOUNDS`.
4. **`_not_strings`.** Символы вставляются в GBNF-классы без экранирования,
   как в native. Имена свойств с `]`, `"` или управляющими байтами дадут
   невалидную грамматику — это native-поведение, не исправлять молча.
5. **`additionalProperties: true` с `properties`.** Нативного теста на эту
   комбинацию нет; фикстура `additional-true-props` проверяет её через oracle.
6. **Размер патча.** 3 245 строк diff; ревьюить блоками: primitive/pattern,
   object/additional, bounds, unions/refs, tuple/format.
7. **Буферы.** `pwork`/`sbuf`/`mprops`/`rref` — ответственность вызывающей
   стороны; guard есть только у `pwork`.

## 6. SHA-256

| файл | SHA-256 |
|---|---|
| `base/runtime.goose` | `3be19f7fe73250d3ee79f3caca444d7f75659c00d9f6067c402d855f47e736e3` |
| `draft.goose` | `b45d9ddea3a774b84e123f83d24622d4891ef9f96a4202e15156fa39251fd7cb` |
| `cases.md` | `972b112a198bad5fe1dfa9bedfeea8f5407ffa773dd2bef2d705e8d54aa5e7df` |
| `tests/fixture.cpp` | `1a636b9afc97e7494ac2783392c662e5958dbb1d3b06e569b7eda334e575ef2a` |
| `tests/build.sh` | `107a114908e7da3c23b97b19f9ab2ef8faaeee241c9c9a51cfcf396f32e680d2` |
| `integration/base/format-grammar.goose` | `820ac08ffeb1887fb50d6201d0123dee2c7cd027f2cd6e0ae5f0399c7ea3b15b` |
| `integration/format-grammar.goose` | `41d6172e3ea0eb18e352946b3fc3f98ceb1179e1ce80a37db97b5f679167d0a8` |
| `integration/format-grammar.patch` | `ac7b4b779a149e403263a44158f5d5a9e78bba8afc2ee8e28d3040bea57e0c58` |
| `integration/EXPORTS.txt` | `6f4fb56d341798b7596a318baa6759457b2559ea2437da5f3dcff881aba370cd` |

SHA-256 `REPORT5.md` не приводится: он меняется при вставке хеша.

## 7. Следующее действие

Запустить `tests/build.sh` (30 схем). После зелёного гейта — адаптер
`schema_build -> meta/props/vals` и перенос `json_schema_convert` в рабочее
дерево одним атомарным коммитом.
