# EXP298: quantized K/V -- native serves, our line dies with Goose assertions

P5-02's criterion names F32/F16/BF16/Q8/Q4 K/V layouts. The profile uses the default F16 cache, so the quantized layouts
were never exercised live. This pass ran both lines on the same request with `--cache-type-k/--cache-type-v` set.

| cache types | native | ours |
|---|---|---|
| q8_0 / q8_0 | served, `1da0f499...`, "over the lazy dog." | **exit 1**, never became healthy |
| q4_0 / q4_0 | served, `1da0f499...` -- identical to q8_0 and to F16 | **exit 1**, never became healthy |

Native produces the same continuation for all three cache types, which is expected for a deterministic greedy decode on
this prompt.

Our log shows sixteen `goose runtime error: assertion failed` lines immediately after `FG376S_HYBRID_CONTEXT`, before
`FG183F_CUDA_FWHT`, then the process exits 1 without serving.

## Where to look next

The rotation path is the only piece that is enabled *specifically by quantization*: `attn_rot_k` and `attn_rot_v` require
`ggml_is_quantized(type_k)` / `ggml_is_quantized(type_v)` and an embedding width divisible by 64. With an F16 cache the
Hadamard tensors are never built, which is why the accepted profile never touches that code; with q8_0 or q4_0 they are
built and fed through the FWHT module.

So the failure is most likely in the Hadamard/FWHT area (EXP320 generator, EXP333 matrix store, FG183F_CUDA_FWHT) rather
than in the layout arithmetic, and the immediate next step is to identify which module raises the assertion rather than to
guess. A selective switch bisect is the tool: the FWHT and rotation switches can be turned off one at a time, which the
earlier "all switches off" experiment could not do because the server refuses to start with everything disabled.

## Localization: the asserting module is fg143 in libggml-cpu

A first attempt under gdb appeared to succeed, but it was false evidence: without `LD_LIBRARY_PATH` the binary's RPATH
resolved to an older variant directory (`franken-goose-exp150-expert-registry-20261107/variants/cpu-backend/run`), a
different build entirely. With `LD_LIBRARY_PATH` pinned to the stage, the failure reproduces and the backtrace is:

    gg_fail                              libggml-cpu.so.0
    gg_function_11.constprop             libggml-cpu.so.0
    gg_function_1.constprop              libggml-cpu.so.0
    fg143_compute                        libggml-cpu.so.0
    fg143_try.part                       libggml-cpu.so.0

Sixteen identical failures appear because sixteen worker threads reach the assertion, and each `gg_fail` calls `exit(1)`
-- which is also why the server dies without printing a bind message.

## Which side triggers it

| cache K / V | assertions | listening |
|---|---|---|
| q8_0 / q8_0 | 16 | no |
| q8_0 / f16 | 16 | no |

Quantized **K** alone is enough. That points at the K rotation path, which is built only when K is quantized
(`attn_rot_k` requires `ggml_is_quantized(type_k)`), rather than at the V layout.

The next step is to identify the assertion inside fg143's compute for that op -- either by rebuilding the CPU module with
symbols and mapping the failing program counter to its generated function, or by dumping the op name that fg143 dispatches
when the rotation tensor first appears.

## The operator: CPU flash attention with quantized K

A deeper backtrace names the native operation the Goose dispatch wraps:

    gg_fail <- gg_function_11 <- gg_function_1 <- fg143_compute <- fg143_try.part
            <- ggml_compute_forward_flash_attn_ext <- fg143_cpu_control <- fg143_control_dispatch
            <- ggml_graph_compute_thread

So the failing path is CPU `flash_attn_ext`, not the graph construction or the K layout arithmetic. `fg143` does not port
flash attention -- there is no `flash_attn` function in its Goose sources -- it wraps the native call, and the assertion is
in its dispatch helper.

Two limits found:

- The assertion cannot be bypassed with `-fa off`: the context then fails for a different, native reason ("failed to
  create context with model"), so flash attention is effectively mandatory for this substrate and the combination
  q8_0 K + no FA is not a supported configuration.
- No Goose switch exists for the flash-attention dispatch: the 61 names in the DSO contain none for fwht, rotation or
  flash attention, so a switch bisect cannot isolate this path; the isolation has to come from the backtrace, which it now
  has.

Next: inspect the dispatch helper's model of the op at the failing assertion -- the helper walks four per-dimension
values against a limit -- and compare it with the native `flash_attn_ext` preconditions for a quantized K type (block
size 32, type size 34).

## The failing condition, read from the disassembly

At the catchpoint the failing instruction is `gg_function_11.constprop.0+96`, and the code just before it is:

    0+27   test %rax,%rax ; je  +96                     (a value must be non-zero)
    0+32   cmp  $0x1e,%rax ; setne %sil                 (30 = BF16)
    0+40   cmp  $0x1,%rax  ; setne %dl                  (1  = F16)
    0+52   cmp  $0x19,%rax ; jne +224                   (25 = another id)
    0+62   mov  (%rcx),%rax
    0+70   test %rax,%rax ; jns +109                    (a field must be >= 0)
    0+91   call gg_fail  <-- taken

So the helper first dispatches on a type id and then requires a field to be non-negative; with q8_0 K the field is
negative. The dispatch only names three ids explicitly (1, 25, 30), which is the shape of a type table that was written for
the types the accepted profile used, with everything else falling through to a branch that does not expect a block-quantized
row.

That is the concrete hypothesis to test next: find the matching Goose function -- a type switch followed by a non-negative
check on a row/stride field -- and extend it for block-quantized K, rather than patching the wrapper blindly.

## The function is validate(), and the type table is the suspect

`gg_function_11` is `validate(t, available)` from `kernel.goose:32`:

    fn validate(t:&Tensor, available:i64) {
        assert(t.bytes>=0 && t.bytes<=available); let bytes=scalar_bytes(t.kind); assert(bytes>0);
        var last:i64=bytes; var empty:bool=false;
        for axis in 4 { assert(t.ne[axis]>=0&&t.nb[axis]>=0); if ne==0 {empty=true;} else {last=add_size(last,mul_size(ne-1,nb));} }
        assert(empty||last<=t.bytes);
    }

and `scalar_bytes` is `kernel.goose:28`:

    if kind==0||kind==26 {return 4;} if kind==1||kind==30||kind==25 {return 2;}
    if kind==24 {return 1;} if kind==27||kind==28 {return 8;} return 0;

The three ids in the disassembly (0x1, 0x19, 0x1e) are exactly the `1/30/25` arm of that table, and the table returns 0
for every block-quantized kind -- q8_0 is 8, q4_0 is 2. So `assert(bytes>0)` is the leading suspect: `validate` was written
for tensors whose element type has a scalar byte size, and a quantized K has a block layout instead.

Register inspection after the fact is inconclusive: the addresses resolved inside the library are not stable enough to
read the struct reliably without symbols (the breakpoints land in zeros because the module base moves between runs), so the
next step is to rebuild this module with `-g`, catch the assert with line information, and confirm whether `bytes>0` or one
of the per-axis checks fires.

A fix, if the hypothesis holds, is a size model that knows block-quantized kinds (type size and block size per kind) rather
than extending the scalar table with made-up byte counts.

## Root cause, located exactly

`entry()` in `source/entry.goose` validates the secondary sources without asking whether they are scalar:

    if (!scalar_type(p.x) || !scalar_type(p.d)) { return false; }   // x and dst are checked
    validate(p.d, dst.len); validate(p.x, x.len);
    if (p.y.bytes > 0) { validate(p.y, y.len); }                    // k -- not checked first
    if (p.z.bytes > 0) { validate(p.z, z.len); }                    // v -- not checked first

and `validate()` starts with `assert(scalar_bytes(t.kind) > 0)`, where `scalar_bytes` returns zero for every
block-quantized kind. For `flash_attn_ext`, `x` is q and `d` is the F32 result, so both pass the scalar check, `y` is the
q8_0 K cache and `z` the V cache, and the validation of `y` asserts.

The C-side gate does not catch it either: `fg143_try` in `native-bridge.cpp:40` only checks

    if (!fg143_enabled || params->use_ref || !fg143_scalar(t->type)) return 0;

that is, the *destination* type. Sources one and two are never examined before the call.

## Fix

The module does not port flash attention at all -- there is no `flash_attn` in its Goose sources -- so the right behaviour
for a quantized K/V is to decline and let the native implementation run, exactly as the flint line does. Because rebuilding
the Goose module is not a step available here, the minimal correct change is at the gate we own:

    if (t->src[1] && !fg143_scalar(t->src[1]->type)) return 0;
    if (t->src[2] && !fg143_scalar(t->src[2]->type)) return 0;

Only the secondary sources are guarded, so quantized weights -- which arrive as `src[0]` and are handled by the codec path
that runs before the scalar check -- keep their current treatment.

This has to be rebuilt into `libggml-cpu` and gated: the accepted profile must stay at `e68df05b`, and q8_0/q4_0 K/V must
then serve on our line with the same continuation as native (`1da0f499`).
