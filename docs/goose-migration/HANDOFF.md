# Handoff — batch/output block, EXP237–244

Written at the end of a long session. The next session starts from this file, not from conversation memory.

## Where the work stands

Eight Goose routes cover the batch and output path. Five are live at 146–292 calls per request; three are
dormant, each with its reason taken from the guard in the source plus a zero witness — never inferred.

| route | prefix | calls | state |
|---|---|---:|---|
| split_equal expansion | `fg237s_` | 146 | live |
| split_seq selection | `fg238q_` | 0 | dormant: `embd_all` path not taken |
| batch entry classification | `fg239b_` | 146 | live |
| sequence-set construction (256-bit) | `fg240q_` | 146 | live |
| ubatch_add classification | `fg241u_` | 146 | live |
| out_ids + seq_idx | `fg242o_` | 292 | live |
| compat positions | `fg243c_` | 0 | dormant: legacy `llama_batch` entry point |
| output reorder | `fg244s_` | 0 | dormant: requires `n_outputs > 1` |
| SET view | `fg245v_` | — | gated candidate, deliberately NOT in the DSO |
| PAD index map | `fg246p_` | — | gated candidate, deliberately NOT in the DSO |

All gates re-verified against the shipped DSO: 89920 / 61059 / 67640 / 300008 / 160048 / 1680413 / 261197 /
325375 checks, zero failures.

## The single most important finding: division and modulo differ from C

Goose's `%` is Euclidean, C's keeps the sign of the dividend — `-5 % 10` is `5` in Goose and `-5` in C, and `/`
diverges the same way. EXP246 caught this the honest way: a direct transcription of `pad.cu`'s
`wrap_around(coord, size) = (coord + size) % size` produced **30 wrong indices in 10234 checks**, with the module
returning `+5` where the reference returned `-5`.

`docs/goose-migration/MODULO_SEMANTICS.md` holds the sweep of the eight shipped routes: seven have no `/` or `%`
at all, and the single site (EXP240 `sid/(64 as i64)`) is **verified safe** because its guard sits one line above
the division. That file also states the standing rule for new modules.

Read that file before porting anything that indexes or wraps.

## Verified performance

`exp238` vs `exp242`, four-step pair (old → new → new → old), only `libllama.so.0.7.0` swapped:

| | prefill tok/s | decode tok/s |
|---|---:|---:|
| old mean | 2493.52 | 24.547 |
| new mean | 2497.46 | 24.664 |
| delta | **+0.16%** | **+0.48%** |

**Prefill verdict is sound; decode is not.** Within-series decode spread reached 6.87% — three times the 2%
threshold. Four runs bound a decode regression at roughly ±4%, not ±2%. Do not quote a decode verdict from a
single pair. Prefill spread stayed at 0.07–0.43% and is the only trustworthy signal available here.

## How to run a measurement (learned the hard way)

- one `FG_STAGE` per campaign and a distinct `FG_SUFFIX` per run; the runner must derive **both** the results
  and measured roots from them — a hardcoded path made four runs overwrite one `summary.json`, and a fifth run
  returning figures identical to two decimals is what exposed it;
- switch DSOs by **symlink**, never by copying a `run` directory (648M, and a partial copy from a full disk is
  invisible until the runner fails minutes later with `No such file: libggml.so`);
- check `du -sh` against 648M before trusting a stage directory;
- verify the port is free rather than trusting `setsid` — without `--wait` it returns immediately;
- write link outputs **outside** the directory about to be rebuilt (I deleted one DSO that way).

## Repeated mistake classes, with the check that catches each

1. **Pristine-source rebuild drops the previous route.** Caught by `nm -u <object> | grep fg` before linking.
2. **Derived runner keeps the old path.** Caught by grepping the generated script for the new stage name.
3. **Fixture generator does not span the trap, so a real mutation passes.** Widening two generators raised the
   same mutations from 2 and 0 failures to 5344 and 30855. When a mutation passes, first suspect the generator,
   not the module.
4. **Guessing instead of instrumenting.** Three theories about a linked-but-silent route were wrong; one
   unconditional counter (`block=146 guard=146 flat=146`) settled it in a single run.

## Dequantisation block, EXP248–262

Every path in `ggml-cuda/dequantize.cuh` now has a Goose implementation, gated
against the native template compiled from the read-only source into the same
fixture:

| group | experiments | destinations |
|---|---|---|
| K-quants | EXP182 Q4_K, EXP248 Q6_K, EXP249 Q5_K, EXP250 Q2_K, EXP251 Q3_K | F32 |
| IQ lattice-free | EXP252 IQ4_NL, EXP253 IQ4_XS | F32 |
| IQ grid tables | EXP255 IQ2_XXS, EXP256 IQ2_XS, EXP257 IQ2_S, EXP258 IQ3_XXS, EXP259 IQ3_S, EXP260 IQ1_S, EXP261 IQ1_M | F32 |
| MX | EXP254 MXFP4 | F32 |
| legacy 32-element | EXP262 q1_0, q2_0, q4_0, q4_1, q5_0, q5_1, q8_0 | F32 pair |
| q4_0 8-block | EXP188 | F16 |

All are **OFF_PATH** and deliberately not linked: the fixed 32K/128 profile never
calls `ggml_get_to_fp32_cuda` for these types, because Q4_K_M executes through
MMQ. Each module carries a differential gate (native oracle in the fixture,
bitwise comparison), a host-plan gate where a row wrapper exists, and five
mutations. The shared grid/sign tables (`iq2xxs_grid`, `iq2xs_grid`, `iq2s_grid`,
`iq3xxs_grid`, `iq3s_grid`, `iq1s_grid_gpu`, `ksigns_iq2xs`) are imported as
explicit device constant primitives — data, not code; every index and arithmetic
operation is Goose.

### Non-F32 destinations, EXP263–272

The same templated functions were then reproduced for `half` and
`nv_bfloat16`, two destinations per module: Q4_K (EXP263), Q6_K (264), Q5_K
(265), Q2_K (266), Q3_K (267), IQ4_NL+IQ4_XS (268), IQ2_XXS+IQ2_XS (269),
IQ2_S+IQ3_XXS (270), IQ3_S+IQ1_S (271), IQ1_M+MXFP4 (272). Only the store
differs: `__float2half` for the implicit round-to-nearest-even conversion and
`__float2bfloat16` for bf16. Every module is gated on both destinations against
the native instantiations (83 200 values per destination) with five mutations,
including one that swaps the bf16 store for the half conversion. The legacy
32-element callbacks (EXP262) are destination-independent — the cast lives in
the shared `dequantize_block_cuda` template — so they need no variants.

The F32/F16/BF16 matrix for every templated dequantizer in `dequantize.cuh` is
therefore complete. The activation quantizer (`quantize_mmq_q8_1`) is the only
dequantisation-adjacent piece still native, and it is blocked on the same
emitter address typing as MMQ.


Defect classes found in this block, all by the gate rather than by reading the
formula: block-layout conflation (Q2_K dm at the end of the block, IQ1_S stride
50 not 66), guard-index vs block-index conflation (IQ4_NL), fixture
under-allocation (MXFP4), an unaligned word read (q5_0), and a fixture whose
failure counter was invisible to `cudaMemcpyFromSymbol`, which made every
mutation report zero failures while the run looked green. The last one is the
most dangerous and is worth checking first in any new fixture.

## What is next, in order

1. `ROLL` — closed by EXP247. `SET` (EXP245), `PAD` (EXP246) and ROLL stay
   implemented and gated but not shipped: a route earns a place in the shipped
   DSO by being reachable, not by being finished.
2. Other `n_expert_used` widths for `mm_ids_helper`; only the `int)8`
   specialisation is ported and the others are dormant for this profile.
3. Dequantisation — closed: EXP248–262 cover every `dequantize.cuh` path for
   F32, and EXP263–272 cover `half`/`nv_bfloat16` for every templated function.
   The activation quantizer (`quantize_mmq_q8_1`) remains native and is blocked
   on the EXP180 emitter typing, as is the shared `dequantize_block_cuda`
   launch template.
4. MMQ / MUL_MAT — the largest remaining native share (`mul_mat_q` Q4_K 30.3% +
   Q6_K 9.7% ≈ 40% of GPU time). Blocked on EXP180's codegen gap: 824 SASS
   instructions against native's 160, including 206 `IMAD` against ~20. The next
   idea recorded there is to precompute all address arithmetic host-side so the
   kernel only does `base + i0`. Six attempts were made; the blocker rule was
   reached.
5. Dense `out-prod` — closed by EXP288. The file is one 20-line pointer kernel
   plus a host function that does nothing but pick one of three cuBLAS branches;
   the kernel is bit-identical on 3 519 pointers and the branch decision,
   `lda`/`ldb`/`ldc`, op, batch count, strides and 16x16 pointer geometry are
   gated separately, 10/10 mutations caught. cuBLAS itself stays an external
   primitive. Trap worth remembering: the native assert checks
   `(src1_T ? nb11 : nb10) == 4` while `ldb` reads the other pair.
6. Dense `mul_mat_vec_f` — closed for every source type this profile can carry.
   EXP184 covered float (one destination column, no fusion); EXP287 adds half
   and bfloat16 to the same body, 3 696 values bit-identical against a verbatim
   native copy, eight mutations caught. The lesson that cost the first draft:
   the native kernel advances `y` in floats and then indexes it as `float2`, so
   `yb*4 + col2*8` is the pair offset — `(yb+col2)*8` is invisible while the
   channel base is zero, which is why channel 0 passed and channel 1 did not.
7. CUDA shared-memory kernels — unblocked by EXP279 and closed by EXP280–285
   (softmax forward mask-free/mask/sinks, backward, cooperative) and EXP283
   (bitonic argsort). The pattern: explicit shared base/load/store/
   `__syncthreads`/shuffles plus a statement-by-statement port.
8. CUDA recurrent kernels (RWKV6/7 `wkv.cu`, GLA `gla.cu`) — **blocked on local
   arrays**. Each thread keeps `float state[head_size]` with `head_size` 64 or
   128 in registers, and Goose has no local array type; the device backend was
   deliberately kept free of the stack allocator (the emit gate asserts no
   `gg_allocate`). Two honest options, neither taken blindly yet: an explicit
   scratch-memory contract for the state (values identical, memory pattern
   different, needs its own gate) or a language array feature (the compiler is
   read-only). EXP172 already covers the GDN scalar/MMA path, so this is the
   remaining recurrent scope only.
9. KV cell maintenance — EXP289 closes the `pos_add`/`pos_div`/`seq_pos`
   remainder of P5-01 that EXP199 and EXP204 left open: 957 717 checks against a
   transcription of `llama_kv_cells`, 8/8 mutations caught (two of them by an
   invariant abort, so the driver counts exit status as a channel). The module is
   gated but not integrated — the source tree is read-only. Two traps recorded
   there: `seq_pos` as `std::set<std::pair<pos,uint32_t>>` needs an *unsigned*
   index or the `upper_bound({p, UINT32_MAX})` sentinel becomes -1 and skips
   cells at exactly `p`; and a `const` in a Goose module reads as zero unless
   `gg_program` ran, so compile-time constants are literals.
10. KV slot search — EXP290 closes the `find_slot` item that EXP199/204/289 left open,
   together with its SWA predicate. 39 032 predicate checks and 2 455 slot-search
   checks against the native function and a transcription of the class, 7/7
   mutations caught; one candidate mutation was rejected because no output could
   observe it. `seq_pos_max` is a scan here rather than the cached set — same
   value by the set invariant, different cost, and this module is not timed.
11. KV range operations — EXP291 closes the CP/idxs remainder: `seq_rm(seq_id,p0,p1)`
   in both branches, `seq_keep(seq_id)` and the same-stream `seq_cp`, all with the
   head-move rule. 3 937 168 checks over 120 trials x 40 operations, 8/8 mutations
   caught. The native `seq_add` precondition (destination bit absent) had to be
   modelled explicitly: a random state carrying both sequences is undefined
   behaviour, and the 14-vs-12 pair mismatch that appeared first was that, not a
   port defect. Cross-stream `seq_cp` stays open — it enqueues a buffer copy
   (sc_info) and rebuilds the destination cells, which is buffer work, not cell
   metadata. It is recorded, not approximated.
12. Cross-stream `seq_cp` — closed by EXP292: the full-buffer test, the reset of
   the destination stream, the per-cell rebuild (undo the shift, pos_set, seq_add,
   re-apply the shift through pos_add, copy ext) and the head handover, 130 370
   checks / 0 failures, 9/9 mutations caught. The only unported part is the buffer
   copy itself: native queues it in sc_info, and the port reports the pair. With
   this, P5-01 has no open cell-metadata item left; only the live run remains, and
   that needs a model run and Igor's approval under rule 1.
13. Recurrent slot placement — EXP293 transfers `llama_memory_recurrent::find_slot`
   whole (P5-04): head reset, shared-sequence sweep, ring search, slot assignment
   with tail copying, the re-order pass with tail retargeting, final position and
   set write, source-reference staging, and the range/used recount. 131 200 checks,
   8/8 mutations caught, re-order exercised 1 233 times. Note the cell emptiness
   rule here is the sequence set, not the position. And a trap: using `src0` as
   scratch for the reference count clobbers cells outside the range, which native
   leaves at -1 — the gate caught it.
14. Recurrent sequence operations — EXP294 transfers `seq_cp`, `seq_keep`,
   `seq_add` and `seq_div` for recurrent state: 1 170 000 checks against a
   transcription and 8/8 mutations caught. A recurrent cache owns one cell per
   sequence, so the position bounds clamp and select a cell instead of walking a
   range — and `seq_cp` clamps bounds it never reads, which the port preserves
   rather than "fixing". Still open in P5-04: state write/read, rollback, stream
   ordering, and the live recurrent witnesses.
15. Recurrent removal — EXP295 transfers `seq_rm`, `set_rs_idx`, `clear` and
   `state_clear`: 951 126 checks, 8/8 mutations caught. The rollback branch is the
   subtle part — single-use snapshot, bounded by `n_rs_seq`, refused for a cell
   shared by several sequences, and position-only when the memory is empty. Two
   notes worth keeping: native compares `seq_id` as unsigned against `n_seq_max`,
   so negative ids are rejected before the `seq_id < 0` branch, which is therefore
   dead; and a uniform position draw never reaches the rollback branch, which is
   why three mutations first looked harmless. The gate now aims half the removals
   at the sequence's own cell and probes the exact snapshot bound.
16. Recurrent state metadata — EXP296 transfers `state_write_meta` and the
   whole-cache branch of `state_read_meta`: 44 480 checks, 8/8 mutations caught,
   write compared byte for byte and read fed the exact bytes the transcription
   produced. The tensor payload around the metadata stays native. Note: the
   single-sequence read branch calls `find_slot`, already covered by EXP293, and is
   not duplicated. Gate lesson: injecting the rejected cases everywhere made the
   exact-capacity boundary untestable, so every sixteenth trial now draws a clean
   stream.
17. Recurrent payload plan — EXP297 transfers the transfer-record arithmetic of
   `state_write_data`/`state_read_data` (which tensor, which offset, which size,
   in which order, plus every type/row/header check): 16 692 checks, 8/8 mutations
   caught, 120 rejected streams. The bytes stay native. Two lessons: a helper that
   multiplies its index by the slot size must not be called with an
   already-scaled index — the error showed only in `size` because the first cell's
   offset stayed right — and a cursor counted in fields in one arm and bytes in the
   other reports eight times too much. One candidate mutation was rejected as
   unobservable: accepting a larger layer count cannot be seen because field
   consumption follows the model, not the header.
18. SWA cache policy — EXP298 opens P5-03 with the iswa pair: the padded SWA size
   (unified scaling, `swa_full` override), the base/SWA layer filters, the
   delegation rules (`seq_rm` is an AND, position observers read the SWA cache
   because the base is a superset) and the memory-breakdown merge in key order.
   10 412 checks, 8/8 mutations. `init_batch` is not part of it: the ubatch split
   belongs with the batch allocator. Gate defect worth remembering: a breakdown
   fixture that draws duplicate keys describes an input the native map cannot
   produce, so the merge comparison was testing the wrong thing.
19. MSA cache policy — EXP299 adds the main+indexer pair: derived indexer
   hyperparameters (one key head per layer, indexer head width, rope unchanged),
   `n_stream = unified ? 1 : n_seq_max`, two independent filters, and the delegation
   rules. The contrast with iswa is the point: position observers read the *main*
   cache here, because the indexer is not a superset of anything, while iswa reads
   the SWA cache because its base is. 14 099 checks, 7/7 mutations. The position
   check passes values, not a constant, so reading the wrong cache shows as a wrong
   number.
20. DSA cache policy — EXP300 adds the MLA+lightning-indexer pair: indexer MQA
   with one key head, indexer head width, rope forced to NeoX (unlike MSA), two
   independent filters, position observers on the MLA cache, `can_shift` including
   the size equality, and `state_read` as a transaction that clears the MLA part
   when the indexer read throws. 16 495 checks, 8/8 mutations. Harness lesson: an
   operation that writes through an omitted output buffer segfaults instead of
   mismatching, so a gate that can crash is not measuring the module.
21. DSV4 policy helpers — EXP301 transfers the compressed-state decision helpers
   (comp size, used key rows, stream offset with the native rejection, coupled
   detection, token membership, graph stream fan-out, K-only hparams, plane count)
   from the dsv4 cache: 9 000 checks, 8/8 mutations. Two notes: the native
   stream-offset function throws, and an FFI entry cannot, so the contract is -1
   and the gate pins it; and a comparison written as `x>(1 as i64)` does not parse
   -- the bracket is read as a generic and the parser wants a closing `>`; write
   `x > 1 as i64`.
22. DSV4 comp plan (non-overlap, no rollback planes) — EXP302 transfers
   `dsv4_build_comp_plan` for `overlap == false` and `n_rs_seq == 0`: n_visible,
   state_pos, n_kv, write/read lists with the three-way source, the CSA and HCA dummy
   blocks, and the sorted persist rows. 87 911 checks, 8/8 mutations. Still open in
   this plan: the overlap index layout and the restore/snapshot planes. Four traps
   recorded: an output section sized "a few entries per token" overflows when a dummy
   block writes `ratio` entries (use separate arrays); the native token lookup keeps
   the *last* token for a duplicated (sequence, position); `GGML_PAD(n_kv, 256)` hides
   "max" versus "last" unless a position above 1024 is drawn; and the HCA dummy's
   assert (last slot must not be live) has to be modelled so invalid draws are skipped
   instead of faked.
23. DSV4 comp plan, overlap layout — EXP303 adds the `overlap == true` read layout:
   previous-window halves for every block, then current-window halves, collected
   separately and concatenated once, with dummy blocks appending to both. 124 118
   checks, 8/8 mutations. My own slip, caught by the count before the contents: a
   text substitution missed two dummy sites because their indentation differed, so
   they kept writing to the shared list. Compare counts first in every plan gate.
   Still open in this plan: the restore/snapshot planes (`n_rs_seq > 0`).
24. DSV4 rollback planes — EXP304 closes the comp plan: the restore/snapshot tail
   for `n_rs_seq > 0`, emitted for every layout stream so graph widths never depend
   on the batch. 162 768 checks, 8/8 mutations. The trap: `rs_idx` is looked up by
   sequence id against the *table length*, not the stream count — equal until a
   legal id falls outside the stream range, and wrong from the first such batch.
   One candidate mutation was rejected as unobservable (the prefix condition at
   equality is zero either way).
25. DSV4 compressed-state operations — EXP305 transfers `clear`, `seq_cp`,
   `apply_copies`, the stream-range helpers and the stream ids `state_write`
   computes: 17 780 checks, 8/8 mutations. Native throws on every rejection and the
   FFI contract is -1. Two harness slips, one of them a repeat: the dispatcher's `op`
   was never written into the packet, so every call ran op 0; and the apply records
   were written at `2*i*8` through a helper that already scales by 8. **Rule now
   recorded: a helper that takes an index must never receive a byte-scaled offset** —
   this is the second occurrence (EXP297 was the first).
26. DSV4 state tensor I/O — EXP306 transfers the per-stream transfer records and
   metadata checks of the compressed state: 9 924 checks, 8/8 mutations. A
   representation defect worth carrying: pointer-plus-length cannot tell a null map
   from an empty one, and native does distinguish them; there is now an explicit
   presence flag. **Length zero does not encode presence.** Still open in P5-03:
   `init_batch` (belongs with the batch allocator) and the remaining dsv4 state
   read path.
27. DSV4 compressed-state restore plan — EXP307 transfers the header validation,
   destination range and per-layer call order of `comp_state::state_read`, with
   distinct rejection codes and a read count that exposes a skipped or duplicated
   read: 5 000 checks, 8/8 mutations. The lesson is now written as a rule: every
   inequality rejection needs a probe on both sides — a fixture that only corrupts
   upward leaves `!=` weakened to `>` undetected, and this is the second occurrence
   (EXP306 was the first).
28. DSV4 cache construction policy — EXP308 transfers what the five pieces are
   made of: K-only copies for raw/CSA/HCA/indexer, the indexer's one key head at the
   indexer width on NeoX rope, the two ratio filters, the padded compressed sizes, the
   three compressor-state shapes, and the two deliberate `false` flags (raw and
   compressed stay per-sequence even in unified mode). 31 432 checks, 8/8 mutations.
   The flags are the part worth guarding: a later "simplification" that follows the
   public setting would show here instead of at a recall regression.
29. Hybrid indexed memory — EXP309 opens P5-05 with the pool staleness rules:
   minimum edited position, regroup-from-zero at or below the sequence's first
   position, the recurrent-refusal short circuit in `seq_rm`, the shifted mark for a
   negative `seq_add`, and the destination/all/sequence marks of
   `seq_cp`/`seq_keep`/`seq_div`. 19 362 checks, 8/8 mutations. The narrow margins
   are the two boundaries (edit exactly at the first position, shift that crosses it),
   which is why the generator draws `p0` and `seq_min` from overlapping ranges.
30. Hybrid pool layout — EXP310 transfers kpool_pad, kpool_rank and the
   per-sequence layout update: resume from the stored scan point when clean and the
   first position still matches, rebuild (dropping the scan point) when stale or
   shrinking, and the two grouping modes (by order, by position with alignment and
   consecutive positions). 32 147 checks, 8/8 mutations. Two candidate mutations were
   rejected as unobservable, not missed: the `max(64, …)` floor is dead code and
   resuming from an empty layout at scan point zero scans the same as rebuilding.
   Replaced by observable ones — an extra pad block and a rebuild that keeps the old
   scan point.
31. Hybrid indexed state ordering — EXP311 transfers the write suffix order, the
   mirrored restore order with the layout hand-over, the failure transaction that
   drops the sequence from all three caches, `state_drop` clearing directly, and the
   ubatch stream count: 22 290 checks, 8/8 mutations. Gate lesson: a "does nothing"
   mutation is only visible if the output buffer is sentinel-filled between calls,
   otherwise it reads back the previous result.
32. Hybrid k-pool state marking — EXP312 transfers `kpool_build_state`: the
   generation-stamped marks, the stale pass (pools ending after an edited position),
   the token pass in both modes, the representative-row uniqueness, and the graph-size
   bound. 23 587 checks, 8/8 mutations. Four defects of mine, all in the interface
   rather than the rules: `POS_CLEAN` is the maximum position (I used -1, which made
   the stale pass run every step), `kpool_rank` searches cells and I passed the pool
   count, the pool-offset table and the pool array were one pointer, and a vector
   sized with `assign` then filled with `push_back` keeps its zeros at the front. The
   bound mutations only became visible once the generator made the bound actually
   bind.
33. Hybrid indexed context stepping — EXP313 transfers `next`, `apply` and
   `kpool_track`: staleness marks are cleared only on the first step (the ubatch that
   exists to consume them), apply ANDs both sides and only then extends the layout and
   pins the pool step, and track is derived rather than cached. 19 405 checks, 8/8
   mutations. The two track mutations are the narrowest because each disabled
   condition needs a draw satisfying every other one, which is why flag combinations
   are exhaustive.
34. Hybrid host pool inputs — **closed by EXP314** after the rewrite the previous
   handoff asked for. The arena now carries one `seqdesc` table (stream, cell range,
   pool range) and both arms read it; the earlier divergence was the harness
   describing the same facts three times. Transfers the pool tables (representative =
   last member's row, member cells, sentinel for absent sequences, distinct padding
   cells, M-RoPE positions) and the masks (visible prefix by rank or position,
   selection slots, tail cells). 212 205 checks, 8/8 mutations. The position stride is
   the real pool count here because `n_new_g` belongs to EXP312. Three further
   harness defects: an empty sequence read past its cells, tokens naming sequences
   without cells (native asserts a rank), and comparing the selection mask when the
   graph had not asked for it.
35. Base hybrid memory — EXP315 transfers the layer split between the attention and
   recurrent caches (each with its own external filter, otherwise the recurrent pattern
   decides), the batch split choice (all-output by sequence, else equal with the
   non-sequential form for a non-unified attention cache and the rollback window kept
   inside one ubatch), and the prepare order (recurrent first, because it can refuse and
   the attention preparation is the expensive one). 23 582 checks, 8/8 mutations.
36. KV attention mask — EXP316 transfers the float path of `set_input_kq_mask`:
   the empty/sequence/causal/SWA drops for all three window types, plus the row-reuse
   shortcut with its recorded set. The oracle is a full pass per token, so the shortcut
   is tested against what it optimises away rather than against itself. 132 213 checks,
   8/8 mutations. One candidate mutation was measured and rejected: weakening the
   recorded-window comparison is unobservable because a boundary cell's verdict is the
   same for every token of its sequence. Still open in this area: ALiBi, 2-D/M-RoPE,
   the half path, and the non-causal SWA-full span.
37. KV mask, full float path — EXP317 adds ALiBi (signed distance, and row reuse
   disabled because the value depends on the query position), the 2-D/M-RoPE causal
   comparison (same position, y then x), and the non-causal SWA-full span. 115 277
   checks, 8/8 mutations. Still open for this function: the half-precision path only.
38. KV view shapes and n_kv — EXP318 transfers the K/V view shapes, strides and the
   stream offset, both V layouts, and the `n_kv` padding (`max(n_pad, 256)`, clamped to
   the cache size, maximum over streams). 29 000 checks, 8/8 mutations. Note that the
   plain-V token stride uses the GQA width, not `head_v * n_head_kv` — the variable-V
   case, which is easy to "simplify" away.
39. KV copy-in plan — EXP319 transfers `cpy_k`/`cpy_v` operation sequences: the head
   merge, the cross-stream reshape for global indices, the contiguous-copy versus
   reshape choice for a transposed V, the width pad, and the flattening to one element
   per row; plus the V index shape (one per token, or per token and value element when
   transposed). 30 201 checks, 8/8 mutations.
40. KV rotation matrices — EXP320 transfers the Walsh-Hadamard generator (Sylvester
   recursion, signs compared and orthonormality checked on the reconstruction) and the
   `n_rot_k`/`n_rot_v` policy (K doubles from 64 while the key width is a multiple of
   twice the size; V is fixed at 64 for quantised V with a 64-multiple head width; the
   indexer architectures rotate when the key width equals the indexer head size).
   5 819 576 checks, 8/8 mutations. The narrow one is the factor-two test (19 checks):
   it needs a width divisible by the size but not by twice it.
41. KV state ranges — EXP321 opens P5-08 with the cell selection of
   `state_write`: non-empty, carries the sequence (all cells for -1), not excluded by
   the sliding window at the sequence's highest position, coalesced into ranges whose
   lengths must equal the per-stream counts; and the partial-only section drop for a
   cache without a window. 40 670 checks, 8/8 mutations. Still open in P5-08: the
   metadata payload and the tensor data of the same write/read pair.
42. KV state metadata — EXP322 transfers `state_write_meta`: per cell the position, the
   id count, the extension when present, and the ids in increasing order, with a named
   sequence writing only itself and an unnamed write carrying all of them. 109 429
   checks, 8/8 mutations. One candidate mutation was unobservable with a single bit
   word and was replaced. Still open in P5-08: the tensor data and the read side.
43. KV state data plan — EXP323 transfers `state_write_data`: the header (layout flag,
   layer count, both rotation sizes), the per-layer key type and row size with one
   transfer per range, the plain-V repetition of that shape, and the transposed-V walk
   where the embedding dimension is the outer loop and the offset is
   `(first + j*kv_size) * element_size`. 65 202 checks, 8/8 mutations. Still open in
   P5-08: both halves of the read.
44. KV metadata restore — EXP324 transfers `state_read_meta` in both branches with
   its six rejection codes: the single-sequence path (fit check, destination removal,
   exact one-id records, mirrored layout or search, extension restored after the batch)
   and the whole-cache path (cells from zero, count-only mirror check, id range check,
   head reset). 6 715 checks, 9/9 mutations. Still open in P5-08: `state_read_data`.
45. Multi-GPU `allreduce` — not ported. It is a 977-line NCCL/communication
   subsystem; this host has one GPU, so no arm can exercise it. It belongs to
   P3-18's hardware-unverified ledger rather than to blind porting.
46. Downstream phases P4–P11.

## Disk

The host was at 100 percent (3.1 GiB free) on 2026-11-11. About 85 GiB were
reclaimed by deleting old stage `run`/`build` copies and archived agent builds
after making EXP244 self-contained (87 external symlinks replaced with hard
links first). The retention set is EXP244; `runtime/active.json` records the
cleanup. Do not recreate stage copies.
