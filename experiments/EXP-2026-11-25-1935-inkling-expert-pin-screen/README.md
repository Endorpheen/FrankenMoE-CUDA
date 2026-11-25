# EXP-2026-11-25-1935: Screen Inkling expert mmap registration

**Verdict: REJECTED for the full pin profile.** No request completed
in a registered arm. Keep EXP-1934 with pageable expert weights.

The full EXP-1933 trace showed about 103 MB per token of expert
uploads. In the cumulative Inkling runtime, zero expert mmap
bytes were registered. This probe reconnects the existing
Inkling Goose registration module to the EXP-1934 host after
loading the expert-bank layout. Registration uses the shared
CUDA path, page-aligned spans, a byte ceiling, supported residency
checks and host RAM headroom. The CUDA objects and arithmetic
are unchanged. The isolated path requires `I2_INSTALL_PINS=1`
and honors the shared `GOOSE_EXPERT_PIN_MIB` budget.

## Startup and request screening

Both launches used RTX 4070, context 36864, batch 4096, ubatch 2048,
Q4_0 K/V, 576 MiB activation arena and the 6720 MiB expert cache.

| Registration limit | Request | CUDA registered bytes | Result |
| ---: | ---: | ---: | --- |
| 12800 MiB | 2048/256 | 13250002944, 48 ranges | CUDA vector error 1 and abort in the first prefill |
| 4096 MiB | 512/16 | 0 | Registration skipped for nonresident mmap pages; request timed out |

The full probe initially reported 14996735968 resident model
bytes. It failed at a graph MUL after registration succeeded.
The CUDA status and failing node are recorded. The precise
cause of the invalid argument is unresolved. The request
returned no data before the HTTP timeout. The system swap-out
counter rose by 331883 pages between this setup and the second
attempt. No throughput evidence can be drawn from either run.

During the second setup, resident model bytes had dropped to
1365139456. The registration guard rejected the planned
4204355584-byte subset before pinning. Swap-out did not
increase further through its bounded request. That run
disabled core dumping and was stopped after the timeout.
It is evidence about the guard, rather than a test of 4 GiB
registration performance on a resident mapping.

The first abort produced a 14735583938-byte systemd crash
dump. Keep core dumps disabled for follow-up screening runs.
Any future retry should first verify residency and system
memory stability, then isolate the CUDA vector failure with
a bounded non-model probe or minimal request.

The runtime and launch profiles in use were not changed.
The frozen Apex manifest passed. No Apex throughput gate ran.

## Reproduction artifacts

`source/pinned.goose` is the existing isolated Inkling pin module.
`source/inkling-backend.c.patch` connects it to the EXP-1934 host.
The build helper uses KeelWing and GCC 13 and reuses the EXP-1934
archive. `results.json` contains the exact startup settings,
residency counters, errors and timeouts. The artifacts document
a rejected configuration and are intended for investigation.
