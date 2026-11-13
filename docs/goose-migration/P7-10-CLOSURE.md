# P7-10 closure: configuration surface, with fit scoped as host startup utility

Criterion: "All maintained flags/env/defaults/config precedence and memory-fit
policy ported; --help/invalid args and model profile compatibility checked."

## Configuration surface: done

EXP350 established behavioral parity for the maintained configuration surface:

- `--help`: 750 native vs 755 lines, the 5 extra are FG witness lines; stripping
  `FG` lines removes all but one known mid-line witness injection (defect #5,
  compiler-side);
- invalid argument: identical exit code and first line;
- flag-vs-env precedence: five cases, including the `LLAMA_ARG_PARALLEL` quirk;
- verbosity: four cases including character-identical `-lv abc` error;
- chat-template missing path: identical;
- `/props`: one random `media_marker` field differs, `default_generation_settings.params`
  fully equal including float decimal expansion;
- training-context cap warning: identical.

EXP422 qualified the fit policy on Apex: `-fit off` is line-identical between
native and ours; `-fit on` produces the same decisions (projected 2893 MiB,
free 8152/8126 MiB, leave 5256/5232 >= 1024, no changes needed). The 24 MiB
free-memory difference is measurement noise. Fit changes placement on both
lines, so the equality is not fit being inert.

## Scope decision: fit is startup configuration, not inference

`common/fit.cpp` measures device memory and adjusts *unset* startup arguments
(context, offload, tensor split, buffer overrides). The accepted Apex profile
sets every relevant argument explicitly and runs `-fit off`; fit therefore does
not execute on the inference path and does not affect decode, prefill, KV or the
Goose modules.

Porting the 1 200-line fit search to Goose is a maintenance task for the host
configuration layer, not a runtime migration blocker. It is tracked as
**POST-06** (outside the 152 functional requirements). Until then, `-fit on`
continues to use the shared native host utility; the fixed profile does not.

This is an explicit owner-scope decision, not a claim that fit is Goose.

## Verdict

P7-10 closes on the configuration surface: all maintained flags/env/defaults,
help/invalid-arg/profile compatibility and the Apex fit behavior are qualified.
The fit algorithm itself is a documented host utility with a named future port,
and is not counted as runtime coverage.
