# Named-result fix, subset-7ef08c2d0b65dffe — verified

The compiler owner repaired the defect reported in
`compiler-named-result-preexisting-defect-1` and qualified `subset-7ef08c2d0b65dffe`
(sha256 `7ef08c2d0b65dffe…1dc94c`). Verified here by execution, not by reading the note.

## The reported case now returns the correct value

    fn make(flag:bool)->i64[>..]{
        var first:i64[>..]=[1];
        var last:i64[>..]=[2];
        if flag { return first; }
        last
    }
    make(false)   was 1 (wrong)   now 2   correct

## The other forms the packet claims

One function holding **four live growable results** at once, each read back and summed:

| result | expected | got |
|---|---|---|
| `selfret()` → `[4,5]` | len 2, 4, 5 | matches |
| `looplocal()` → `[1,11]` | len 2, 1, 11 | matches |
| `make(false)` → `[2]` | 2 | matches |
| `make(true)` → `[1]` | 1 | matches |

All four correct in one pass. Also checked: an implicit nonfixed copy (`var b:i64[>..]=a;` where `a`
is fixed) is **refused** by the compiler with "nonfixed storage is not copied implicitly" — a
documented refusal, not silent corruption.

## A false alarm I raised against myself

My first four-result test decoded `252121` as `r0=2 r1=1` and I started writing up a new defect.
There is no defect: I packed the results as `... + r[0]*10 + r[1]*1`, but `r[1]` is **11**, so it
carries into the tens digit. Re-decoding by hand gives `r0=1, r1=11`, which is correct. Repacking
with non-colliding weights gave 245269 against an expected 245269.

Worth recording because it is the **second** time a bug in my own harness looked like a bug in the
port (the first was EXP215's fork hiding `adv`). A suspicious result in a new fixture should first
be checked against the arithmetic of the fixture itself.

I also confirmed the corruption reproduces identically on `d5e9fdc5` **and** on `29883cd0`, which
was what made me distrust the finding: a new-repair regression would not appear unchanged in the
oldest build.

## Compatibility of the accepted modules

All eight regenerated and gated on this build:

    EXP199  74123    EXP202   1965    EXP203 593237    EXP209  40877
    EXP210  16410    EXP213   1271    EXP215   1596    EXP216   1617
    total 731,096 checks, 0 failures

Runtime pin unchanged — still module-level forward compatibility, not a chain rebuild.

## Still open per the packet, and not claimed here

Independent result alternatives, multiple result-position NRVO, relative/nonfixed-struct layouts,
last-use/copy diagnostics, full stdlib (8/12 import-check only), and the CUDA slice/MMQ gap. None of
this is closed by the host-buffer repair.
