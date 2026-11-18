# EXP-2026-11-18-973: the role counts describe a tree nothing builds

Status: accounting corrected. No runtime code; two misleading readings removed from
the report.

## Hypothesis fixed before changes

EXP972 found one asserted line that had become false. The natural follow-up was to
audit the rest of the report the same way: which lines are computed, which are
asserted, and which of the asserted ones no longer match the repository.

## What was found

### 1. `runtime/core` has no build target, and section 2 did not say so

Section 2 prints roles for `runtime/core`:

    superseded by Goose:  170 fns in 15 files
    still needed:          22 fns in 2 files

Both numbers are computed from the file list, so they are honest arithmetic. What
the report did **not** say is that no build produces `libbmoe_core`:

    find builds -name 'libbmoe_core*'   -> 0 results

So "superseded" describes a decision recorded in a document, not a replacement in a
shipped artifact. `docs/goose-migration/LINEAGES-20261116.md` already states this -
`bmoe_core` is "referenced by exactly two things, both accounting" - but the report
itself read as though these were live replacements.

Checked directly: none of the 15 "superseded" files calls any Goose export.

    runtime/core/src/config.cpp        goose_calls=0
    runtime/core/src/engine/session.cpp goose_calls=0
    runtime/core/src/moe/gguf_offsets.cpp goose_calls=0
    runtime/core/src/io/file_reader.cpp   goose_calls=0

That is consistent: the decision lives in a Goose module gated by an experiment,
and the C++ body was never wired because nothing builds it.

### 2. A missing file would have read as "zero functions"

`fn_count` returned `0` for any unreadable path. If a listed file were renamed or
deleted, the report would quietly shrink and look like progress. It now refuses
loudly and returns non-zero.

    $ fn_count /nonexistent/file.cpp
    migration-status: listed file is missing: /nonexistent/file.cpp
    0
    rc=1

## Result

| check | before | after |
| --- | --- | --- |
| section 2 build target | not stated | **"NONE - no build produces libbmoe_core"** |
| missing listed file | silently 0 fns | **error on stderr, rc=1** |
| `superseded by Goose` count | 170 fns / 15 files | unchanged (all 15 files present) |
| script exit code | 0 | **0** |

## Not claimed

Nothing about the migration changed. These 15 files were never live in any
artifact, so their status was already what the report now states; the correction is
to the reading, not to the code.

Whether `runtime/core` should be built at all - and whether its `superseded` bodies
should be deleted as dead code - is a separate decision. Rule 15 forbids deleting
before acceptance, and rule 17 requires the shared architecture support to survive,
so this report change is the safe part of the question.

No model was started.

## Reproduction

    bash tools/migration-status.sh | sed -n '/2. Own C/,+6p'
    find builds -name 'libbmoe_core*' | wc -l          # 0
    grep -cE '\bfg_?[0-9]{3}[a-z]_' runtime/core/src/config.cpp   # 0
