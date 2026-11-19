# Which artefact answers which door

Written 2026-11-19 after two tests were found measuring the wrong library. It exists because
"the library" is not one thing: there are three lineages in this tree, and a check that does not name
its artefact proves nothing about the artefact a user runs.

| artefact | built from | carries | exported doors measured |
|---|---|---|---|
| `runtime/goose-stage/lib/libllama.so` | `integration/m1/common/*-bridge.c` staged as an m1 build, 2026-11-15 | the m1 bridge symbols (`fg_gr_enabled`, `fg_unicode_flags_*`, `fg377s_enabled`, `fg_gr_check`, `fg_n01s_count`, `fg_q35s_plan`) | `tools/check-goose-modules.py`: **8/8** |
| `builds/checkpoint-20261119-EXP1476/libllama.so.0` (the release, sha256 `0ffd98c2`) | `runtime/reconstructed/build.py` over the module list, 2026-11-19 19:00 | reconstructed unit doors (`json_parse` absent, `json_dump` absent) | `tools/unit-carry.py`: **198 of 1254** |
| `builds/flint-acc/bin/libllama.so.0` (sha256 `bea5d6af`, 19:18) | unknown — no script under `tools/` writes there | reconstructed unit doors **including** `json_parse`/`json_dump` | do not measure: provenance unknown |

## The consequences, stated once

- A test of the m1 doors must name the stage build, and that build is four days old: 8/8 says the m1
  staging answers its questions, not that today's tree does.
- A test of the reconstructed unit doors must name a checkpoint or the release, and the release
  carries none of the m1 door set - the two sets do not overlap, so neither artefact can stand in for
  the other.
- The working directory carries doors that the release does not. That is the release being older, not
  the working directory being right: it was written by nothing recorded, so a measurement on it
  describes that directory only (rule 22).

Until one build carries both sets, any figure quoted for "the runtime" must say which artefact it was
measured on. That sentence is the reason this file exists.
