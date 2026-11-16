# EXP-2026-11-17-880 — the last unwired Goose bridge is wired: file-alignment geometry

Igor's direction (2026-11-17): the base is the WORKING runtime, its C++ already largely ported to
Goose; do not build a parallel standalone. So the work is to find ready ports that are not yet called and
wire them into the one working path.

## How the gap was found

A mechanical check over the shipped library: 67 `fgNNN` bridge prefixes are linked, and comparing them with
the symbols our sources actually call left exactly one unwired:

```
prefixes linked but never called from our sources:  fg377s
```

`fg377s_plan` is EXP-377's aligned-read geometry: for a DirectIO read it decides the aligned offset, the
rounded-up length, the skip into the first block and a capped buffer size. `llama-model-loader.cpp:1860-1868`
computed the same four values inline in C++.

## What changed

`llama-model-loader.cpp` now calls `fg377s_plan` and falls back to the native arithmetic behind
`GGML_GOOSE_FILE_ALIGN=0`. The wrapper ABI is scalar in / array out (`offset, size, alignment, cap ->
{aligned_offset, bytes_to_read, skip, buffer_size}`), so no packet marshalling is needed.

The four outputs were checked against the native expressions before wiring: `align(offset+size) -
aligned_offset` equals the module's `align(skip+size)`, and `skip = offset - aligned_offset`. The behaviour
is meant to be identical, not merely close.

## Verified so far, and what is not

- The bridge is default-on (`fg377s_enabled()` returns 1 from the shipped library).
- Apex with the pinned argv still produces the reference hash `c2ff42cc1219`, decode within the session's
  observed spread (38.45 on a re-run; one 28.81 outlier that a repeat did not reproduce).
- **The call is not exercised on the mmap path**, which is what the pinned profile uses: the aligned-read
  branch runs only when `upload_backend` is set. No `FG377S_FILE_ALIGN` witness appears in any pinned run,
  which is the proof that the branch is not taken.
- Forcing `--no-mmap` to exercise it dies before serving (the 21.7 GB model held whole, on top of the
  buffers), so the wired branch is **not yet witnessed in execution**.

That last point is the honest state: the code is wired, gated and behaviour-preserving on the path it
replaced, and it has not been observed running. Marking it as verified would repeat the mistake this project
keeps finding.

## Next

Exercise the upload path without the memory wall: a smaller model, or `-ngl` reduced so the pinned host
buffers fit, and check for the `FG377S_FILE_ALIGN` witness plus an unchanged output.
