# OpenCode and FrankenMoE chat smoke test

Run this checklist for each new model or HTTP compatibility change. Record the
server build and SHA-256, model and quantization, chat-template source, OpenCode
version and provider configuration, OS, GPU, and relevant launch flags. Run one
model server at a time. Use a separate OpenCode project and isolated settings.

## Prepare

1. Read the model card and the GGUF chat template. Identify its system prompt,
   reasoning markers, tool definition format, tool call syntax, tool results,
   stop tokens, and context and output limits. Select the matching server
   adapter. Do not infer tool support from the architecture name alone.
2. Create a test directory containing `marker.txt`. Put a fresh, distinctive
   string in the file and keep the expected string out of the prompt. Reserve
   `output.txt` for write and edit tests.
3. Run FrankenMoE on loopback. Check `GET /health`, `GET /v1/models`, and load
   the bundled web page in a fresh browser context. Confirm the selected
   model and capture statuses for relevant failed requests.
4. Connect OpenCode using its OpenAI-compatible provider and a base URL ending
   in `/v1`. Enable model reasoning and tools. Limit the test session to
   read access to the fixture, edits to `output.txt`, and the exact command
   `pwd`. OpenCode can match file permissions relative to the Git root.

## Web chat and OpenCode

1. Send a short question with a known answer in the bundled chat. For example,
   ask for 18 + 24 in Russian. Expect 42 and a successful chat request.
2. Ask a question that elicits reasoning. Confirm the response delivers
   reasoning separately from the answer. Open and close the reasoning block
   in the bundled chat. In OpenCode check the reasoning parts in the session,
   enable reasoning in display settings, and verify the visible controls in
   the installed build separately. Record whether that build shows summaries,
   collapsible details, or only the final answer.
3. Ask OpenCode to use `read` on `marker.txt` and report the first line after
   the tool result. Count a pass only if OpenCode records a completed tool
   part and the final answer contains the exact marker. Raw XML in the answer
   and a guessed marker are failures.
4. Ask OpenCode to run these tools in order, waiting for each result:
   `write` creates `output.txt` containing `ALPHA-BEFORE`, `edit` replaces
   that value with `ALPHA-AFTER`, and `bash` runs exactly `pwd`. Confirm all
   three completed tool parts and their visible cards. Verify the file
   contents and working directory independently.
5. Send the Markdown request with tools disabled: a level-two heading, bold
   phrase, two-item list, inline code, a JSON block containing
   `{"ok": true, "value": 42}`, and a Python block containing `print(42)`.
   Inspect heading, list, emphasis, and the two fenced blocks separately.
   Confirm language labels and copy behavior if the client offers it.
6. For each additional supported tool, add one isolated task with an
   externally verifiable result. Exercise numeric, boolean, array, and
   object arguments if the tool schema uses them. Keep permissions narrow.
   Record protocol failures and client permission failures separately.

## Check the protocol and timings

- Inspect one non-streaming and one streaming reply for text, reasoning,
  and a real tool call. Verify `message` versus `delta`, stable call IDs,
  valid JSON arguments, tool finish reason, terminal SSE event, and a
  follow-up containing the tool response.
- Confirm that SSE delivers distinct events while generation is active.
  For a long answer record time to first useful event and time to completion.
  A response buffered until generation ends can distort the client rate even
  when the server's prefill and decode counters are correct.
- Check a request with no explicit output limit and a request with the
  provider's configured maximum. Verify that the server accepts both within
  the available context. Reproduce a small intentional cap and verify
  `finish_reason: length`. The web chat can label a reasoning-only result
  as `Cancelled` even when the server reached the token limit normally.
- Compare server prefill/decode token counts and durations with response
  `usage` and `timings`, then compare the client display. Prefill and decode
  use different token counts and intervals. Note whether the client reports
  decode, provider-active, or total-turn throughput.
- For interoperability claims, repeat the same cases with the same model
  and matching context and sampling settings on a known llama.cpp reference.
  For a Windows release repeat the live checklist on Windows with NVIDIA.
  A cross-build or deterministic HTTP fixture is separate evidence.
- Save a pass/fail table with server and client versions, statuses, the names
  and outcomes of tool calls, and timing fields. Keep credentials, real user
  prompts, model reasoning, and private file contents out of shared artifacts.
