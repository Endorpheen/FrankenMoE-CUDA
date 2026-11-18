# EXP-2026-11-17-938: Goose can open a listening socket

Status: probe PASSED. This closes the transport question that has stood since the
MODEL ownership blocker was recorded in `docs/goose-migration/CODEX-HANDOFF.md`:
the standalone runtime has server decisions but no listener.

## Hypothesis

The blocker recorded in the handoff is that Goose cannot call libc: the generated
translation unit includes system headers that already declare `open`, `close`
with different prototypes, so an `extern` with the Goose signature fails to
compile. If the blocker is about **specific names** rather than about calling
into libc at all, then socket calls — which no included header declares — should
bind directly, and only `read`/`write`/`close` need a rename.

## Probe

`socket`, `bind`, `listen` are declared in Goose with their own C names and
compile with no conflict. `close`, `read`, `write` conflict exactly as recorded,
so they are bound through three one-line libc forwards with distinct names
(`gx_close`, `gx_read`, `gx_write`); the forwards contain no decision, only the
prototype.

Result on 127.0.0.1, kernel-chosen port:

    LISTEN_rc=0

`socket -> bind -> listen -> close` executes from Goose.

## What this changes

The "no HTTP transport" remainder of P11-01 is not a compiler wall. The
listener can be written in Goose now, with the three tiny forwards as recorded
adapter debt (translation only, no policy). The `open`/`mmap` ownership blocker
is narrower than stated: it is about names `unistd.h`/`fcntl.h` already declare,
not about reaching the kernel.

## Reuse

    goose --standalone -o r.c --header r.h source/listen.goose
    gcc-13 -O1 -o listen tests/main.c r.c source/libc-wrappers.c -lm
    ./listen          # prints LISTEN_rc=0

## The listener itself

`goose/server/http-listener.goose` now carries the transport decisions:

- `ht_open` — socket, SO_REUSEADDR, bind, listen, with a distinct return code
  per failing step so a gate can name the step;
- `ht_accept` — one connection, with the peer address buffers owned by the host
  (a Goose slice is passed by value, so a local array cannot stand in for the
  C pointer);
- `ht_read_request` — read until CRLFCRLF, bounded by the header limit, then the
  method code, the path span (query excluded) and the header end;
- `ht_keep_alive`, `ht_contains_ci` — case-insensitive header search and the
  keep-alive rule;
- `ht_response_head`, `ht_write_all` — status line, Content-Length, content
  type, connection header, and a short-write loop.

Real request through it:

    HTTP/1.1 200 OK
    Content-Type: application/json
    Content-Length: 11
    Connection: keep-alive

    {"ok":true}

Server-side log of the same request: `ACCEPT conn=4`, `REQ n=60 method=1
hend=60 path_off=4 path_len=6`.

Gate: `bash tests/run.sh [outdir]` prints `HTTP_LISTENER OK`.

### Two defects the probe exposed

- reading before `accept` gives `ENOTCONN` (107) on the listening socket; the
  first version did exactly that and looked like a broken read;
- `accept` cannot take a Goose slice for `sockaddr`/`socklen_t`: slices are
  passed by value, so the host supplies those buffers and the forward passes
  their pointers.

## Router behind the listener

`integration/goose-standalone/tests/http_server.c` closes the loop: accept, read,
take the path span, ask `route_id`, answer with the status the route implies and
a body per route category, honouring keep-alive.

    /health  -> HTTP/1.1 200 OK      route=1
    /props   -> HTTP/1.1 200 OK      route=3
    /metrics -> HTTP/1.1 200 OK      route=2
    /nope    -> HTTP/1.1 404 Not Found  route=0

Gate: `bash tests/server-route.sh [outdir]` prints `HTTP_ROUTES OK`.

`route_id` signals "no route" with **0**, not -1; the first version treated 0 as
a hit and answered 200 to an unknown path. The host now maps `route > 0` to 200.

## One unit, HTTP in, real ids out

`tests/http-tokenize.sh` composes listener + router + GGUF reader + vocabulary
layout + tokenizer into **one** Goose unit, builds it with a host that owns only
buffers and the model path, and drives it over TCP:

    POST /tokenize  "The capital of France is" -> {"ids":[760,6511,314,9338,369],"count":5}
    POST /tokenize  "Hello, world!"            -> {"ids":[9419,11,1814,0],"count":4}
    GET  /health                               -> {"route":1}

Both id lists were checked against libllama on the same strings and match,
including the trailing 0 for "Hello, world!" (it is a real token, not a bug).
Loader log from the same binary: `LOAD parse ok keys=52 tensors=753`,
`vocabulary tokens=248320 special=33`.

Gate: `bash tests/http-tokenize.sh [outdir]` prints `HTTP_TOKENIZE OK`.

### Three defects the integration exposed

- composing two readers (`goose/model/gguf-reader.goose` and the EXP937 reader)
  emitted `gr_parse`/`gr_find_kv` twice and the second copy silently won, so the
  host called a parser whose state layout differed. `gguf-reader.goose` is now a
  thin import target naming the EXP937 implementation as the single body;
- key lengths were typed by hand (`"tokenizer.ggml.token_type"` is 25 bytes, not
  26) and a wrong length answers -1 with no other symptom. The host now computes
  every key length;
- `gr_parse` succeeding says nothing about lookups working: the first run printed
  `parse ok keys=52` and then failed at the first key lookup.

## Round trip over HTTP

`POST /detokenize` takes `{"ids":[...]}` and returns the text the Goose piece
path produces:

    POST /tokenize    "The capital of France is" -> {"ids":[760,6511,314,9338,369],"count":5}
    POST /detokenize  {"ids":[760,6511,314,9338,369]} -> {"text":"The capital of France is","count":24}
    POST /tokenize    "Hello, world!" -> {"ids":[9419,11,1814,0],"count":4}
    POST /detokenize  {"ids":[9419,11,1814,0]} -> {"text":"Hello, world!","count":13}

Text -> ids -> text is therefore answered end to end inside one Goose unit, over
TCP, with no libllama and no GPU. The gate checks all four calls.

## /props reports the model, read by Goose

The endpoint no longer returns a canned body. It reads the numbers out of the
GGUF header with `gr_meta_int` and reports the vocabulary the tokenizer engine
built:

    {"n_vocab":248320,"n_special":33,"n_layer":41,"n_embd":2048,"eos":248046}

`n_layer` 41 and `n_embd` 2048 match the Apex file; `eos` 248046 is the
`<|im_end|>` the chat format stops on. The gate now checks these two numbers, so
a reader regression that leaves the transport working still fails the gate.

## Generation: the full runtime answers /completion

The standalone binary now loads the Apex weights, binds 733 tensors by offset,
builds the whole 40-block trunk through Goose and generates over HTTP.

    POST /completion "The capital of France is"
    -> ids 11751 11 264 3177 8807 303 3712 321 ... (256 tokens)
    -> text " Paris, a city rich in history and culture. Established in the 9th
       century, Paris grew from a small settlement on the Seine River ..."

The first eight ids are identical to the recorded EXP922 run, so the trunk path
in the server is the same one the gate measured. `/props` in the same process
reports `n_layer=40`, the vocabulary the engine built and the binding count.

Gate: `bash tests/http-completion.sh [outdir]` prints `HTTP_COMPLETION OK`.

### What is Goose here and what is not

Goose: the trunk plan and every layer decision, attention and GDN branches, MoE
routing, KV bookkeeping, the tokenizer, the sampler and the piece path.

Native, and stated as such: the GGUF container walk (`gguf_init_from_file`,
tensor offsets) and the ggml CPU compute call. Both are the substrate the
audit already lists under P10-12 G8; no layer, branch or token decision is made
in the host.

### Two host defects the run exposed

- `sm_greedy` takes the candidate COUNT as its second argument, not a flag; the
  first version passed 0 and every step "failed" with the logits already
  computed;
- the logits tensor is `[n_vocab, n_tokens]`, so the next token comes from the
  last position's row. Reading row 0 instead of row `past-1` would have sampled
  from the wrong position while still producing plausible text.

### Requests must not leak into each other

The first server reset only the KV bookkeeping. A second request then produced
`11751 369 524 9444 369 524 9444 ...` — plausible-looking text built on the
previous request's recurrent state. Resetting the KV counters is not enough:
every GDN conv/state buffer and every KV cell buffer is per-request state and
must be zeroed too.

With that fixed, two identical requests return byte-identical id lists, and the
gate asserts it.

## End of generation is the Goose set, not the token cap

`bt_eog_ids` builds the stop set from the token texts (5 entries on Apex) and
`bt_is_eog` answers the stop question; the first version passed an EMPTY table,
so generation could only ever end at the cap.

A chat turn now stops where it should:

    <|im_start|>user
    What is 2+2?<|im_end|>
    <|im_start|>assistant

    -> 50 tokens, last id 248046 (<|im_end|>)
    -> " ... The answer is 4.\n\n**4**\n\nIs there anything else I can help you with?<|im_end|>"

`/props` reports the set size (`n_eog`), and the gate asserts that the chat
answer ends on 248046 with the stop token in the text.

## First speed numbers, and the limit they exposed

Single request, CPU only, `GOOSE_THREADS` sweep, prompt "The capital of France
is" (5 tokens), 256 generated:

| threads | prefill tok/s | decode tok/s |
|---:|---:|---:|
| 4 | 16.4 | 9.27 |
| 8 | 18.5 | 9.50 |
| 16 | 18.5 | 8.73 |

Peak RSS during a run: **10966 MiB**. Memory: the weights are mapped read-only
and the graph arena is per step.

Against the accepted reference (2625.21 prefill / 55.52 decode) the standalone is
about an order of magnitude slower on decode, and that comparison is not
like-for-like: the reference runs with GPU offload and batched prefill, this is
CPU only, one block at a time, with a freshly built graph per token.

Prefill at 5 tokens is startup-dominated (0.30 s total), so the number above says
almost nothing about prefill throughput; it needs a long prompt to be meaningful.

### The real limit: a fixed graph arena does not scale with the prompt

The graph context was a fixed 1 GiB, enough for a five-token smoke prompt and
fatal for a normal one:

    ggml_new_object: not enough space in the context's memory pool
    (needed 1097320864, available 1073741824)

Sizing it as `base + 6 MiB per token` moved the failure rather than fixing it:
the arena grew and the request grew with it (2.72 GB → 3.79 GB → 4.63 GB, each
time just above what was allocated). That behaviour is the argument for the real
fix: allocate the graph through `ggml_gallocr`/the backend allocator, which reuses
buffers by node lifetime instead of giving every node its own slice. Until that
lands, prompts beyond roughly a thousand tokens cannot be served, and that is
recorded rather than papered over.

## Allocator with buffer reuse: the arena limit is gone

The graph now builds with `no_alloc` and is placed by `ggml_gallocr` on the CPU
backend, which reuses buffers by node lifetime. Inputs are filled AFTER
allocation, because with `no_alloc` they have no data until the allocator places
them. `ggml_backend_cpu_set_n_threads` replaces the per-call thread argument.

A 517-token prompt, which the fixed arena could not serve in any size:

    TIME prompt=517 prefill_s=5.78 prefill_tok_s=89.5 decode_n=122 decode_s=10.35 decode_tok_s=11.79

Short prompt, unchanged ids and repeatable:
`11751 11 264 3177 8807 303 3712 321 ...`, decode 11.70 and 11.59 tok/s.

| configuration | prefill tok/s | decode tok/s | peak RSS |
|---|---:|---:|---:|
| fixed arena, 5-token prompt | 18.5 | 9.50 | 10966 MiB |
| gallocr, 5-token prompt | 30.3 | 11.70 | — |
| gallocr, 517-token prompt | 89.5 | 11.79 | 16528 MiB |

Decode +23% against the fixed-arena build on the same prompt and thread count,
and the long prompt works at all. RSS rises with the prompt because the allocator
reserves for the largest graph; that is the next memory item.

Gate: `bash tests/http-long-prompt.sh [outdir]` prints `HTTP_LONG_PROMPT OK` and
fails if the allocator reports "not enough space".

## Where the memory goes, and where the speed gap really is

Measured, not guessed:

| quantity | value |
|---|---:|
| graph context metadata | 2 MiB |
| gallocr buffer, 517-token prefill | 502 MiB |
| allocated logits copy per step | 513 MB at 517 tokens |
| RSS during a 517-token request | 16150 MiB |
| VmSize of the process | 32136 MiB |
| mappings of the model file | 1 |

So the arena question is settled: the graph costs about half a gigabyte at that
prompt length, and the rest of RSS is resident pages of the read-only weight
mapping (21.7 GB mapped). RSS therefore grows with how many expert tensors the
prompt touches, which is paging behaviour, not a leak.

Decode at 11.9 tok/s is not a mystery either. With the Apex MoE active weights at
roughly 3.5 GB of Q4 bytes per token, 11.9 tok/s is about 42 GB/s of RAM traffic
- a plausible DDR figure. The standalone is memory-bandwidth-bound on the expert
reads and has no expert cache.

That also explains the reference: its 55.52 decode is measured with
`--moe-cache-mib 4992`, i.e. the hot experts served from VRAM. The standalone has
no such cache, so the comparison is not "the same work, slower" - it is "less
work avoided". Closing it means bringing the expert cache into the standalone,
whose admission, copy and SLRU decisions are already Goose modules
(`fg262a`, `fg263c`, `fg756s` in the accepted library). That is the next
substantive step, not a micro-optimisation of this graph.

## One reader, not two

The server unit was composing two readers (`goose/model/gguf-reader.goose` and
the EXP937 copy), which emitted `gr_parse`/`gr_find_kv` twice. The second body
won silently and the host called a parser whose state layout differed.

The first attempt at a fix - reducing the canonical module to a comment - broke
the **shipping** standalone build (`tools/build-main-runtime.sh` composes the
canonical reader directly and needs its own bodies), which the build caught
immediately: `unknown function: gr_lookup`.

The right fix keeps one body: the newer functions (`gr_val_span`,
`gr_arr_str_table`) moved into `goose/model/gguf-reader.goose`, the EXP937 copy
stays as the experiment record only, and every gate and the server now compose
the canonical module alone.

    main runtime ok: 1117 global symbols, 0 duplicates, 136 extern
    SERVER vocab=248320 special=33 eog=5 eos=248046 layers=40 bindings=733
    HTTP_COMPLETION OK

Same ids, same text, decode 11.86 tok/s.

### The lesson recorded

A local fix that makes one test pass can break the shipping build. The shipping
build is `tools/build-main-runtime.sh`; it is cheap (seconds) and it is the thing
that decides whether the runtime exists. It runs before a claim, not after.
