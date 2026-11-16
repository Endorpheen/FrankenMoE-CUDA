# EXP-2026-10-31-122: release KV physical pages after explicit reset

Status: NOT_RUN (measured bound recorded, implementation not completed).

EXP101 has no unmap API; the 261632-token run of the previous series mapped the
full 1440 MiB and retained it. EXP121 shows that at 32K only ~190 MiB of KV is
physical, so releasing after reset cannot improve the 32K peak by more than that;
after a long request it can return up to 1440 MiB of retained idle memory.

Design (not implemented): a registry proc `ggml_backend_cuda_lazy_release`
synchronizes the device, unmaps granularity-aligned mapped intervals, keeps the
virtual addresses, and leaves the next `apply()` to remap+zero. Trigger from
`llama_kv_cache` only when no sequence uses the cache (seq_rm/clear with used=0).
Must preserve checkpoints/prompt cache invariants and captured-graph addresses.
No claim was made.
