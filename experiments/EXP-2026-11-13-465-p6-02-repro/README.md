# EXP465: P6-02 reproduced locally

One run of our line with `-c 2048 -ngl 0 -np 1` and an 1 800-character prompt on
port 18094. The server becomes ready, the request closes the connection, two CUDA
errors are logged and the process dies with SIGABRT (exit 134).

`tests/repro.sh` reproduces it and `tests/repro.result` records the fields:
`CUDA_ERRORS=2`, `ABORT=1`, `SERVER_EXIT=134`, `LEFT_PROC=0`.

The backtrace again goes through `fg164_component` (the Goose scheduler) and
`fg163_io`, so the defect is in our scheduler code. The investigation note lists
the next three gdb probes.
