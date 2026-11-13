# EXP156: Goose tensor constructors,views and context arena

Continue solo from verified EXP155. Move arena placement/capacity/publication,tensor validation/default metadata/layout,view alias flattening,reshape/permute/transpose and object traversal to Goose. Native C imports are typed POD projections,raw zeroing,previous Goose geometry and formatting. Preserve original valid-input layout/order/naming/alias semantics;detect malformed arithmetic before arena publication. Existing fast numerical kernels remain unchanged.

Require independent native context/tensor/view differential tests,failed-allocation transaction tests,negative constructor safety cases,explicit rollback and one fixed Apex32K/128 correctness/performance/memory/liveness gate. No requirement marked fully complete from this submodule alone.
