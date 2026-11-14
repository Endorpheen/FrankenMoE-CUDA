// Shared trunk/NextN skeleton for P6-05 architectures 14 (glm4) and 15
// (glm-dsa).
//
// Both loaders walk n_layer_all blocks and split that walk at n_layer: the
// layers below n_layer are the decoder trunk, the layers at and past it are
// the NextN/MTP block. glm4 loads the extra block with TENSOR_SKIP and never
// runs it, glm-dsa builds a separate MTP graph for it, but the split itself,
// the `n_layer_nextn < n_layer_all` assert and the 2*n_embd NextN projection
// are the same decision in both files, so they live here.
//
// Native anchors:
//   glm4.cpp:8-9, :32-37, :54-64
//   glm-dsa.cpp:60-61, :125-128, :181-191
//
// The C symbol of the abort hook is shared, so the Goose-level name is
// local to this module: both base modules are imported by glm-dsa.goose and
// two declarations of the same identifier would collide.

extern "fg200g_fail" fn trunk_abort(code: i64);

const K_TRUNK: i64 = 0;
const K_NEXTN: i64 = 1;

fn t_ensure(v: bool, c: i64) {
    if !v {
        trunk_abort(c);
    }
}

// t_n_layer_all: hparams block_count, i.e. the trunk plus every NextN block.
// glm4.cpp:32 iterates `n_layer_all`; glm-dsa.cpp:125 does the same.
fn t_n_layer_all(n_layer: i64, nextn: i64) -> i64 {
    t_ensure(n_layer >= 0, 60);
    t_ensure(nextn >= 0, 61);
    return n_layer + nextn;
}

// t_nextn_ok: glm4.cpp:9 and glm-dsa.cpp:61 carry the same assert,
// `n_layer_nextn < n_layer_all`. A file that declares as many NextN blocks as
// it has blocks is rejected, so the plan cannot be built from it.
fn t_nextn_ok(nextn: i64, n_layer_all: i64) -> i64 {
    if nextn < 0 {
        return 0;
    }
    if nextn < n_layer_all {
        return 1;
    }
    return 0;
}

// t_layer_kind: the plan decision for one block. Trunk blocks are the decoder,
// blocks at and past n_layer are the NextN block. A block outside the model is
// reported as -1 rather than guessed.
fn t_layer_kind(n_layer: i64, n_layer_all: i64, il: i64) -> i64 {
    t_ensure(n_layer >= 0, 62);
    t_ensure(n_layer_all >= 0, 63);

    if il < 0 || il >= n_layer_all {
        return -1;
    }
    if il < n_layer {
        return K_TRUNK;
    }
    return K_NEXTN;
}

// t_plan: fill one kind per block and return the count. A short destination is
// a caller bug, not a model property, so it aborts instead of truncating.
fn t_plan(n_layer: i64, n_layer_all: i64, kinds: i64[:]) -> i64 {
    t_ensure(n_layer >= 0, 64);
    t_ensure(n_layer_all >= 0, 65);
    t_ensure(kinds.len >= n_layer_all, 66);

    var i: i64 = 0;
    while i < n_layer_all {
        kinds[i] = t_layer_kind(n_layer, n_layer_all, i);
        i = i + 1;
    }
    return n_layer_all;
}

// t_nextn_eh_proj: glm4.cpp:56 and glm-dsa.cpp:183 both declare the NextN
// projection as {2*n_embd, n_embd}: the block concatenates the normalised
// token embedding and the normalised previous hidden state on dim 0.
fn t_nextn_eh_proj(n_embd: i64) -> i64 {
    t_ensure(n_embd > 0, 67);
    return 2 * n_embd;
}

// t_output_dup: the lm_head weight is optional. When it is absent the loader
// duplicates token_embd (glm4.cpp:27-30, glm-dsa.cpp:119-123), so the shape is
// the same but the source tensor is not. 1 means the duplicated source.
fn t_output_dup(has_output: i64) -> i64 {
    if has_output == 0 {
        return 1;
    }
    return 0;
}
