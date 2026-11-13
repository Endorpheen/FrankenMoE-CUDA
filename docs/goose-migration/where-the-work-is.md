# What is left, and why each remaining candidate is not a port

I spent a session's worth of iterations finding live candidates and kept landing on code that either is
not arithmetic or cannot cross the module boundary. Recorded here so the next session starts from the
map instead of re-deriving it.

## Ported and live (21 route prefixes in `libllama-exp233`)

Ten KV/recurrent routes, five Unicode/tokenizer routes, the loader, graph decisions, the CPU quant
batch, the CPU MoE executor, the CUDA kernels up to MMQ dispatch. Measured call counts, per request:

| route | calls |
|---|---:|
| `fg228e` cell ext | 65,814 |
| `fg217c` whole-string UTF-8 | 255,553 |
| `fg213p` piece copy | 321,941 |
| `fg228e`/`fg229i`/`fg227n`/`fg233c` | 65,814 / 292 / 146 / 146 |
| `fg232r` state ranges | 9 |
| `fg230s`, `fg231a` | 0 |

## Examined and rejected, with the reason

| candidate | why not |
|---|---|
| `llama_sampler_softmax_impl` | working set is **20 elements** — `top_k` truncates first (`sampler-not-a-target.md`) |
| `llama_memory_recurrent::find_slot` | passes are sequentially dependent through per-cell `std::set` mutations (`find-slot-scope.md`) |
| `state_write_meta` / `state_read_meta` / `state_write_data` | almost entirely `io.write` / `io.read`; the arithmetic already moved in EXP232 |
| `set_input_kq_mask` | with FA on, the mask path is swizzled and not executed |
| `output_reorder` | no output swaps in this profile |
| `set_input_v_idxs` transposed branch | `v_trans` is false when FA is on |
| `build_cvec`, `build_hadamard_input` | `cvec`/`hdmd` are null for qwen35moe |
| `build_norm`, `build_inp_embd` | decision wrappers around `ggml_*` node creation; two of three embd paths dead |
| `ggml_cpu_extra_compute_forward`, `ggml_cpu_extra_work_size`, `traits.cpp` | dispatch loops, no arithmetic |
| `quantize_row_q8_K_ref` | already reachable through the EXP148 CPU quant batch |
| `used_max_p1`, `pos_min` | one-line accessors; the boundary would cost more than the body |
| `mm_ids` other widths | native handles them through its `template == 0` path, which Apex never takes |

## The honest shape of the remaining work

The migration's cheap and medium targets are done. What is left falls into three buckets, none of which
is "port another function":

1. **MMQ / MUL_MAT** — 45% of kernel time, ported byte-exact, blocked on a performance question whose
   measurement this host cannot resolve. Not a porting task.
2. **Graph-builder DSL (P6-03)** — needs a decision from Igor and Codex about whether graph construction
   is expressed in Goose at all, at what granularity. Not a porting task.
3. **Native removal (P4-16)** — deleting the renamed originals once coverage is proven. A verification
   task that depends on a fresh census, which `ncu` refuses to give on this machine.

Continuing to port small helpers would keep producing commits with gates and no live effect. That is
worth stopping and saying plainly, which is what this file is for.
