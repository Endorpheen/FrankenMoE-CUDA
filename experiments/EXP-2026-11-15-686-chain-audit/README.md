# EXP-2026-11-15-686 — chain audit: what is Goose, what is substrate, what is absent

Base: `aeb5542d`. Audit only; no build, no model, no GPU.

Requirement being answered: verify the real Apex chain — load → graph → CUDA → KV
→ prefill → decode — with no hidden C++ call, fallback or substitution, and delete
nothing without proof.

## 1. What the shipped standalone actually links

```
ldd builds/goose-standalone/frankenmoe-v4
  libstdc++ libm libgomp libgcc_s libc        (no libllama, no libggml as shared)
nm: llama_* symbols      0
nm: bmoe::* symbols      0        <- the old own C++ runtime is not in there
nm: cuda/CUDA symbols    0
```

So the standalone contains **no old runtime code and no GPU code**. The
`libstdc++` entry is my own adapters, which are debt and listed as such.

The 10 `kv_`, 4 `prefill`, 6 `decode` matches are decision functions, not engines:
`kvp_prefill_span`, `kv_rule`, `qry_decode_path`, `ce_decode`, `tc_add_prefill`.
None of them allocates a KV buffer, runs a prefill or loops a decode.

## 2. The chain, stage by stage

| stage | Goose | substrate / other | verdict |
|---|---|---|---|
| GGUF container parse | — | ggml-base reader | substrate, stays native |
| GGUF metadata, every key | `s1_*` (7 exports) | — | **works** (4 real files) |
| tensor directory + extents | `s3_*` (7 exports) | — | **works** (24/24 shapes on the real model) |
| payload placement | `layout`/`nbytes`/`align_*` | reader materialises | **works** (0 mismatches, 940 tensors) |
| layer plan | `q35_plan`/`q35_kind` | — | **works** (42 = 10 attn + 31 recr + 1 MTP) |
| node construction | `gb_common`, 16 primitives, 15 predicates | — | **works** for the nodes tried |
| attention block compute | modes 4/11/3/11/0 | ggml CPU backend | **works** (5 nodes, 8.07e-08) |
| MoE expert matmul | mode 12 | ggml CPU backend | **works** (256 experts, 8 used, exact) |
| rope | mode 25 exists, **never exercised** | — | **not proven** |
| attention (flash_attn_ext) | mode 26 exists, **never exercised** | — | **not proven** |
| GDN stack (31 recurrent layers) | `op_ssm_conv`, `op_gated_delta_net` exist, **never exercised** | — | **not proven** |
| full 42-layer graph | **not built** | — | **absent** |
| KV cache buffers | policy only (`kvp_*`) | — | **absent** |
| prefill execution | — | — | **absent** |
| decode loop | — | — | **absent** |
| CUDA | — | — | **absent** |
| tokenizer mechanics | decisions only | — | **absent** |
| HTTP server | decisions only | — | **absent** |

## 3. The honest headline

**The chain is not complete, and nothing here is old C++ passed off as Goose.**
What exists is a proven core: the file layer, the architecture plan, the tensor
extents, and four node kinds that compute correctly on the CPU backend. What does
not exist is the engine: no rope, no attention, no GDN, no full graph, no KV
buffers, no prefill, no decode, no CUDA.

There is no risk of a substitution because there is nothing to substitute: the
standalone binary contains zero llama and zero `bmoe::` symbols. The risk the other
way is real and must be stated: a reader could see "Apex graph executes" and
assume inference works. It does not. Four nodes is not a model.

## 4. Temporary shims, under count

Every one is a removal target; none survives final acceptance:

| file | lines | role |
|---|---:|---|
| `integration/m1/host/goose_host.c` | 609 | host primitives (fp16, bit views, stores/flags) |
| `integration/goose-standalone/host_v4.c` | 334 | smoke host: reads a file, prints a report |
| `integration/goose-standalone/graph-bindings.cpp` | 190 | ggml translation for the 16+3 primitives |
| `integration/goose-standalone/tensor-data.cpp` | 129 | arena bookkeeping + agreement check |

Tests (`integration/goose-standalone/tests/`) are excluded from the debt count:
they hold no runtime logic and nothing links them into a shippable host.

## 5. What "ironclad proof" would require, in order

1. rope and attention exercised at real dims, not only declared.
2. The GDN stack exercised — 31 of 42 layers take that path, so it is the majority
   of the model, not an edge case.
3. A full 42-layer graph assembled from the plan and executed for one token.
4. KV buffers allocated and advanced across two steps.
5. A prefill over a prompt and a decode across tokens.
6. CUDA backend binding, for the Apex GPU path.
7. A real model run on Apex comparing output against the reference binary — which
   needs Igor's explicit permission.

Until 1–6 are done, nothing gets deleted. That is rule 15, and this audit is the
evidence that it is being followed rather than asserted.

## 6. Evidence reuse

Every "works" row cites an experiment already accepted (EXP671–685). No build was
run for this card; the symbol and library facts come from the existing binary.
