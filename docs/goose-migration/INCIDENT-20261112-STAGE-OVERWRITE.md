# Incident: I overwrote the accepted runtime's server library through a symlink

## What happened

`assemble_candidate.py` copied the merged `libllama-server-impl.so` into the candidate directory with `cp -f`. That path is a **symlink** into
`runtime/reconstructed/build/stage-run/`, and `cp -f` follows symlinks, so the copy replaced the shared target instead of the link.

The shared target is used by every runtime copy:

    runtime/run                                   -> stage-run/libllama-server-impl.so
    runtime/measured/run                          -> stage-run/libllama-server-impl.so
    runtime/exp244/run                            -> stage-run/libllama-server-impl.so
    experiments/EXP-...-340-.../stage             -> stage-run/libllama-server-impl.so
    runtime/reconstructed/candidate-20261112      -> stage-run/libllama-server-impl.so

All five now resolve to the merged build (7,637,792 bytes, md5 `aec02581`). The accepted runtime's server implementation is gone from those
paths.

## What this invalidates

- **The "stage also aborts" control in EXP355 is invalid.** The stage it ran was already the merged library by then, and the abort message even
  named `server-merge/server-context.cpp:4229`, which is my file. So the claim "pre-existing defect, the stage aborts too" is unproven and must
  be withdrawn until the original is restored and retested.
- Any future Apex gate run against `runtime/run` would be measuring the merged server, not the accepted one.

## What survives

- `libllama` and the `libggml-*` libraries are untouched; only the server implementation was overwritten.
- Recorded artifacts survive as files: the baseline content hash `e68df05b`, the raw request and response, the argv, and the memory records in
  `runtime/results/restore-exp150-32k128/`.
- Candidate backups of a server implementation exist, with different hashes: `EXP-347/build/stage-run` and `EXP-354/build/stage-run` hold the
  same overwritten content, but `EXP-2026-11-11-364-flint-kv-save-restore/build-stage-run-backup/libllama-server-impl.so` (7,619,408 bytes,
  md5 `aa360322`) does not, and neither do the per-experiment builds of EXP269, EXP271, EXP273 and EXP275.

## What I will not do without a word from Igor

I will not guess which backup is the accepted one and write it back over the shared target. Choosing wrong would silently change the baseline
that every measurement is compared against, which is worse than a known-missing artifact. The options are to restore the EXP364 backup after
verifying it against a re-run of the Apex baseline, or to rebuild the accepted server implementation from its sources -- and both need a
decision and a model run.

## The fix that prevents a repeat

`assemble_candidate.py` must remove the symlink before copying, so a candidate never writes through into a preserved runtime. That is a one-line
change and it is in this commit.
