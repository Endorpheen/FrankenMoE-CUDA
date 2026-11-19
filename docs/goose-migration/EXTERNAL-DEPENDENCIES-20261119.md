# External dependencies of the shipped set, and a RUNPATH defect found while checking them

Recorded 2026-11-19 for F1-03, from the published release set
`/home/end0/AI/LlamaServer/frankenmoe-release-20261120-exp1476` (asset of `v0.10.0-exp1476`).

## What the shipped `llama-server` links

```
libllama-server-impl.so   shipped
libllama.so.0             shipped
libllama-common.so.0      shipped
libmtmd.so.0              shipped
libggml.so.0              shipped
libggml-base.so.0         shipped
libstdc++.so.6            system    (GCC 13 runtime)
libgcc_s.so.1             system
libc.so.6                 system
libssl.so.3, libcrypto.so.3  system  (OpenSSL 3 - cpp-httplib TLS)
libm.so.6                 system
```

Vendored sources in the tree, all header-only or compiled in: `cpp-httplib`, `nlohmann`,
`miniaudio`, `stb`, `sheredom` (utf8), `hash` (robin-map style), plus `flint` itself (the
llama.cpp fork).

## The defect

`readelf -d` on the shipped binary and on `libllama.so.0` reports:

```
RUNPATH  /home/end0/AI/FrankenMoE-CUDA/builds/flint-acc/bin:
```

An absolute path from the build host. Consequences, verified:

| check | result |
|---|---|
| `env -i ldd ./llama-server` on this host | resolves everything - **from the build directory**, not the package |
| `env -i LD_LIBRARY_PATH=$PWD ldd ./llama-server` | 0 `not found` - the package's own libraries are used |
| `env -i LD_LIBRARY_PATH=$PWD ./llama-server --version` | runs, prints the Goose diagnostics |
| on any host without that build directory | the RUNPATH is dead; without `LD_LIBRARY_PATH` the loader falls back to system paths and will not find `libllama-server-impl.so` |

So the archive is **not self-contained by RUNPATH**: it works through the `LD_LIBRARY_PATH=$PWD`
line the release notes prescribe, and it works on the build host even without it - which is the
worse case, because the second outcome hides the first. The acceptance run for the release used
`APEX_SERVER_DIR`, which sets the path, so it exercised the package's libraries; that part of the
record stands.

## The fix, and why it is not applied yet

The correct value is `$ORIGIN`, so the loader looks next to the binary. `patchelf` is not installed
here, so it cannot be corrected in place without touching the shipped bytes by hand. The honest
options, in order of preference:

1. rebuild with `-Wl,-rpath,'$ORIGIN'` in the link line, which fixes it for every future artefact;
2. ship a `run.sh` in the archive that sets `LD_LIBRARY_PATH` before exec, and keep the notes;
3. install `patchelf` and rewrite the RUNPATH of the published asset, re-uploading it.

Re-uploading the current asset changes a published artefact, so it is Igor's decision, not a
step taken unilaterally.

## What this means for F1-03

The dependency list above is accurate for the artefact that exists. What remains for the report:
licences and versions per shipped library, the version of each vendored source, and the statement
of which of them are primitives the runtime cannot avoid (GPU driver, libc, C++ runtime) versus
logic that is still replaceable (the JSON parser being the one already ported).


## Licences and versions, from the tree

| dependency | version | licence | source in the tree |
|---|---|---|---|
| `nlohmann/json` | 3.12.0 (`NLOHMANN_JSON_VERSION_*`) | MIT (the file also carries the Apache-2.0 note for its embedded parts) | `vendor/flint/vendor/nlohmann/json.hpp` |
| `cpp-httplib` | 0.62.0 (`CPPHTTPLIB_VERSION`) | MIT | `vendor/flint/vendor/cpp-httplib/`, `LICENSE` |
| `miniaudio` | not declared in the header | public domain | `vendor/flint/vendor/miniaudio/miniaudio.h` |
| `stb_image` | not declared | public domain | `vendor/flint/vendor/stb/stb_image.h` |
| `sheredom/utf8.h` | not declared | MIT-style, single header (licence text in the file) | `vendor/flint/vendor/sheredom/utf8.h` |
| `hash` (robin-map style) | not declared | MIT-style (see its CMakeLists and header) | `vendor/flint/vendor/hash/` |
| `flint` (the llama.cpp fork) | pinned by `vendor/flint/PROVENANCE.txt` | MIT | `vendor/flint/LICENSE` |
| OpenSSL 3 (`libssl.so.3`, `libcrypto.so.3`) | system | Apache-2.0 | not vendored; loaded from the host |

## Which of them are primitives, and which are replaceable logic

The distinction F1-03 asks for, applied to the list above:

- **Unavoidable primitives of this platform**: glibc, `libstdc++`/`libgcc` (while any C++ remains
  linked), the CUDA driver and runtime, OpenSSL for TLS. None of these is a decision the project
  makes; each is a facility the OS or the GPU provides.
- **Replaceable logic that is still external**: `nlohmann/json` - the parser and dumper are already
  ported and exercised (`tests/json/*`), so what remains is the adapter and the swap, not the
  capability. `cpp-httplib` - the HTTP framing decisions are ported in `goose/server/http-*.goose`,
  and the library currently supplies the socket loop.
- **Facilities with no decision in them**: `miniaudio`, `stb_image`, `utf8.h`, `hash` - they move
  bytes or hash them, and the project's own rules count them as primitives, not as logic to port.

The report is complete for the shipped set. What it does not yet carry is a symbol-level inventory
per dependency, which is only meaningful once the JSON swap has happened.
