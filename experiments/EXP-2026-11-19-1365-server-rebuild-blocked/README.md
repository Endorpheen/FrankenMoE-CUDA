# EXP-2026-11-19-1365: why the server binary cannot be rebuilt here, in four stages

The server-side counters (`FG_SP_SYNTH`, `FG_ACCEPT_*`) live in `server-context.cpp`, compiled into
`libllama-server-impl.so`. That library has not been rebuilt since they were added, so a live door measurement on the
server side reports nothing even when the door is reached - proved in EXP-2026-11-19-1364 by a probe inside the door
itself.

Rebuilding it is blocked by the Web UI asset pipeline, which resolves in this order:

| stage | condition | what happens here |
|---|---|---|
| 1 | `tools/ui/dist/index.html` exists in the source tree | absent |
| 2 | `BUILD_UI=ON` | off in this build |
| 3 | `HF_ENABLED=ON` | **downloads** `dist.tar.gz` + `.sha256` from a bucket |
| 4 | none of the above | `emit_files()` over an empty directory, then `FATAL_ERROR: UI: missing required asset(s)` |

Stage 3 needs network, which this machine does not have: `UI: download dist.tar.gz.sha256 from b2503 failed: HTTP
response code said error`. Setting `HF_ENABLED=OFF` moves the failure to stage 4, which is a fatal error rather than a
skip - so the option that looks like "build without the UI" is not one.

A previous configure had left `builds/flint-acc/tools/ui/dist` populated; reconfiguring removed it (`file(REMOVE_RECURSE
${DIST_DIR})` in the download path), which is why the second attempt failed differently from the first.

## What was checked afterwards

The reconfigure touched only the cached `HF_ENABLED` value, which was restored (`ON`) so the candidate build directory
keeps its original configuration. Everything that matters was re-verified:

```
llama target        builds, rc 0
accepted components ok
acceptance          decode 63.23, prefill 2958.6, hash c2ff42cc1219
```

## Stage 1 was tried, and it is stricter than it looks

The built dist from `builds/flint-acc/tools/ui/dist` was copied into the source tree so stage 1 would return before any
download. It was reached - `UI: using pre-built assets from .../vendor/flint/tools/ui/dist` - and then failed the same
check for a different reason: the required set includes `workbox[hash].js`, which this dist does not contain. So a
pre-built tree only helps if it carries the service worker, and the copy was removed rather than left in the vendored
tree (`git status` showed it as untracked).

## What would unblock it, and who can do it

1. a machine with network access, so stage 3 succeeds once and the dist is cached in the build directory;
2. a `tools/ui/dist` tree copied in from any build that has one - stage 1 then returns before any download;
3. `emit_files()` tolerating an empty directory when both `BUILD_UI` and `HF_ENABLED` are off - a two-line change to a
   vendored script, which is not mine to make unilaterally because it changes how a vendored dependency builds.

Option 2 is the cheapest and needs no code change: `builds/checkpoint-*` directories may already carry a usable dist.
