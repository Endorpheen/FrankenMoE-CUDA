# Retiring the batch rewriter: it produced a behaviour-changing port and cannot be trusted for C++

`tools/rewrite-graph-reuse.py` is deleted rather than repaired. The reason is not that it was buggy in a way a
patch would fix; it is that the approach it encoded is unsafe for this job.

## What it did wrong, in one line

It matched `res &= ` anywhere in the text, including inside comments, and it flattened guarded rows to
`present = 1`. Those two choices changed behaviour in five functions (EXP-892), and the reference hash still
matched, so nothing caught it except a later audit.

## Why repairing it is not the answer

Every fix I made to it produced another instance of the same class of fault: the inserted comment contained the
literal the post-check counted (twice - `res &= ` and then `fg_gr_check`), the boundary search `\n}\n` landed past
the function and swallowed the next one, and the offsets it stored stopped being valid after an edit. Each of
those was found only by an assertion I had just written, which means the assertions, not the transformation, were
doing the work.

The transformation itself is not mechanical. `attn_kv_iswa` and `mem_hybrid_iswa` interleave guarded and
unguarded rows; `dsv4` has a prelude the block must follow; `attn_temp` has a presence row plus a single equality;
`pos` and `out_ids` have one row each and are arguably not worth a bridge at all. A script that handles all of
that is a C++ parser, and a C++ parser written to avoid reading the code is how the defect got in.

## What replaces it

Read the module, understand the guards, write the rows by hand with the guard structure preserved, and say what
changed. That is what the last three commits did, and none of them needed a rewriter:

- `e9dbd1dc` attn_kv_iswa, 4 guarded rows
- `c5e6ff02` attn_kv_msa, attn_k_iswa, dsv4, mem_hybrid_iswa, attn_temp
- `d1def8bc` removal of the two native bodies that were actually proven

Automation remains welcome for checks - the decision-coverage tool, the native-mass map, the differential gates -
where a wrong answer is visible as a number rather than as silent behaviour.
