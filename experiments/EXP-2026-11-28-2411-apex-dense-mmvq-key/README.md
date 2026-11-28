# EXP-2026-11-28-2411 — Initialize the dense MMVQ graph key

The dense MMVQ graph hash and comparison read 16 words. The Goose port
populated the first nine and left the tail uninitialized. Initialize the
whole array before copying the prefix, as in the frozen EXP-1880 control.
The graph key layout and capacity stay the same.

Both Linux binaries use the EXP-2410 `-O2` host build, the same Fried Goose
compiler and CUDA images, and Apex Q4_K_M on the RTX 4070. Context 262144,
batch/ubatch 4096/4096, Q4_0 KV, and 6290 MiB expert cache. Both used one full
warm-up followed by a 32768/512 request with zero prompt reuse. All 512 IDs,
response text, and stop type match EXP-1880. Scored-window system swap-out
was zero.

| Warm CUDA trace | EXP-2410 | EXP-2411 |
| --- | ---: | ---: |
| Decode graph captures and instantiations | 494 | 2 |
| Dense MMVQ pairs launched outside CUDA graphs | 457 | 0 |
| Total graph instantiation API time, ms | 21.94 | 0.07 |
| Client request, s | 17.088 | 17.132 |

The warmed unprofiled candidate scored 3348.27 prefill and 77.69 decode
tok/s at 16.461 s client time. Peak RSS was 21,501,112 KiB. Sampled process
VRAM was 8774 MiB, versus 8864 MiB in both EXP-2410 candidate arms. Process
swap and system swap-out were zero during scoring. This result qualifies
the deterministic cache key and observes a lower sampled VRAM peak. It
does not establish a throughput increase over EXP-2410. The Apex speed
gates remain open.

| Artifact | SHA-256 |
| --- | --- |
| EXP-2410 executable | `73f9b2e357b2a4baaf0df654c037a441afb26643d1f08b81eb27abf0eea2ae58` |
| EXP-2411 executable | `da5652712161a8ecf9c3245229bdb2ca57f496231638b4f8b66fb42b803f9e04` |
| Frozen request | `d81f501a24aa29e1933662fbb8b05d3a1eec5bf7709c622fe7ec63371d8d2d9f` |
| EXP-2410 warm trace response | `03b6601915a4f05956ee855dcd5403cb63748f9647f9fc7624266f9c7646253b` |
| EXP-2411 warm trace response | `e6c8af2c6f8a5d8827883409c620157be4a1933362c68be94696168280511470` |
| EXP-2411 unprofiled warm response | `be8bc8e1bd4c03121c16897ee176786c3a5f5adb7df138d93a60f396b2908054` |
