# P10-07 — replace or justify vendored executable dependencies

Audit and port report, 2026-11-15. Base commit `70de805c`.
Canon: `upstream/llama.cpp` at `0b5be7e4a25862bc2777d0c47eae18788a8c963a`.
Shipped tree read for comparison: `work/llama.cpp-integration` (fork `4aaad5d`),
unmodified.

---

## 1. What the requirement asks

`docs/goose-migration/tasks.json` P10-07:

> Each JSON/Jinja/HTTP/regex/image/audio helper classified; replace
> owned/replaceable logic or explicitly justify true external primitive. Vendored
> C++ is not automatically unavoidable.

Sources named by the row: `vendor/**`, `ggml/src/ggml-cuda/vendors/**`,
`common/jinja/**`. The web UI's `src/lib/vendors/**` was added because it is the
only other vendored code in the same pinned tree, and the row's "image/audio
helper" wording reaches it.

## 2. Method

1. Every file under the four trees was hashed and attributed to a component,
   with its upstream pin taken from `scripts/sync_vendor.py` — the table the
   project itself uses to refresh the vendored files — or from the in-tree
   origin when the file is not downloaded at all.
2. Each component was traced to its consumers by `#include` and by
   `target_link_libraries` in the real CMake files, so "unused" means unused by
   the build the product runs, not unused by a grep of one directory.
3. Each component was classified into exactly one of:
   **replace** (an owned implementation exists or is trivially droppable),
   **justify** (a primitive whose replacement would be a new project),
   **not-a-dependency** (first-party code that only looks vendored).
4. The decisions that keep a vendored file admitted were ported to Goose and
   gated offline: the licence payload that ships with it, the pin and patch
   anchor that make it reproducible, and the build-flag/linkage rules that select
   it. See §5.

## 3. Inventory

Totals: **14 components**, 13 third-party and 1 first-party shim set.
`vendor/` holds 27 files and 6,491,829 bytes. Class counts:
**9 replaceable, 4 justified, 1 not a dependency.**

| # | component | version / pin | licence | files | size | consumers | class |
|---|---|---|---:|---|---:|---|---|
| 1 | nlohmann/json | 3.12.0, `releases/latest` (floating) | MIT | `json.hpp`, `json_fwd.hpp`, `CMakeLists.txt` | 960,329 B / 25,719 L | `common/json.cpp`, `tests/gguf-model-data.cpp`, linked by `llama-common` | replace |
| 2 | cpp-httplib | 0.54.1 canon / **0.53.1 shipped**, `refs/tags/v0.54.1` | MIT | `httplib.h`, `httplib.cpp`, `LICENSE` | 784,713 B / 22,889 L | `tools/server/server-http.cpp`, `server-models.cpp`, `common/http.h`; `llama-common:147`, `llama-server-impl:51` | justify |
| 3 | miniaudio | 0.11.25, commit `9634bedb` | public domain / MIT-0 | `miniaudio.h` | 4,108,413 B / 95,870 L | `tools/mtmd/mtmd-helper.cpp:33`; `mtmd:83` | replace (scope) |
| 4 | stb_image | 2.30, **branch `refs/heads/master`** | public domain | `stb_image.h` | 283,225 B / 7,994 L | `mtmd-helper.cpp:36`; `mtmd:83` | replace (scope) |
| 5 | sheredom/subprocess.h | commit `0dccaa9a` | Unlicense | `subprocess.h` | 68,741 B / 2,200 L | `common/subproc.h:9`, `server-mcp.cpp`, `mtmd-helper.cpp`; `llama-common:132`, `mtmd:83` | justify |
| 6 | xxhash | 0.8.3, commit `9f465f1e` | BSD-2-Clause | `xxhash.c/.h/LICENSE` | 286,143 B* | `examples/gguf-hash` only | replace (drop) |
| 7 | sha1 (clibs) | commit `e1e2536f` | public domain | `sha1.c/.h` | * | `examples/gguf-hash` only | replace (drop) |
| 8 | sha256 (jb55) | commit `5e637272` | public domain | `sha256.c/.h/LICENSE` | * | `mtmd-helper.cpp:386` cache key, `gguf-hash` | justify |
| 9 | rotate-bits | commit `27e78494` | MIT | `rotate-bits.h`, `LICENSE.md` | * | `sha256.c:5` | justify (transitive) |
| 10 | Jinja engine | in-tree, llama.cpp | MIT (llama.cpp) | 14 files under `common/jinja/` | 6,261 L | `common/chat.cpp`, `chat-auto-parser.h`, 4 test files | replace |
| 11 | nerdamer-prime | vendored JS | MIT | 8 files | 1.4 MB | web UI `sandbox-worker.js` global | replace (UI) |
| 12 | big-integer | vendored JS | Unlicense | `BigInteger.js` | 60 KB | nerdamer | replace (UI) |
| 13 | decimal.js | 10.6.0 | MIT | `decimal.js` | 144 KB | nerdamer | replace (UI) |
| 14 | `ggml-cuda/vendors/*.h` | in-tree | llama.cpp | `cuda.h` 28 L, `hip.h` 301 L, `musa.h` 151 L | 480 L | `ggml-cuda` translation units | not a dependency |

\* items 6–9 share one directory: 286,143 B and 7,891 lines for the whole
`vendor/hash` tree.

### 3.1 Notes that change the classification

- **Item 14 is first-party.** `cuda.h`, `hip.h` and `musa.h` are macro aliases
  and version guards written by llama.cpp to normalise three vendor APIs
  (`CUBLAS_GEMM_DEFAULT` → `HIPBLAS_GEMM_DEFAULT`, `CUDART_VERSION < 11020`
  fallbacks, the FP8/FP4 includes). The directory name says "vendors"; the
  content is the project's own compatibility layer. There is nothing to replace
  and nothing to license.
- **Items 6 and 7 are compiled but never called by the product.** `vendor/hash`
  always builds `xxhash.c`, `sha1.c`, `sha256.c`; only `hash.cpp`'s
  `hash_sha256_hex()` is used, by `mtmd` as an image cache key. `sha1.c` exists
  because `examples/gguf-hash` prints all three digests, and it even carries three
  local patches (a namespace and two includes) purely to avoid a BoringSSL symbol
  clash — a maintenance cost paid in every build for an example tool.
- **Items 3 and 4 are product scope, not code decisions.** `miniaudio` (audio
  input) and `stb_image` (image input) are reached only through `mtmd`, which the
  server links unconditionally even though the shipped profile is text-only
  (`/embeddings` and `/rerank` answer 501 per P8-05; no vision or audio model is
  loaded). Dropping them is a one-flag decision (`LLAMA_BUILD_MTMD=OFF`), not a
  rewrite; keeping them is defensible only while multimodal is on the roadmap.
- **Item 1 has an owned replacement already accepted.** `goose/json/json-parser.goose`
  reproduces `common_json::parse` (RFC 8259, number classification, escapes,
  duplicate keys, error classes and positions) and `json-dump.goose` reproduces
  the serializer, including `dump_safe`. What remains native is the container
  type and its allocator — a data structure, not a JSON decision.
- **Item 10 has an owned replacement already accepted.** `goose/jinja/` (15
  modules: lexer, parser, runtime, value, string, JSON bridge, builtins,
  delimiters, streaming, auto-parser, tool builders) is the port of
  `common/jinja/`. The vendored engine stays only as the native carrier.
- **Item 2 is the one true external primitive.** Sockets, TLS, keep-alive,
  chunked framing and the thread-pool queue are not project decisions; P8-02
  closed with exactly that statement, and this audit confirms it rather than
  reopening it.
- **Item 11–13 are browser assets.** They are never compiled into the native
  binary; they expose a `nerdamer` global to model-generated expressions inside
  the UI's sandbox worker. They are "vendored executable dependencies" of the web
  UI only, and they leave with the UI.

## 4. Findings the audit produced

1. **The shipped tree vendors a different cpp-httplib than the canon.**
   `work/llama.cpp-integration/vendor/cpp-httplib/httplib.h` is 0.53.1
   (`b36e352e…`), the canon pins 0.54.1 (`300a319f…`, `httplib.cpp`
   `c796420a…`). Both `sync_vendor.py` copies agree with their own tree. Nothing
   in the repository records that the product runs one minor version behind the
   pin; `UPSTREAMS.md` pins the clone, not the vendored files inside it.
2. **Three of eighteen downloaded entries are not reproducible.**
   `nlohmann/json.hpp` and `json_fwd.hpp` come from `releases/latest/download`,
   and `stb_image.h` from `refs/heads/master`. A refresh at a later date produces
   a different tree with no record of the change. `xxhash`, `sha1`, `sha256`,
   `rotate-bits`, `sheredom` and `miniaudio` are pinned by commit;
   `cpp-httplib` by tag.
3. **Only one vendor licence is registered with the build.**
   `vendor/cpp-httplib/CMakeLists.txt:2` calls `license_add_file("cpp-httplib"
   "LICENSE")`. Every other component's notice lives only in its own source
   header (`xxhash` and `sha256`/`rotate-bits` do ship a `LICENSE` file that
   nothing reads; `nlohmann` carries SPDX headers; `stb`, `miniaudio` and
   `sheredom` carry inline notices). The embedded `license.cpp` therefore
   under-reports the third-party inventory: P10-11's licence audit is correct
   that the texts are present, and this is the gap behind its "optional
   NOTICE/THIRD_PARTY summary" note.
4. **`vendor::nlohmann` is added unconditionally but linked by one component.**
   With `LLAMA_BUILD_COMMON=OFF`, `vendor/CMakeLists.txt` still adds the
   directory while nothing links it. `sys_added_not_linked` reports exactly this
   class of mismatch; the same mask reports `miniaudio`/`stb` when `mtmd` is off.
5. **The production profile builds examples and tests.**
   `build/expert-tier-franken-cuda/CMakeCache.txt` has
   `LLAMA_BUILD_EXAMPLES=ON`, `LLAMA_BUILD_TESTS=ON`, `LLAMA_BUILD_TOOLS=ON`,
   `LLAMA_BUILD_SERVER=ON`, `LLAMA_OPENSSL=ON`. Everything in the inventory is
   therefore live in that tree, including the two hashes that only examples use.

## 5. Resolutions verified offline

Three slices ported to Goose, each gated against the native rules executed by
their own interpreter. Full detail in `README.md`; the numbers:

| slice | module | cases | values | fail `-O1` | fail `-O0` | mutations |
|---|---|---:|---:|---:|---:|---:|
| S1 licence payload | `vendor-licence-payload.goose` | 60 | 60 | 0 | 0 | 4/4 |
| S2 provenance | `vendor-provenance.goose` | 141 | 141 | 0 | 0 | 4/4 |
| S3 selection/linkage | `vendor-selection.goose` | 39 | 39 | 0 | 0 | 4/4 |
| **total** | | **240** | **240** | **0** | **0** | **12/12** |

The gates are differential: `cmake -P` runs the real `license.cmake`, the real
ggml `if()` blocks and the real `vendor/CMakeLists.txt`; Python's `str.count()`
counts the real anchors on the real patched files; the component linkage is
parsed from the real `target_link_libraries` lines. The gate found five defects
in the first draft of the port (§"What the gate found" in the card), including
the missing `cpp-httplib` link from `common` that would have made the linkage map
wrong.

## 6. Risks

1. **Floating pins (three of eighteen entries).** A `sync_vendor.py` run at any later
   date changes `json.hpp` and `stb_image.h` with no diff to review beyond the
   vendor tree. Recommendation: convert those three entries to commits and record
   the hashes in the table (a `vnd_`-driven checker can then fail on drift).
2. **Version skew between canon and product (finding 1).** A CVE or a fixed bug
   in `httplib` 0.54.1 is not in the binary the service runs. The audit cannot
   fix this — the shipped tree is off limits — but it must be visible.
3. **Dropping `sha1`/`xxhash` from the product link touches a shared CMake file.**
   It is a three-line change (`VENDOR_SRCS` split, an examples-only target), but
   it changes `vendor/hash/CMakeLists.txt`, which is upstream-owned: it would
   become a local patch that `sync_vendor.py` does not track. That is precisely
   the trade-off item 6/7 encode, and it is why they are classified "replace"
   rather than done here.
4. **The `owned` classification is a policy, not a fact.** `CMakeLists.txt` files
   under `vendor/` are first-party, and the two `httplib` files produced by
   `split.py` are generated from a downloaded `httplib.h`. If upstream ever ships
   a `CMakeLists.txt` in the download table, `vnd_file_state` would admit a
   downloaded file as owned. The mitigation is the table check (declared first),
   which the current gate already exercises in both orders.
5. **The Windows arm of `IS_ABSOLUTE` is untested** (no Windows host). It affects
   only licence path resolution on a Windows build, and both arms are explicit
   arguments rather than a compile-time guess.
6. **`vnd_pin_kind` is a shape test, not a trust test.** A pinned commit in the
   URL is reproducible, not verified: nothing checks that the downloaded bytes
   match a recorded digest. Adding that check is the natural next step and is the
   reason the inventory carries per-file hashes.

## 7. What remains

Out of scope for this experiment, in dependency order:

1. **A `tools/check-vendored-deps.py` front-end** that walks `vendor/`, feeds
   `vnd_`/`sys_`, and fails on an untracked file, a floating pin or an
   added-but-unlinked directory. The gate already proves the decisions; the
   front-end is wiring plus a CI hook.
2. **Recorded digests per vendored file** (SHA-256 in the table) so that a pin
   check becomes a hash check, not a shape check. `results/*-vendor-sha256.txt`
   in this experiment is the starting data for both trees.
3. **The three product decisions**, which are the lead's, not a port's:
   `sha1`+`xxhash` out of the product link, `mtmd` behind a flag (drops
   `miniaudio`+`stb_image`), and `cpp-httplib` bumped to 0.54.1 in the shipped
   tree.
4. **The remaining Jinja and JSON surfaces** are owned by their own requirements
   (P9-07 and the JSON series); nothing here reopens them.
5. **Regex**: the row names a regex helper. There is none vendored —
   `common/regex-partial.cpp` is llama.cpp's own partial-match helper and
   `std::regex` is the standard library. Recorded so the row's wording does not
   imply an unclassified dependency.

## 8. SHA-256

Artefacts (`results/hashes.txt`):

| file | sha256 |
|---|---|
| `source/vendor-licence-payload.goose` | `3405622452bd2b658e6ac45ccf0bfcabd2b1ae3154f4f936e71d616d4fd87813` |
| `source/vendor-provenance.goose` | `9409fad9027aaa689a6a6cb1f7b4c4f485f2aef89a028e60506b51361125b65a` |
| `source/vendor-selection.goose` | `7a6cfeb577e2d7a043bc95765e9067e5f6b33ef42666bdeb858f2a29eb18dc5d` |
| `tests/oracle_lic.py` | `2c27e1d93dd7dd21fc7370def62d86daf4c2635ce7187fb88e5ec80adb285e3d` |
| `tests/oracle_vnd.py` | `6e8aceae13904c5e0dcc7ae7f42996ca605ee860ab35cc1902e6395ae2e2de7c` |
| `tests/oracle_sys.py` | `a241fefe6185d771bdaf752a7f25b028f4bc50b0494cba8fbe63b7848df20ab7` |
| `tests/lic_fixture.cpp` | `a6bfde7128a39b6e665bdfef817c8b724ec4eccd9ec3669d3ee98e1deb633ef9` |
| `tests/vnd_fixture.cpp` | `60c5049349c14abb15aed276c4a28738826db48ff6f4865d74f99a15b38796b2` |
| `tests/sys_fixture.cpp` | `9b9361d0298593d97da457706c708d86de459f674ef9e564bf15362cdd0d9cf3` |
| `tests/mutate.py` | `2dcd43d1d6ad2c8dbe7a38c7f72535fa381b6d8742cbe86534d7377d0d25bd4c` |
| `tests/build.sh` | `5191c5eccf001ed21e26f11ca56468f99833ced4ba35b4519f4aacd8b4f7b881` |
| `tests/gen_cases_vnd.py` | `d45c937d6553c34ef4929144b8d6ae0a861839a26067e98bd672d1f44a0ca4c1` |
| `tests/cases/lic.tsv` | `e77ea7140a6840b6e800c7987d528105d0a68f0deefbce1975e68a57c6cbe825` |
| `tests/cases/vnd.tsv` | `861cf1eb2cdf0c72b6044358984e680634cb56e86dfd6e1ed08e265b503cf88a` |
| `tests/cases/sys.tsv` | `f9a610ba3fbca272acd9aa33d3b72833d43fbea65fbc711a94cf3f8444519721` |

Recorded runs: `results/gate-o1.log`, `results/gate-o0.log`,
`results/mutate-1.log`, `results/mutate-2.log`, `results/tree-probe-prod.log`
(the same `vnd_` probe against the shipped tree: it passes, because the path set
and the pin classes agree — content divergence is a hash question and is recorded
separately), `results/vendor-hash-diff.txt`.

Native inputs (`results/native-sha256.txt`, copied byte-identical):

| file | sha256 |
|---|---|
| `cmake/license.cmake` | `083515dff8a9b8696e348c7c616934e53941a7db30e1b3f092576632e9a2d1d4` |
| `scripts/sync_vendor.py` | `b36dc1f59aff47892382028666444eab0b53309632275d0729867721dfd7b19e` |
| `CMakeLists.txt` | `04c16dc102dfcce1d10da90e8689a7d9540f19fb441ad1dbdfca68e52e74f6b2` |
| `vendor/CMakeLists.txt` | `97242b05cf7d56231b05afa991036260025b9d6fe4f003dbc997dd7f5d2be06b` |
| `common/CMakeLists.txt` | `e15f28090782582b554183a3509f9d80646d9568bc3d9da4135f972c1a8d169e` |
| `tools/mtmd/CMakeLists.txt` | `699b24cd2861200d659d2ca5376a1cf208cb54120378b2e2538019359b491a80` |
| `tools/server/CMakeLists.txt` | `d6da19380ce9cd2918571700d6a4edc06f76efabc305a6123e14c9d0c5e5a328` |
| `vendor/hash/xxhash/xxhash.h` | `1d5a18c925f71a9c2d8424742df47492f846f9890e3e8c55d201e4729d9e3d6d` |
| `vendor/hash/sha1/sha1.h` | `aded220220587c5a409dfa04f4bc2e760bd5bc81ac74af830aceb5bde7bc43e6` |
| `vendor/hash/sha1/sha1.c` | `6de78afb7c2e54ccbd742c0307015db4dceaa64d85533709f99de05074d639e4` |
| `vendor/hash/sha256/sha256.c` | `ba7a8ea69ad125385db28ef17e77be4a09dfdc30bb9532c30b259baaf670129c` |
| `vendor/cpp-httplib/LICENSE` | `4b45cbe16d7b71b89ae6127e26e0d90a029198ca5e958ad8e3d0b8bbed364d8b` |

Vendored trees: `results/upstream-vendor-sha256.txt` (canon, 25 files + 7 build
files) and `results/prod-vendor-sha256.txt` (shipped tree, shows the 0.53.1
`httplib` divergence).

## 9. Reproduce

```
cd /tmp/opencode/sub-653/EXP-2026-11-15-649-p10-07-vendored-deps
bash tests/build.sh all              # 240 values, 0 failures, -O1
bash tests/build.sh all mutate       # the same at -O0
python3 tests/mutate.py 1            # 6/6
python3 tests/mutate.py 2            # 6/6
```

Needs `cmake`, `python3`, `gcc-13`, `g++-13`, the goose compiler and read access
to the pinned `upstream/llama.cpp`. No network, no GPU, no model, no build of
llama.cpp.
