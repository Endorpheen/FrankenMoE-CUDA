# EXP214: stock std.sort replaces the hand-written loader sort — ACCEPTED

EXP201 had to hand-write an insertion sort for `merge_ranges` because the language had no ordering
primitive. `subset-29883c08ce59e7fe` qualifies the stock `std.sort`, so the loader can use the
route native actually takes (`llama-model-loader.cpp:1483` calls `std::sort`).

## What was verified before wiring it

- `import std;` then bare `sort(...)` works; bare `sort` without the import is still
  `unknown function`, and `std::sort(...)` is also unknown. The working form is the unqualified
  call after the import.
- The comparator form works and **captures arrays**: sorting an index array by a separate key
  array gives `3120` for keys `[3,1,2,0]`, i.e. indices `[3,1,2,0]`.
- A range is `a[0..n)` — `a[0..4]` is four elements. My first probes assumed inclusive bounds and
  reported a wrong result; the module was right and the probe was wrong.

## Result — ACCEPTED

    FG201L_RANGES merge=2 sub=1 sort=2
    2554.29 / 25.97 first run, tokens and all 128 probabilities exact, sha e68df05b
    gate: FG201L_FIXTURE checks=6342 failures=0 (unchanged, still against std::algorithm)

## Balanced interleaved measurement (A = EXP213, B = EXP214)

| pair | prefill | decode |
|---|---:|---:|
| 1 | +0.44% | −2.53% |
| 2 | −0.55% | +0.05% |
| **mean** | **−0.06%** | **−1.25%** |

Inside the 2% rule on the mean. Reported honestly: pair 1 decode exceeded 2% on its own and pair 2
did not, so the true effect is within this stage's measurement noise. Two pairs cannot resolve
better than that, and one pair must not be used alone either way.

## Shape

`sort_pairs` uses stock `std.sort` over an index array when the count fits a compile-time bound of
256, and keeps the insertion sort as the general path — "small in practice" is not a bound, and
artificially capping mmap range count would be a silent failure mode. The pairs are read in sorted
order into staging arrays before being written back, because an in-place scatter would overwrite
pairs it had not read yet.

The compile-time array bound also requires `[0;N]` initialisation: `var a:i64[256];` is
"variable read before initialization".
