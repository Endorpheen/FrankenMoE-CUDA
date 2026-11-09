# Perf check found a real regression the Apex profile cannot see

`test-backend-ops perf` does print timings for operators with a meaningful size, contrary to the
first reading of this build — it prints for `CPY` and not for `RMS_NORM`/`GET_ROWS`, so the earlier
note "no timings" was wrong about the mode, not about those operators.

## The measurement

`test-backend-ops perf -o CPY -b CUDA0`, same DSO, `GGML_GOOSE_CUDA_CPY=0` for the native arm,
microseconds per run:

| case | Goose | native | delta |
|---|---:|---:|---:|
| f32→f32 786432×256, `permute_src=[1,0,2,3]` | 15304 | 4168 | **+267%** |
| f32→f32 768×1024×256, transposed | 6756 | 3930 | **+72%** |
| f32→f32 8192×512×2, permuted | 238 | 186 | +28% |
| f32→f32 3072×512×2, permuted | 85 | 64 | +32% |
| f32→f32 512×3072 | 15.8 | 15.4 | +2.8% |
| f16→f16 all transposed cases | 2545 / 1901 / 1890 | 2548 / 1903 / 1889 | ±0.1% |
| q4_0→f32 and f32→q4_0 | 434 / 92 | 432 / 92 | ±0.3% |

The f16 and quantized copies are exact. The **f32 transposed path is up to 3.7× slower**.

## Why Apex never showed it

The live witness reads `FG177C_CUDA_CPY host=0x4,paths=0x1` — bit 0 only, which is the same-type
contiguous memcpy route. The transposed route (route 4) and the element route (route 2) are not
executed by this profile at all, so a whole-model Apex pair could not have caught this.

That is the general lesson already in these documents, in a new form: a green Apex run bounds what
the profile executes, not what the module does.

## Correctness is unaffected

`test-backend-ops test -o CPY` is 2/2 passed. This is a performance regression, not a wrong result.

## The fix, not yet applied

Declining route 4 to the native branch would restore the native speed on a path Apex does not
use, exactly as the `sample_ratio` fix declined broadcast shapes. Not applied in this pass because
it changes what the module covers and that decision should be recorded with its own paired
measurement rather than folded into a diagnostic note.

Severity: no effect on the Apex profile. Real for any workload that does large f32 transposed
copies through this backend.
