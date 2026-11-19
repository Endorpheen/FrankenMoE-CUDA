# EXP-2026-11-17-922: profile and CPU-compatible CUDA probe

Status: **REJECTED AS A MODEL OPTIMISATION**. EXP921 stays the working reference.
Current accepted medians remain6.518697 decode and6.247443 prefill on75/256.

The newly cached path profile has43.843473s decode: CPU FFN24.016244911s,
expert positioned reads5.892474669s, graph construction0.484604949s, PLE0.015483121s.
Dense copies and DU reads are zero during decode. A total59925 dense requests
hit retained bytes. Profile token IDs match all256 control IDs. Diagnostic
profiling had process swap0 but global swap-out115 pages; it is not formal speed.
OMP observation shows outer8 threads only for the first layer; other layers
have outer1, so general nested serialisation was not established.

Ordinary GGML CUDA changes activation packing (CPU Q3_K uses Q8_K, CUDA Q8_1;
CPU Q4_0/IQ4_NL use Q8_0). Its first latent final output fails the frozen
2e-5abs+2e-5rel gate: max_abs0.00033486262,1974 outside tolerance.

A Goose integer-dot probe with exact CPU activation packing passes90 real
matrix cases across D/U/gate/up/down, layers0/47, groups0/3/7 and amplitudes
0.125/1/8. The full top10/shared callback also passes the existing numerical
gate, max_abs4.42843884e-7, repeated callback bits equal. This does NOT establish
full inference parity: the isolated model candidate diverges at token61 after
60 identical IDs. It was stopped as rejected (241 outputs observed before owned
process termination; no completed256 speed result). Keep all partial logs.
Normal EOS was not suppressed; the planned second short model test was cancelled
because the mandatory model gate already failed. No acceptance tolerance changes.

GPU residency was1415577600 DU bytes plus512*1164800 expert bytes, a tagged
bounded direct-mapped cache, with one stream and CPU Q8 activation packing.
Own VRAM rose5296 to7218MiB; own RSS max11133668KiB and process swap0. Global
swap-out increased58038 pages; this independently disqualifies resource acceptance.
Observed steps were around0.2s, without a speed gain; do not promote partial
throughput or compare changed output as a performance A/B. Floating reduction
order is a remaining hypothesis for model divergence, not an established cause.
A future GPU attempt needs bitwise primitive/state agreement or another justified
correctness contract, plus reduced launch/copy overhead, before a model benchmark.

The rejected integration is retained as candidate.patch and immutable local
build inputs, while production pack.cpp/variant.py remain the EXP921 bytes.
Probe sources under integration/edge0/probes are diagnostic, not linked by the
normal build. Arithmetic uses pinned self-hosted Goose and the accepted EXP251
Q3 scale decoder; native CUDA resource/ABI code remains experimental debt.
No Apex/compiler/model file or foreign process changed. No full backend rebuild.
Baseline C++ replaced/removed0; dead C++ removed0; retained closure248213 lines/
350 files, necessary minimum UNKNOWN. Production native glue remains340 lines.

Next EXP923: preserve exact CPU arithmetic and share duplicate D projections
within a layer's top10 when selected experts have the same group. Record the
hypothesis and bounds first, test the actual callback and exact256 model IDs,
then compare serially against the immutable EXP921 control. No GPU candidate is
accepted by this experiment and the15-17tok/s target remains open.
