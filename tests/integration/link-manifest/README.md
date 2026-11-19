# link-manifest

**Requirement:** P10-06 - the compile/link manifest audit, listed as its own item.
**Produced by:** `tools/audit-link-manifest.py`; permanent since 2026-11-19.
**Tier:** fast - reads a directory with `readelf` and `nm`, about a second.

## What it reports

For every shipped artefact: its size, its exported symbol count, and each `NEEDED` dependency
resolved to `shipped`, `system` or **missing**. Then the two things a release reader cares about:

1. **a dependency the set does not contain** - always a failure, because the archive would not run;
2. **a RUNPATH into a build tree** - a warning by default, a failure with `--strict-runpath`.

The split is deliberate: the accepted set of 2026-11-19 carries such a RUNPATH, so making it fatal
here would leave a permanently red check for a defect whose fix belongs to the next build. The
warning names every artefact, and a release gate passes `--strict-runpath`.

## Result on the published set

24 artefacts, no missing dependency: everything the set needs is either inside it or part of the
platform. **Twelve** artefacts carry `RUNPATH=/home/end0/AI/FrankenMoE-CUDA/builds/flint-acc/bin` -
the binary and every library, not the two checked when the defect was first found. The archive works
through the documented `LD_LIBRARY_PATH`, and on the build host it silently prefers the build tree.

## Run

```
bash tests/integration/link-manifest/run.sh            # newest set, warnings only
bash tests/integration/link-manifest/run.sh <set-dir>
python3 tools/audit-link-manifest.py <set-dir> --strict-runpath   # the release gate
```
