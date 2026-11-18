# EXP-2026-11-17-942 — Maxwell reasoning in live chat

The Maxwell GGUF ships a Jinja template whose output declares message boundaries but does not declare a reasoning field. The generic PEG parser treated generated think tags as message content. An initial Goose adapter fixed requests without tools. The browser also sent two tool schemas with its automatic reasoning setting, and the initial adapter routed these requests back to the generic parser. The Maxwell template lacks tool-call syntax, so the corrected gate checks the capabilities of the template.

## Changes

- The Goose module matches the simple Maxwell template and splits response spans. It buffers partial opening and closing tags across stream chunks, retains reasoning when generation ends before the closing tag, and removes a terminal Maxwell turn marker.
- The patch connects those spans to the existing chat message lifecycle. It preserves the rendered prompt, the tool-capable PEG parsers, the explicit `none` format, and the sampler and tokenizer configuration.
- Apply [the patch](source/runtime.patch) to the Maxwell-enabled server sources at revision `1b44e30775dcb2f93952116b05f89e5bd797fb00`. The verified overlay replaces only the `chat.cpp` object and adds one object generated from [the Goose module](source/maxwell-reasoning.goose). Compile the Goose C and the `chat.cpp` object with the generated header in scope and `-Dgoose_init=fg_maxwell_reasoning_init`. Use `-fPIC` for both objects and `-fvisibility=hidden` for generated C. Relink `libllama-common.so.0` with the original build's remaining objects and libraries. The compiler SHA-256 was `664d4de42463cc10cb9c4a61dac59ce6023faa9c9da2838fddc0f4fb8294b3ab`.

## Verification

CPU tests compare the original and fixed parsers using the actual Maxwell and Apex Jinja templates. They cover full replies, streaming prefixes, split tags, truncation, UTF-8, end markers, and explicit plain-text mode. The tools fixture reproduces the regression with the first adapter and passes with EXP942. The rendered Maxwell prompt and the tested Apex output match the control.

Live tests used Maxwell Q4_0 on an RTX 4070 12 GB, context 32768, GPU expert cache 3800 MiB, Q4_0 KV, Flash Attention, mmap, batch and microbatch 2048. An ordinary API response split the reasoning and content fields and stopped normally. SSE coverage included a handler returning a capped, unclosed reasoning block. The final live browser request sent two tools and automatic reasoning, and returned reasoning separately, a clean answer and a normal stop. The user confirmed the Reasoning panel appeared during generation and collapsed at completion.

The patch changes the chat library only. GPU buffers and peak VRAM were not rebenchmarked in a paired test. The source build, Maxwell v3, Apex EXP906, and GGUF were left intact.
