# Remaining Apex scope: CPU flash attention, CPU-only milestone, fused/cold experts

These three requirements describe paths the agreed Apex execution does not use.
They are closed on the Apex scope and assigned to compatibility work; the code
or test is not claimed as done.

## P2-09 -- complete CPU Flash Attention subsystem

Apex attention runs on the GPU (`-fa on`, `-ngl all`); the CPU Flash Attention
subsystem is not invoked by the fixed profile. The GPU FA path is P3-12
(standard Q4, accepted). CPU FA variants, cache dtypes, masks and backward modes
are C2/P10-10 compatibility. No Apex attention trace exists because the CPU path
is not on the trace.

## P2-17 -- CPU-only functional milestone across model families

The product runs Apex with GPU attention and CPU MoE, not CPU-only inference.
The CPU kernels actually called are the two Q4_K/Q6_K expert dots from the
CPU-math inventory. A CPU-only model-family matrix is a separate compatibility
milestone (C2/P10-10), not an Apex acceptance item.

## P1-08 -- fused and cold expert execution orchestration

On the Apex architecture:

- the fused `gate_up_exps` path is dead -- forcing `if (false)` changes nothing
  (APEX-FINAL);
- cold experts are absent from the loader/profile;
- the stock expert path is Goose and live: FG143 control/executor, FG147 backend,
  FG149 pool, expert cache/admission/upload decisions EXP150/262-265.

Other fused/cold variants, bank remapping for other formats and non-stock
executors are C2 compatibility.

## What remains genuinely blocked

- **P4-03**: the deployed `libggml-base` cannot be reproduced from the recorded
  source, and a rebuild changes the numeric baseline; the prepared constructor
  fix cannot be applied without relinking that base.
- **P6-02**: the scheduler/CUDA-split abort was escalated after eight attempts;
  it needs senior-helper/compiler guidance.

These are not scope decisions; they stay open.
