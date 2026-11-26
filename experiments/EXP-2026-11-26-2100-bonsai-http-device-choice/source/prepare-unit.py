"""Add optional resident device choice to an isolated Goose generation unit."""

from pathlib import Path
import sys


def replace(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"Expected one Goose generation anchor: {old!r}")
    return text.replace(old, new)


base, output = map(Path, sys.argv[1:])
unit = base.read_text()
unit = replace(unit, 'extern "fg_q35_generation_profile"', '''extern "fg_bonsai_choice_available" fn q35_choice_available(eog: const i64[:]) -> bool;
extern "fg_bonsai_choose_next" fn q35_choose_next(token: i32, logits: f32[:], ignore_eos: bool) -> i64;
extern "fg_q35_generation_profile"''')
unit = replace(unit, '    let start = q35_generation_now();\n    q35_generation_reset();',
               '''    let device_choice = !filter && !repeats && top == 0 && q35_choice_available(eog);
    var device_token: i64 = -2;
    let start = q35_generation_now();
    q35_generation_reset();''')
unit = replace(unit, '''        if s.ignore_eos {
            for i in eog.len {''', '''        if device_token < 0 && s.ignore_eos {
            for i in eog.len {''')
unit = replace(unit, '''        var next: i64 = -1;
        if filter {''', '''        var next: i64 = device_token;
        if device_token >= 0 {
            // The resident evaluator already applied the exact greedy policy.
        } else if filter {''')
unit = replace(unit, '''        if q35_generation_eval(batch[0 .. 1], logits[..], false) != vocabulary {
            q35_generation_reset();
            return -1;
        }''', '''        if device_choice {
            device_token = q35_choose_next(batch[0], logits[..], s.ignore_eos);
            // -2 denotes a completed step with full legacy logits available.
            if device_token < 0 && device_token != -2 {
                q35_generation_reset();
                return -1;
            }
        } else if q35_generation_eval(batch[0 .. 1], logits[..], false) != vocabulary {
            q35_generation_reset();
            return -1;
        }''')
output.write_text(unit)
