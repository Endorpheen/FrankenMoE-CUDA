# P10-07 — replace or justify vendored executable dependencies

Status: **PARTIAL** — 3 of 3 slices accepted; the replacement plan for the nine
replaceable components is open.

## Inventory

14 dependencies were classified: 13 vendored plus the in-tree
`ggml-cuda/vendors/*.h` headers (first-party shims, not a dependency).

| component | pin | licence | class |
|---|---|---|---|
| nlohmann/json | 3.12.0 | MIT | replace (there is a Goose JSON port) |
| cpp-httplib | 0.54.1 canon / 0.53.1 production | MIT | justify (transport) |
| miniaudio | 0.11.25 | PD/MIT-0 | replace by scope |
| stb_image | 2.30, master branch | PD | replace by scope |
| sheredom/subprocess.h | commit | Unlicense | justify |
| xxhash | 0.8.3 | BSD-2 | replace (examples only) |
| sha1 (clibs) | commit | PD | replace (examples only) |
| sha256 (jb55) | commit | PD | justify (mtmd cache key) |
| rotate-bits | commit | MIT | justify (transitive) |
| Jinja | in-tree | MIT | replace (there is a Goose Jinja port) |
| nerdamer-prime / big-integer / decimal.js | JS | MIT/Unlicense/MIT | replace (UI asset) |
| `ggml-cuda/vendors/*.h` | in-tree | llama.cpp | not a dependency |

Totals: 9 replaceable, 4 justified, 1 not-a-dependency.

## Slices

| slice | content | exports |
|---|---|---|
| S1 licence payload | payload registration, inline notices, per-component licence wiring | `lic_*` |
| S2 provenance | pinned sources and the reproducibility rule for latest/branch entries | `vnd_*` |
| S3 selection and linkage | component masks, system vs in-tree, required links | `sys_*` |

## Evidence

- Experiment: `experiments/EXP-2026-11-15-651-p10-07-vendored-deps`.
- Canon: `goose/misc/{vendor-licence-payload,vendor-provenance,vendor-selection}.goose`.
- Gates: 60 + 141 + 39 comparisons, 0 failures, identical at -O0.
- Mutations: 12/12 (6+6).

## Findings

- The production tree carries cpp-httplib 0.53.1 against the 0.54.1 pin (2 of 27
  files differ).
- 3 of 18 `sync_vendor.py` entries are not reproducible (`releases/latest` twice,
  a branch once).
- Only cpp-httplib registers a licence file; the rest rely on inline notices.
- `vendor::nlohmann` is always added but linked by one component.
- The first port missed the cpp-httplib link from `common`; the gate caught it.

## Remaining

Replace or record a replacement plan for the nine replaceable components, and
wire the three decisions into `cmake/vendor` and `sync_vendor.py`.
