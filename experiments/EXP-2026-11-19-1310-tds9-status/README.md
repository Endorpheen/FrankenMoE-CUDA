# EXP-2026-11-19-1310: TD-S9 stays a workaround, and the workaround is what carries the evidence

## The blocker as recorded

`fg_sampler_typical` cannot be exported directly - the compiler reports that it cannot resolve one concrete
function signature - and the module carries a comment saying so. The port ships a thin door, `fg_typical_entry`,
that calls it, and the host uses that door.

## An attempt to isolate the cause, and why it failed

Copying the composed unit to a scratch directory, marking the function `export fn` and running `--check` gave a
**different** error, about a line that is fine in the real build:

```
sampler-chain-root.goose:693: error: expected top-level declaration
export fn fg_dry_rep_limit_entry(...)
```

So the scratch copy is not equivalent to the build the script performs - it loses whatever ordering or framing the
generator applies - and the experiment says nothing about the export problem. Recording that plainly is better than
quoting the error as if it were a finding.

## What is actually verified

The workaround is verified in both worlds:

- `fg_typical_entry` is what the host calls, and the harness that drives it reports **1800 of 1800** cases identical
  against the native typical-p implementation (EXP-2026-11-18-1188);
- the same door is inside the profile's chain, which matches the native path over 120 rounds (EXP-2026-11-19-1286);
- `fg_typical_entry` is one of the eleven accepted components the pre-flight check verifies in the shipped library.

So the compiler limitation costs a wrapper, not correctness, and it costs nothing at run time beyond a call.

## Status

Open as a compiler issue with a working workaround. It does not block any accepted path, and it is not evidence of a
defect in the port.
