# Test coverage of every module in the shipped unit, and what is not covered

The unit is 24 modules. This is the map the last request asked for: module, what it decides, what proves it, and what
remains untested. It is generated from the repository by `tools/audit-door-evidence.py --modules`, so it cannot drift from
the sources without the command saying so.

```
python3 tools/audit-door-evidence.py --modules   -> ok, every module in the unit is traceable to evidence
```

## Modules with direct evidence

| module | decision | evidence | not covered |
|---|---|---|---|
| `sampling/sampler-chain-full` | top-p, typical, XTC, DRY, mirostat, penalties, temp_ext, adaptive_p | per-stage doors compared on selected tokens; the DRY order defect and the XTC sort defect found this way; AWT now drives them through an agent session | adaptive_p off-by-one ULP off the acceptance profile |
| `speculative/spec-doors` | synthetic and rejection acceptance, prefix agreement, draft width, compatibility | 880 rejection combinations, 180 compatibility, 630 ngram-accept, synthetic live on the model | nothing requires a draft model; the live rejection route does |
| `model/bpe-tokenize` | the encoder | 248 320 token texts equal; 23 prompts; a 32 000-token text prompt with the same answer hash | pre-tokenizer families other than qwen35 (guarded, never served) |
| `model/bpe-detokenize` | piece cache, buffer form, end-of-generation, sequence form | all 248 320 pieces equal, then served; short buffers equal; streaming edges and abandonment | none known |
| `model/gguf-reader` | the metadata reader | every key index and type, 34 values bit for bit, 13 strings, 744 231 array elements, refused on disagreement | float arrays (Apex has none), multi-file shards |
| `model/gguf-tensors` | tensor records | writer cases relinked against the shipped library, 41 cases / 307 checks | - |
| `model/arch-registry` | architecture names and recipes | an 89-recipe sweep with 0 disagreements at load | - |
| `model/slru-policy-goose` | the expert-cache replacement | 61 441 decisions equal on a real run | **not served**: needs the per-layer maps inside the door |
| `misc/pretokenizer-qwen35` | the segmenter | inside the encoder's equality | other families |
| `cli/fit-params-line` | the fit line | 20 cases equal to the native format, `-ts` and `-ot` included | no call site in this tree |
| `cli/llama-bench-report` | report formatting | 5 hand-checked cases (means, CSV escaping) | no call site; no end-to-end report comparison |
| `cli/config-validate` | config rules | 4000 cases, 0 uncovered | - |
| `server/hf-cache-naming` | HF cache naming | 6 hand-checked cases, `models--org--name` | no call site |
| `cuda/cuda-arch-policy` | compute capabilities | 5 hand-checked cases | wiring lives in CMake |
| `cuda/cuda-target-flags` | nvcc flags | `-lineinfo` in debug, absent otherwise | wiring lives in CMake |
| `cuda/cuda-install-layout` | install destination | 2/1/0 for dynamic/shared/neither | no call site |
| `misc/spec-decode-decisions` | ngram draft, accept, emit | 11 exports named; reached through the spec doors | live draft routes need a draft model |
| `metrics/metrics-traces` | CSV/trace field rules | 21 exports named; the `%.6g` provider exists because the module needed one | the sinks live in another tree |

## Modules exercised through their callers

`misc/unicode-text` and `misc/gguf-headers` have no card of their own. The audit shows why they are still accounted for: the
first is called by `pretokenizer-qwen35` and `bpe-detokenize`, the second by `gguf-reader`. Their correctness is the
correctness of those callers, which the table above proves on 248 320 pieces and 744 231 metadata elements.

`speculative/spec-decisions` and `model/tokenizer-decisions` export nothing: they are bodies called by the doors above.

## The gap this map makes visible

Every module has evidence, and that was **already true when the server was dying**. The crash came from
`common_token_to_piece` in the C++ host, not from a Goose module - and no per-module test could have found it, because the
defect was in how the host used the port's answer. That is why the agent-level test is now part of acceptance: it exercises
host and port together, which is the boundary the module tests do not cross.

## The rule this establishes

A module is not DONE because its exports are named in a card. It is DONE when a test drives the shipped call path that
reaches it and checks the result, and the untested column above is the honest list of what remains.
