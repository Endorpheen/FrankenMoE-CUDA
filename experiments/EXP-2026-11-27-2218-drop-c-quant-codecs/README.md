# EXP-2218: Exclude the GGML quantization reference from GooseInfer

The universal runtime uses Goose quantization paths but still linked the
GGML reference codecs through pointers in the C type table. Build only
the metadata part of the table in the universal runtime. Keep the full
reference available to separate builds and differential tests. Linking
a universal component that requires the removed quantization API now fails.

Linux and Windows linked without ggml-quants.o. The native Linux probe
printed identical names, block sizes, type sizes, quantization flags and
row sizes for all 43 type entries. Build the probe twice with GCC 13 and
the vendor include directory. Link the first executable with the reference
ggml.o, ggml-quants.o and ggml-system.o. Link the second with ggml.o and
ggml-system.o from build-gooseinfer-common-core.sh. Use
`-Wl,--gc-sections -lm -pthread` and compare stdout with cmp.

The universal build excludes 5,639 C lines in ggml-quants.c. The remaining
two compiled GGML files contain 8,244 physical lines, down from 13,871.
This count includes the compatibility sections of ggml.c that the new
build definition excludes. No full-model throughput or memory result
was collected for this change.
