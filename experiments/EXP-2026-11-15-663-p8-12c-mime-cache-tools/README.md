# EXP-2026-11-15-663 — P8-12c: the asset MIME type, the Cache-Control text and the tool dispatch become runtime switches

Base: `c565e71f`. Isolated flint CPU tree; the repository tracked files were
not modified by the experiment itself. No model, server or GPU run happened.

## 1. Hypothesis

After EXP661 promoted the model status and the Origin localhost test, three
server call sites still carried their native bodies behind compile-time `#if`
blocks. Their Goose modules were already accepted with differential gates
(`mime-table.goose` and `web-assets.goose` in EXP653, `tools-pure.goose` in
EXP572). The claim of this step:

> If each of the three sites asks its Goose module first through one shared
> header that the differential gate also includes, then the answers are
> byte-identical to the native bodies on every case the gate covers, the two
> sites whose native body is redundant can lose it, and the tool dispatch must
> keep its native loop because MCP appends tools at run time.

## 2. What changed

| site | module | result |
|---|---|---|
| asset content type (`server-http.cpp` static asset handler) | `mime-table.goose` (`mime_of` + `mime_text`) | promoted; the native `a.type` value is gone from the serve path |
| `Cache-Control` text (same handler) | `web-assets.goose` (`web_cache_text`) | promoted; the two route literals `cache_immutable` / `cache_revalidate` are gone |
| built-in tool dispatch (`server-tools.cpp` `find_tool`) | `tools-pure.goose` (`tool_index`) | promoted, **native loop kept**: `find_tool` also serves MCP tools appended at run time and a reordered table |

The decisions live in `tools/server/fg-goose-server/fg-goose-sites.h` so the
gate includes the code the server runs rather than a copy of it. Two sites keep
a runtime switch that only disables the improvement (rule 10):
`FG_GOOSE_SERVER_CORS_NATIVE` and `FG_GOOSE_SERVER_TOOLS_NATIVE`. Three sites
have no switch left because their native body was deleted: model status
(EXP661), asset content type and `Cache-Control` (this experiment).

CMake: all five `FG_GOOSE_SERVER_*` options are `ON` by default. The per-site
`*_NUM` compile definitions and the `#if` blocks are gone; the option now only
decides whether the composed unit is linked at all, i.e. it selects a
unit-isolation build, not a product one.

An index-trust rule guards the tool site: the module's index is used only when
the native vector still holds the same name at that position, so a run-time MCP
append or a reordered table falls through to the native linear scan.

## 3. Result

| check | result |
|---|---|
| build + link, phase B (OFF / ON) | 19 608 584 / 19 692 904 B, rc=0 |
| default configuration (no flags) | 19 692 904 B, `goose_init` = 1 |
| differential gate, ON, explicit `goose_init` | 150/150 cases, 0 divergences |
| gate, ON, bridge lazy `goose_init` | 150/150, 0 divergences |
| gate, ON, native arms (Origin + tools) | 150/150, 0 divergences |
| gate, OFF bridge (stubs for mime and cache) | 150/150, 0 divergences |
| gate, default configure | 150/150, 0 divergences |
| mutations | 36/36 killed (12 MIME, 12 Cache-Control, 12 tools) |
| unit symbols in ON | `mime_of`, `mime_text`, `web_cache_text`, `tool_index`, `mg_status_str`, `cors_is_localhost`, `goose_init` — one definition each |
| unit symbols in OFF | 0 of the seven |

Re-verified independently at acceptance: `bash tests/run.sh B` gave 6 passed,
0 failed, and `nm` on the three binaries matched the table above.

## 4. Gate cases

150 cases per run, all TSV, exit 1 on any divergence:

- 9 model status ids (0..6, -1, 7 → `unknown`);
- 6 Origin hosts (3 localhost forms, 3 refusals including the
  `127.0.0.1.evil.com` suffix trap and an `ftp://` parse failure);
- 12 MIME names through the call site plus 5 raw module calls, including
  `INDEX.HTML` (case-sensitive → octet-stream) and an extensionless name;
- 4 Cache-Control policies through the call site plus 5 raw module calls,
  including two unknown ids that must answer the module's `-1`;
- 11 tool names through the call site plus 11 raw module calls, including a
  one-byte-short name, an empty name and an unknown name.

## 5. Mutations

36, in three halves of twelve, each applied to a copy of the composed unit and
rebuilt with `goose --standalone` + `gcc-13 -O1`. Raw data:
`evidence/mutations.tsv`.

- MIME (M1–M12): the html type loses its space, js changes family, the fallback
  is misspelled, manifest loses a byte, jpeg and png answer the wrong type, the
  extension id is read from the wrong end, a dot-less name is served as html,
  the dot stays in the extension, a length mismatch matches, svg becomes
  uppercase, woff2 is misspelled.
- Cache-Control (C1–C12): both literals lose a byte, gain a space or a
  directive, max-age loses a digit, an unknown policy answers the revalidate
  text, immutable answers the revalidate text, nocache falls through to -1, the
  policy id diverges from the call site's, the text is written one byte late,
  `web_put` reports one byte less.
- tools (T1–T12): six names change or lose a byte, the index is off by one, the
  last tool is never found, the compare drops a byte, an unknown name answers
  the first tool, a length mismatch matches, an id is reassigned.

## 6. What still is native here

1. `find_tool`'s linear scan stays: it is the arm a run-time MCP append needs.
2. The asset handler's ETag/304 logic and the gzip branch stay native; only the
   content type and the cache text moved.
3. `server-http.cpp` still parses the Origin URL before the module sees the
   host; `cors-policy.goose` owns the host comparison only.

## 7. Reproduce

```
bash tests/run.sh A            # every native body present
bash tests/run.sh B            # the two deleted bodies; OFF answers the stubs
python3 tests/mutate.py        # 36 mutations, control first
```

The staged tree needs a CPU-only llama.cpp with the merged server ports
(`patches/ports-*.diff`) and the composed unit `source/server.goose`. No
network, no GPU, no model, no server start.
