# kimi-k3 structure cases (P6-05 architecture 10)

Gate: `tests/build.sh` builds `source/kimi-k3.goose` with
`base/model_graph.goose` at `-O1`, then runs `tests/fixture.cpp`:

```
FG-ARCH10 cases=126 values=226 failures=0
```

The expectations are a transcription of `src/models/kimi-k3.cpp`. No native
library is linked; the fixture parameter vector is synthetic (Kimi-K3 sized:
93 layers, 7168 hidden, 384 experts) and exercises rules, not a checkpoint.

## Fixture contract

`p[0..21]` is the parameter vector, `ffexp[0..92]` is the per-layer expert
feed-forward length (`n_ff_exp_arr`), and the marker array gives `n_head_kv`
per layer: zero on KDA layers, one on MLA layers (three KDA, one MLA, repeat).

| p | name | value | native |
|---:|---|---:|---|
| 0 | n_embd | 7168 | `n_embd` |
| 1 | n_vocab | 163840 | `n_vocab` |
| 2 | n_layer | 93 | trunk (`:31`, `:67`, `:277`) |
| 3 | n_layer_all | 93 | no nextn block |
| 4 | n_head | 64 | `n_head` |
| 5 | n_embd_head_kda | 128 | `:78` |
| 6 | ssm_d_conv | 4 | `:79` |
| 7 | n_embd_head_k_mla | 192 | `:109` |
| 8 | n_embd_head_v_mla | 128 | `:110` |
| 9 | n_rot | 64 | `:111` |
| 10 | n_lora_q | 1536 | `:107` |
| 11 | n_lora_kv | 512 | `:108` |
| 12 | n_ff | 18432 | dense lead `:140-142` |
| 13 | n_expert | 384 | `:146` |
| 14 | n_expert_used | 8 | `n_ff_exp` fallback |
| 15 | n_ff_exp fallback | 2048 | used when the array entry is 0 |
| 16 | n_expert_shared | 1 | `:161` |
| 17 | n_layer_dense_lead | 1 | `:139` |
| 18 | n_expert_latent | 1536 | `:56`, `:154-158` |
| 19 | attn_res_block_size | 8 | `:63-76`, `:288` |
| 20 | expert_gating_func | 1 (sigmoid) | `:40` |
| 21 | situ_linear_beta bits | `0x3f800000` (1.0f) | `:180` |

`ffexp[0] = 0`, `ffexp[1] = 1024`, `ffexp[2..92] = 2048`: layer 0 is the dense
lead and never reads the array, so its zero is the fallback path; layer 1
proves the array is read per block instead of a single global width.

## Cases

1. **Attention marker** — `n_head_kv == 0` means KDA, positive means MLA
   (`:31-33`); the fixture marker array maps layer 0 to KDA and layer 3 to MLA.
2. **Marker sanity** — a positive `n_head_kv` must divide `n_head` and not
   exceed it; zero is rejected because it would select KDA. MLA keeps one KV
   row per token, KDA reports `-1` (no KV cache).
3. **Plan** — layer 0 is `K_DENSE`, layers 1 and 92 are `K_MOE`, index 93 and
   negative indices are `-1`; `mk_plan` fills 93 kinds.
4. **No MTP** — `mk_mtp_supported() == 0`; a declared block count of 94 over a
   trunk of 93 is refused by `mk_plan_ok` instead of silently dropping a block.
5. **Residual attention** — on with `attn_res_block_size > 0`, off at zero;
   pushes at 0 and 8, not at 7 or 92; 12 checkpoints over 93 layers at a block
   of 8; zero checkpoints without a block; softmax width `n_ckpt + 1`; the mix
   is inactive while no checkpoint exists and when the feature is off.
6. **Latent width** — `n_expert_latent` if set, `n_embd` otherwise.
7. **Shared experts** — `n_ff_exp * max(n_expert_shared, 1)`: 2048 for counts
   0 and 1, 4096 for 2.
8. **KDA geometry** — `d_inner = n_head * head_dim = 8192`, fused QKV width
   `3 * d_inner = 24576`.
9. **MLA head split** — `nope = head_k - n_rot = 128`, per-head row
   `nope + v = 256`, latent projection `kv_lora + n_rot = 576`.
10. **Activation/gating** — gating functions 0..3 are legal, 4 and -1 are not;
    `situ_linear_beta > 0` enables the up transform (bits 1.0f on, 0 off,
    negative off); a present KDA bound key selects the lower-bound sigmoid.
11. **Globals** — embedding and output `{7168, 163840}`, output norm, the
    output res score, both per-layer norms and both res-score vectors at
    `{7168}`; all three res-score tensors disappear at a zero block size.
12. **Dense lead** — gate/up `{7168, 18432}`, down `{18432, 7168}`; no router
    on a dense layer and no dense gate on a MoE layer.
13. **KDA tensor set** — conv `{d_conv, 1, d_inner}`, fused QKV
    `{7168, 24576}`, `f_a {7168, 128}`, `f_b {128, 8192}`, `beta {7168, 64}`,
    `A_log {64}`, `dt_b {8192}`, full gate `{7168, 8192}`, `o_norm {128}`,
    `wo {8192, 7168}`; no MLA query and no KDA gate on the other branch.
14. **MLA tensor set** — low-rank `wq_a {7168, 1536}`, `q_a_norm {1536}`,
    `wq_b {1536, 12288}`; `wkv_a_mqa {7168, 576}`, `kv_a_norm {512}`,
    `wkv_b {512, 16384}`, absorbed `wk_b {128, 512, 64}` and
    `wv_b {512, 128, 64}`, output gate `{7168, 8192}`, `wo {8192, 7168}`.
15. **Plain query branch** — with `n_lora_q == 0` the layer owns
    `wq {7168, 12288}` and none of `wq_a`/`q_a_norm`/`wq_b`.
16. **Latent MoE** — router and probability bias at the full width, routed
    experts `{1536, 1024, 384}` on layer 1 and `{1536, 2048, 384}` on layer 2,
    `routed_down {7168, 1536}`, `routed_up {1536, 7168}`,
    `routed_norm {1536}`, shared experts at the full width.
17. **No latent length** — experts and router meet at `n_embd`, and the three
    routed adapters disappear.
18. **Shared-expert count** — 0 keeps one expert width, 2 doubles it.
19. **Fallback width** — an out-of-range layer index reads `p[15]` (2048).

## What is not checked

- No GGUF is loaded and no graph is executed: the gate proves the decisions
  and the shape table, not numeric output or runtime behaviour.
- Abort paths are not provoked: `mk_plan`'s `fail(15)` for a planned MTP block
  and `mk_nope`'s `n_rot > head_k` guard have no negative case (the shim would
  record them, but the gate deliberately stays on the accept side).
- The `wkv_a_mqa` row split (`kv_cmpr` view vs `k_pe` view) is a runtime view
  offset, not a declared tensor shape.
- The KDA state shapes (`n_embd_r`, `n_embd_s`), the conv-state rollback and
  the recurrent scan belong to the KV/state subsystem.
- `expert_weights_scale`/`expert_weights_norm` are forwarded to
  `build_moe_ffn` and change no declared shape, so they have no case.
