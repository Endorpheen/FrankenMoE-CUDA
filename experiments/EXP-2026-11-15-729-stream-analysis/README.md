# EXP-2026-11-15-729 — why overlap is zero: 99.2% of expert copies share the compute stream

Base: `5abe72dd`. EXP-726 measured 0% overlap between expert copying (4.24 s) and kernel work
(7.93 s). EXP-728 closed the placement lever. This card answers *why* there is no overlap, at the
level of CUDA streams, and it is a structural answer rather than a scheduling accident.

## 1. The stream distribution

| activity | stream | count |
|---|---|---:|
| kernels, decode window | 14 | 21,057 |
| HtoD copies | **14** | **76,989** |
| HtoD copies | 13 | 592 |
| DtoH copies | 14 | 2,153 |

**Every kernel runs on stream 14. 99.2% of the expert copies run on stream 14 as well.**

## 2. Why that means zero overlap

Work on one CUDA stream executes in issue order - there is no overlap between a copy and a kernel on
the same stream, by definition of the programming model. So the 4.24 s of copying cannot hide behind
the 7.93 s of compute: they are the same queue.

This is not a scheduling heuristic that could be tuned. It is the structure of the graph the expert
cache builds: `impl::copy` creates copy nodes, the scheduler places them in the compute graph, and
the graph runs on one stream. The copies are correctly ordered and correctly synchronised - they are
simply serialised.

The 592 copies that do use stream 13 are 0.8% of the total and cannot move the needle.

## 3. What would be required to overlap

A second stream with explicit dependency edges: expert copies for layer N+1 issued on stream 13 while
layer N computes on stream 14, joined by events so that layer N+1 does not start before its experts
arrive. That is standard CUDA pipelining, and the ggml backend API already exposes the pieces
(`ggml_backend_graph_compute_async`, event-based scheduling in `ggml_backend_sched`).

But it cannot be done by a flag, and it is not a small change:
- the scheduler would need to know that a copy and a compute node are independent;
- the expert cache currently expresses copies as ordinary graph nodes, so the scheduler sees one
  graph and no independent work;
- getting the dependency edges wrong risks reading an expert before it arrives, which changes the
  answer rather than just the speed.

## 4. Honest position

Three levers have now been examined and closed:

| lever | verdict |
|---|---|
| more cache / more resident experts | closed by arithmetic (16.9 GiB of experts, 12 GiB device) |
| `--n-cpu-moe` placement | closed by OOM in four configurations |
| polling / scheduling flags | measured: `--poll 50 --poll-batch 1` gives 39.80 versus 39.86, parity |

What remains is the stream-level change above, and it is the only path to the 4.24 s. It is also the
largest single opportunity measured in this session: 4.24 s of a 17 s decode window.

## 5. What I did not do

Did not attempt the stream change. It requires modifying how the scheduler and the expert cache build
their graph, and the failure mode - an expert read before its copy lands - is a wrong answer, not a
slow one. That warrants its own experiment with a correctness gate on the output hash, not a change
made at the end of a long session.

## 6. Cost

One server run (the `--poll` measurement). The stream analysis is from the existing profile.
