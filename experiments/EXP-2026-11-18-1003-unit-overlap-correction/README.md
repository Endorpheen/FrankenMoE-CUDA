# EXP-2026-11-18-1003: The report said 0; the truth is 12

Status: a measurement in the report was wrong in the reassuring direction, and correcting
it changes the picture from "nothing of the unit runs in Apex" to "twelve decisions do".
No code generation changed; the number did.

## The bug

`migration-status.sh` counted how many of the composed unit's exported functions exist in
the shipped library, using an inline heredoc:

    names = set(re.findall(r'^\s*[A-Za-z_][\w *]*\b(\w+)\s*\(', f.read(), re.M))

In a non-raw `<<'PYS'` heredoc the `\b` reaches Python as a literal **backspace**
character, so the pattern required a 0x08 byte between the type and the name. It matched
nothing, `shared` was empty, and the report printed:

    unit exports in accepted lib  0 of the header's functions

Every cycle since EXP999 repeated that number. It was never true.

## The real number

    unit exports in accepted lib  12 of the header's functions
    (ar_arch ar_count ar_exps ar_find ar_match ar_suffix goose_init
     q35_kind q35_plan q35_rope_multi q35_shape qg_split)

Twelve, and they are not incidental: `q35_plan`, `q35_kind`, `q35_shape` and `qg_split`
are the **Apex architecture plan** - the 41-layer decision this whole migration is
measured against - plus the arch-registry lookups (`ar_*`).

EXP1002 showed `fg-q35-bridge.o` forwards to those exports. That was the clue that made
this check worth re-running; until then the zero looked like a finding.

## Why it stayed wrong for five cycles

The number was in the reassuring direction. "The shipped artifact contains none of the
unit" is a dramatic, quotable fact, and it fit a narrative the series had built: bridges
frozen from experiments, work invisible to the artifact. A number that confirms a
suspicion is checked less often than one that contradicts it.

That is the whole lesson, and it is worth more than the twelve symbols.

## What was done

`tools/unit-overlap.py` replaces the heredoc: a raw-string regex, a tested entry point,
and it prints the names, not just the count. The report now shows both - a count without
names could hide the same class of error again.

    $ python3 tools/unit-overlap.py <lib> <header> --list
    12|ar_arch,...,qg_split

## What this changes

The picture across the last five cycles, corrected:

| question | answer | status |
| --- | --- | --- |
| unit exports in the accepted artifact | **12** | was reported as 0 |
| carrier-unit exports in a fresh build | 72 / 72 | unchanged |
| bridge exports called by nobody | 169 of 342 | unchanged |
| wholesale-dead objects | 1 (`fg-q35-bridge.o`) | unchanged, and it wraps the unit |

So the accepted artifact is not a bridge-only runtime: the Apex plan and the architecture
registry already run as Goose code there. The gap between 12 and 72 is what a rebuild
would add - a much smaller and more precise statement than "nothing runs".

EXP999's headline ("the shipped library has neither") was wrong. Its supporting
measurement has been replaced, and the correction is recorded rather than quietly applied.

## Not claimed

Twelve exports present is not twelve decisions exercised. `q35_plan` is present in the
accepted library and EXP1002 measured that its bridge is uncalled, so the decision may be
linked and reachable without being used on the Apex path. Presence and execution remain
separate facts - the distinction this series keeps insisting on, now applied to its own
correction.

No gate re-run: nothing under `goose/` changed.

No model was started, no benchmark was run.
