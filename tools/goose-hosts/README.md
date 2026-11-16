# Goose decision hosts

Four small C programs that expose accepted Goose decisions to the scripts which
own the surrounding work. Each host links one generated unit and speaks a
one-line-at-a-time protocol on stdin/stdout, so a caller pays one process for a
whole corpus instead of one per decision.

| host | module | product caller | opt-out |
|---|---|---|---|
| `art` | `goose/misc/generated-artifact-policy.goose` | `tools/check-generated-files.py` | `BMOE_GOOSE_ART=0` |
| `env` | `goose/misc/build-env.goose` | `scripts/build.sh` | `BMOE_GOOSE_ENV=0` |
| `pkg` | `goose/misc/release-plan.goose` | `scripts/make-release.sh`, `tools/package-main-runtime.sh` | `BMOE_GOOSE_PKG=0` |
| `fx` | `goose/misc/fixture-plan.goose` | `scripts/make-tiny-moe.py` | `BMOE_GOOSE_FX=0` |

The features are on by default; the flag exists only to turn one off, which
restores the inline code the script carried before the wiring. A missing host
binary is not fatal: each caller prints one line on stderr and falls back to its
inline decisions, so a fresh clone can still run the hook and the fixture
generator before anything is built.

## Build

```bash
bash tools/goose-hosts/build.sh              # -O1, into build/goose-hosts
bash tools/goose-hosts/build.sh --mutate     # -O0, for a mutation run
GOOSE=/path/to/goose CC=gcc-13 bash tools/goose-hosts/build.sh
```

Sources are never edited: the tool concatenates the module with an optional
shim into `build/goose-hosts/<name>-unit.goose`, generates the C and the header
there, and links `<name>-main.c` against it. Generated C, headers, objects and
binaries are build artefacts and are not committed.

`test-runner.goose` has no host. Its two slice-parameter `bool` exports
(`tr_match`, `tr_selected`) cannot be emitted at an export boundary by
`goose --standalone` (the toolchain refuses with "could not resolve one concrete
function signature"), and a shim that calls them with concrete types does not
help; the module only runs under `goose run`. That is recorded in the
experiment report as the remaining blocker, not worked around here.

## Protocol

```
request := COMMAND [SP field]...
field   := text | '-'            '-' is the empty string
text    := hex of the raw bytes  (no field can contain a space or a newline)
reply   := "OK <value>" | "ERR <reason>"
```

`OK` values are decimals for integers, hex for text. `tools/goose-hosts/client.sh`
provides `goosehost_init`, `goosehost_hex`, `goosehost_ask` and `goosehost_raw`
for the shell callers; the two python callers carry the same ten-line client
inline so a hook stays a single file.

## What stays native

The hosts own decisions only. Every caller keeps its own work: the git index and
the head read in the hook, the cmake invocation and the file system in
`build.sh`, `git`/`gh`/`stat`/`sha256sum` and the gguf writer in the release and
fixture scripts, `numpy` in the generator.
