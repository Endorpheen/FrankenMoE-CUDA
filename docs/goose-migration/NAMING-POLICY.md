# GooseInfer naming policy

GooseInfer is the inference engine and repository. Goose is the
implementation language. Fried Goose is a compiler for that language.

| Component | Current source-build name | Compatibility name |
| --- | --- | --- |
| Package launcher | `gooseinfer` | `frankenmoe` in published older archives |
| Inference CLI | `gooseinfer-infer` | `frankenmoe-infer` |
| Standalone HTTP and tokenizer tools | `gooseinfer-serve`, `gooseinfer-tokenize` | `frankenmoe-serve`, `frankenmoe-tokenize` |
| Inference and tensor archives | `libgooseinfer.a`, `libgooseinfer-tensor.a` | `libfranken-inference.a`, `libfranken-tensor.a` |
| Linux package environment | `GOOSEINFER_*` | `FRANKENMOE_*` for supported launcher options |

Build outputs and compatibility aliases are defined in the
[build guide](../BUILD.md). The [release guide](../RELEASES.md) identifies
published packages. Keep their archive names, checksums and executable
names as recorded so existing downloads and launch instructions work.

Keep upstream ABI names where they describe an actual upstream component.
Before renaming a compiled component, check that the shipped call path
reaches its implementation and verify the consumer/provider signatures.
If compatibility names remain, cover both names with the same checks.
Record the build inputs, the executable or archive identity and the
verified call path in the corresponding experiment.
