# How much of the composed unit an artefact actually carries

Measured 2026-11-19 with `tools/unit-carry.py`, which counts the `export fn` names of the composed
unit (from the module list) against the symbols an artefact exports. It is derived, not typed.

## The numbers

| artefact | unit exports | carried | share | what it is |
|---|---|---|---|---|
| `builds/flint-acc/bin/libllama.so.0` | 1254 | 200 | 15% | the shipped library (the EXP1476 build) |
| `build/goose-main/main.o` | 1254 | 1213 | 96% | the unit object from a build made before the Qwen modules joined |

## Why the gap exists, checked rather than assumed

A door that is absent is not a defect on its own: it may be newer than the artefact. Sampling proves
the pattern instead of asserting it:

| door | in the shipped library | in today's unit object |
|---|---|---|
| `mg_stop_gate` | yes | yes |
| `arch_default` | yes | yes |
| `json_parse` | yes | yes |
| `route_id` | **no** | yes |
| `q35_plan` | **no** | yes |
| `key_authorize` | **no** | yes |
| `mime_of` | **no** | yes |

The first three existed when the shipped library was built; the last four were added afterwards. So
the library is a faithful build of the unit **as it stood then**, and the tree has grown since. That
is the honest reading, and it is the reading the earlier per-door findings (model-manager doors, then
the api-key doors) each pointed at.

## The 96% figure is itself an artefact of age

`build/goose-main/main.o` predates the Qwen addition, which is why all `q4m_*` doors appear in its
absent list. `sanitize_into` appears there for a different reason: the composer renames it when two
modules define it, so a straight name comparison cannot see it. Both are properties of the method
that a reader needs, so they are written down rather than smoothed away.

## What this means for the ledger

P11-01 and P11-07 ask for one runtime that carries everything accepted. The measurement above says
the current shipped artefact carries 15% of today's unit, and the reason is the build's age, not a
lost component. Anyone comparing the two must compare artefacts of the same vintage; comparing a
current unit with the EXP1476 library and calling the difference a regression would be the same
mistake as comparing a cold measurement with a warm one.

## Production reach, and a defect in the module list itself

`tools/production-doors.py` indexes the identifiers of `vendor/flint` and `integration` once and
classifies every door of every unit module:

| class | doors |
|---|---|
| production (the identifier appears under `vendor/flint`) | 72 |
| harness-only (`integration/` only) | 139 |
| none | 1123 |

Of 1334 doors, 72 reach the sources the shipped runtime is built from. This is an **upper bound**: an
identifier can appear in a comment or a disabled branch. It agrees with the unit-carry figure, where
the shipped library exports 200 of 1254 - the same order, measured a different way.

The same run found **eight duplicate entries** in the module list: 163 entries, 155 unique. They were
removed, and the composed output is byte-identical before and after (`cmp` on 3 802 549 bytes), so the
dedupe is inert by construction. The module count quoted elsewhere as 163 is therefore wrong; the unit
is 155 modules.
