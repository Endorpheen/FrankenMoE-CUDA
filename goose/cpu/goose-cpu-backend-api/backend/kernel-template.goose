extern "fg145_meta" fn meta(context: i64, field: i64) -> i64;
extern "fg145_float" fn fp(context: i64, field: i64) -> f32;
extern "fg145_action" fn action(context: i64, op: i64, a: i64, b: i64) -> i64;
extern "fg145_witness" fn witness(mode: i64);

fn put(c: i64, field: i64, value: i64) {
    let result = action(c, 1, field, value);
}

fn plan_put(c: i64, field: i64, value: i64) {
    let result = action(c, 12, field, value);
}

fn configure_plan(c: i64) {
    plan_put(c, 1, meta(c, 6));
    plan_put(c, 2, meta(c, 7));
    plan_put(c, 3, meta(c, 8));
}

fn create_backend(c: i64) -> i64 {
    if action(c, 0, 0, 0) == 0 {
        return 0;
    }
    put(c, 0, meta(c, 1));
    for field in 6 {
        put(c, field + 1, 0);
    }
    let backend = action(c, 2, 0, 0);

    if backend == 0 {
        let ignored = action(c, 3, 0, 0);
    }
    return backend;
}

fn release_backend(c: i64) {
    let work = meta(c, 4);
    let a = action(c, 6, work, 0);
    let b = action(c, 3, 0, 0);
    let d = action(c, 4, 0, 0);
}

fn create_plan(c: i64) -> i64 {
    let plan = action(c, 8, 0, 0);

    if plan == 0 {
        return 0;
    }
    let a = action(c, 10, 0, 0);
    let b = action(c, 11, 0, 0);
    let size = meta(c, 13);
    assert(size >= 0);

    if size > 0 {
        let work = action(c, 5, size, 0);

        if work == 0 {
            let ignored = action(c, 9, 0, 0);
            return 0;
        }
        plan_put(c, 0, work);
    }
    configure_plan(c);
    return plan;
}

fn release_plan(c: i64) {
    let a = action(c, 6, meta(c, 14), 0);
    let b = action(c, 9, 0, 0);
}

fn compute_graph(c: i64) -> i64 {
    let ignored = action(c, 10, 0, 0);
    let requested = meta(c, 13);
    assert(requested >= 0);

    if meta(c, 5) < requested {
        let released = action(c, 6, meta(c, 4), 0);
        put(c, 2, 0);
        put(c, 3, 0);
        let work = action(c, 5, requested, 0);

        if work == 0 {
            return meta(c, 47);
        }
        put(c, 2, work);
        put(c, 3, requested);
    }
    plan_put(c, 0, meta(c, 4));
    configure_plan(c);
    return action(c, 13, 0, 0);
}

fn extra_member(c: i64, buffer: i64) -> bool {
    for index in meta(c, 40) {
        if meta(c, 200 + index) == buffer {
            return true;
        }
    }
    return false;
}

fn supports(c: i64) -> bool {
    let op = meta(c, 20);

    if op == OP_NONE || op == OP_RESHAPE || op == OP_VIEW || op == OP_TRANSPOSE || op == OP_PERMUTE {
        return true;
    }
    for source in 4 {
        let buffer = meta(c, 100 + source);

        if buffer != 0 && extra_member(c, buffer) {
            return action(c, 14, source, 0) != 0;
        }
    }
    let x = meta(c, 22);
    let y = meta(c, 23);
    let kind = meta(c, 21);

    if op == OP_CPY || op == OP_SET_ROWS {
        return kind != TYPE_IQ3_XXS && kind != TYPE_IQ3_S && kind != TYPE_IQ2_XXS && kind != TYPE_IQ2_XS && kind != TYPE_IQ2_S && kind != TYPE_IQ1_S && kind != TYPE_IQ1_M;
    }

    if op == OP_MUL_MAT {
        if meta(c, 28) == meta(c, 29) && x == TYPE_F32 && kind == TYPE_F32 {
            return y == TYPE_F32 || y == TYPE_F16;
        }
        let vector = meta(c, 24);
        return y == TYPE_F32 || y == vector || (y == TYPE_BF16 && vector == TYPE_F32);
    }

    if op == OP_SOFT_MAX_BACK {
        return x == TYPE_F32 && y == TYPE_F32 && fp(c, 0) == (0 as f32);
    }

    if op == OP_IM2COL_BACK {
        return x == TYPE_F32 && (y == TYPE_F32 || y == TYPE_F16);
    }

    if op == OP_GET_ROWS_BACK {
        return x == TYPE_F32 || x == TYPE_F16;
    }

    if op == OP_OUT_PROD {
        return (x == TYPE_F32 || ( (x == TYPE_F16 || meta(c, 25) != 0) && meta(c, 31) == meta(c, 32) && meta(c, 33) == meta(c, 34))) && y == TYPE_F32 && kind == TYPE_F32;
    }

    if op == OP_CONV_2D {
        return meta(c, 26) != 0;
    }

    if op == OP_SSM_SCAN {
        return meta(c, 27) == 1 || meta(c, 30) == 1;
    }
    return true;
}

fn build_extras(c: i64) {
    for provider in 4 {
        let value = action(c, 16, provider, 0);

        if value != 0 {
            let ignored = action(c, 15, value, 0);
        }
    }
}

fn copy_extras(c: i64) {
    for index in meta(c, 40) {
        let ignored = action(c, 15, meta(c, 200 + index), 0);
    }
    let ignored = action(c, 15, 0, 0);
}

fn proc_address(c: i64) -> i64 {
    if meta(
        c,
        43) == 26 && meta(c,
        300) == 103 && meta(c,
        301) == 103 && meta(c,
        302) == 109 && meta(c,
        303) == 108 && meta(c,
        304) == 95 && meta(c,
        305) == 98 && meta(c,
        306) == 97 && meta(c,
        307) == 99 && meta(c,
        308) == 107 && meta(c,
        309) == 101 && meta(c,
        310) == 110 && meta(c,
        311) == 100 && meta(c,
        312) == 95 && meta(c,
        313) == 115 && meta(c,
        314) == 101 && meta(c,
        315) == 116 && meta(c,
        316) == 95 && meta(c,
        317) == 110 && meta(c,
        318) == 95 && meta(c,
        319) == 116 && meta(c,
        320) == 104 && meta(c,
        321) == 114 && meta(c,
        322) == 101 && meta(c,
        323) == 97 && meta(c,
        324) == 100 && meta(c,
        325) == 115 {
        return action(c, 17, 0, 0);
    }

    if meta(
        c,
        43) == 32 && meta(c,
        300) == 103 && meta(c,
        301) == 103 && meta(c,
        302) == 109 && meta(c,
        303) == 108 && meta(c,
        304) == 95 && meta(c,
        305) == 98 && meta(c,
        306) == 97 && meta(c,
        307) == 99 && meta(c,
        308) == 107 && meta(c,
        309) == 101 && meta(c,
        310) == 110 && meta(c,
        311) == 100 && meta(c,
        312) == 95 && meta(c,
        313) == 100 && meta(c,
        314) == 101 && meta(c,
        315) == 118 && meta(c,
        316) == 95 && meta(c,
        317) == 103 && meta(c,
        318) == 101 && meta(c,
        319) == 116 && meta(c,
        320) == 95 && meta(c,
        321) == 101 && meta(c,
        322) == 120 && meta(c,
        323) == 116 && meta(c,
        324) == 114 && meta(c,
        325) == 97 && meta(c,
        326) == 95 && meta(c,
        327) == 98 && meta(c,
        328) == 117 && meta(c,
        329) == 102 && meta(c,
        330) == 116 && meta(c,
        331) == 115 {
        return action(c, 17, 1, 0);
    }

    if meta(
        c,
        43) == 25 && meta(c,
        300) == 103 && meta(c,
        301) == 103 && meta(c,
        302) == 109 && meta(c,
        303) == 108 && meta(c,
        304) == 95 && meta(c,
        305) == 98 && meta(c,
        306) == 97 && meta(c,
        307) == 99 && meta(c,
        308) == 107 && meta(c,
        309) == 101 && meta(c,
        310) == 110 && meta(c,
        311) == 100 && meta(c,
        312) == 95 && meta(c,
        313) == 103 && meta(c,
        314) == 101 && meta(c,
        315) == 116 && meta(c,
        316) == 95 && meta(c,
        317) == 102 && meta(c,
        318) == 101 && meta(c,
        319) == 97 && meta(c,
        320) == 116 && meta(c,
        321) == 117 && meta(c,
        322) == 114 && meta(c,
        323) == 101 && meta(c,
        324) == 115 {
        return action(c, 17, 2, 0);
    }

    if meta(
        c,
        43) == 31 && meta(c,
        300) == 103 && meta(c,
        301) == 103 && meta(c,
        302) == 109 && meta(c,
        303) == 108 && meta(c,
        304) == 95 && meta(c,
        305) == 98 && meta(c,
        306) == 97 && meta(c,
        307) == 99 && meta(c,
        308) == 107 && meta(c,
        309) == 101 && meta(c,
        310) == 110 && meta(c,
        311) == 100 && meta(c,
        312) == 95 && meta(c,
        313) == 115 && meta(c,
        314) == 101 && meta(c,
        315) == 116 && meta(c,
        316) == 95 && meta(c,
        317) == 97 && meta(c,
        318) == 98 && meta(c,
        319) == 111 && meta(c,
        320) == 114 && meta(c,
        321) == 116 && meta(c,
        322) == 95 && meta(c,
        323) == 99 && meta(c,
        324) == 97 && meta(c,
        325) == 108 && meta(c,
        326) == 108 && meta(c,
        327) == 98 && meta(c,
        328) == 97 && meta(c,
        329) == 99 && meta(c,
        330) == 107 {
        return action(c, 17, 3, 0);
    }

    if meta(
        c,
        43) == 26 && meta(c,
        300) == 103 && meta(c,
        301) == 103 && meta(c,
        302) == 109 && meta(c,
        303) == 108 && meta(c,
        304) == 95 && meta(c,
        305) == 98 && meta(c,
        306) == 97 && meta(c,
        307) == 99 && meta(c,
        308) == 107 && meta(c,
        309) == 101 && meta(c,
        310) == 110 && meta(c,
        311) == 100 && meta(c,
        312) == 95 && meta(c,
        313) == 99 && meta(c,
        314) == 112 && meta(c,
        315) == 117 && meta(c,
        316) == 95 && meta(c,
        317) == 110 && meta(c,
        318) == 117 && meta(c,
        319) == 109 && meta(c,
        320) == 97 && meta(c,
        321) == 95 && meta(c,
        322) == 105 && meta(c,
        323) == 110 && meta(c,
        324) == 105 && meta(c,
        325) == 116 {
        return action(c, 17, 4, 0);
    }

    if meta(
        c,
        43) == 24 && meta(c,
        300) == 103 && meta(c,
        301) == 103 && meta(c,
        302) == 109 && meta(c,
        303) == 108 && meta(c,
        304) == 95 && meta(c,
        305) == 98 && meta(c,
        306) == 97 && meta(c,
        307) == 99 && meta(c,
        308) == 107 && meta(c,
        309) == 101 && meta(c,
        310) == 110 && meta(c,
        311) == 100 && meta(c,
        312) == 95 && meta(c,
        313) == 99 && meta(c,
        314) == 112 && meta(c,
        315) == 117 && meta(c,
        316) == 95 && meta(c,
        317) == 105 && meta(c,
        318) == 115 && meta(c,
        319) == 95 && meta(c,
        320) == 110 && meta(c,
        321) == 117 && meta(c,
        322) == 109 && meta(c,
        323) == 97 {
        return action(c, 17, 5, 0);
    }

    if meta(
        c,
        43) == 28 && meta(c,
        300) == 103 && meta(c,
        301) == 103 && meta(c,
        302) == 109 && meta(c,
        303) == 108 && meta(c,
        304) == 95 && meta(c,
        305) == 98 && meta(c,
        306) == 97 && meta(c,
        307) == 99 && meta(c,
        308) == 107 && meta(c,
        309) == 101 && meta(c,
        310) == 110 && meta(c,
        311) == 100 && meta(c,
        312) == 95 && meta(c,
        313) == 99 && meta(c,
        314) == 112 && meta(c,
        315) == 117 && meta(c,
        316) == 95 && meta(c,
        317) == 115 && meta(c,
        318) == 101 && meta(c,
        319) == 116 && meta(c,
        320) == 95 && meta(c,
        321) == 117 && meta(c,
        322) == 115 && meta(c,
        323) == 101 && meta(c,
        324) == 95 && meta(c,
        325) == 114 && meta(c,
        326) == 101 && meta(c,
        327) == 102 {
        return action(c, 17, 6, 0);
    }

    if meta(
        c,
        43) == 19 && meta(c,
        300) == 103 && meta(c,
        301) == 103 && meta(c,
        302) == 109 && meta(c,
        303) == 108 && meta(c,
        304) == 95 && meta(c,
        305) == 116 && meta(c,
        306) == 104 && meta(c,
        307) == 114 && meta(c,
        308) == 101 && meta(c,
        309) == 97 && meta(c,
        310) == 100 && meta(c,
        311) == 112 && meta(c,
        312) == 111 && meta(c,
        313) == 111 && meta(c,
        314) == 108 && meta(c,
        315) == 95 && meta(c,
        316) == 110 && meta(c,
        317) == 101 && meta(c,
        318) == 119 {
        return action(c, 17, 7, 0);
    }

    if meta(
        c,
        43) == 20 && meta(c,
        300) == 103 && meta(c,
        301) == 103 && meta(c,
        302) == 109 && meta(c,
        303) == 108 && meta(c,
        304) == 95 && meta(c,
        305) == 116 && meta(c,
        306) == 104 && meta(c,
        307) == 114 && meta(c,
        308) == 101 && meta(c,
        309) == 97 && meta(c,
        310) == 100 && meta(c,
        311) == 112 && meta(c,
        312) == 111 && meta(c,
        313) == 111 && meta(c,
        314) == 108 && meta(c,
        315) == 95 && meta(c,
        316) == 102 && meta(c,
        317) == 114 && meta(c,
        318) == 101 && meta(c,
        319) == 101 {
        return action(c, 17, 8, 0);
    }

    if meta(
        c,
        43) == 31 && meta(c,
        300) == 103 && meta(c,
        301) == 103 && meta(c,
        302) == 109 && meta(c,
        303) == 108 && meta(c,
        304) == 95 && meta(c,
        305) == 98 && meta(c,
        306) == 97 && meta(c,
        307) == 99 && meta(c,
        308) == 107 && meta(c,
        309) == 101 && meta(c,
        310) == 110 && meta(c,
        311) == 100 && meta(c,
        312) == 95 && meta(c,
        313) == 99 && meta(c,
        314) == 112 && meta(c,
        315) == 117 && meta(c,
        316) == 95 && meta(c,
        317) == 115 && meta(c,
        318) == 101 && meta(c,
        319) == 116 && meta(c,
        320) == 95 && meta(c,
        321) == 116 && meta(c,
        322) == 104 && meta(c,
        323) == 114 && meta(c,
        324) == 101 && meta(c,
        325) == 97 && meta(c,
        326) == 100 && meta(c,
        327) == 112 && meta(c,
        328) == 111 && meta(c,
        329) == 111 && meta(c,
        330) == 108 {
        return action(c, 17, 9, 0);
    }
    return 0;
}

fn memory_info(c: i64) {
    var total: i64 = 0;
    var available: i64 = 0;

    if meta(c, 44) != 0 {
        total = meta(c, 45);
        available = meta(c, 46);
    } else {
        let pages = meta(c, 45);
        let page_bytes = meta(c, 46);
        assert(pages >= 0 && page_bytes > 0 && pages <= 9223372036854775807 / page_bytes);
        total = pages * page_bytes;
        available = total;
    }
    let a = action(c, 18, 0, available);
    let b = action(c, 18, 1, total);
}

fn features(c: i64) {
    for feature in meta(c, 51) {
        if meta(c, 400 + feature) > 0 {
            let ignored = action(c, 19, feature, 0);
        }
    }
    let ignored = action(c, 19, -1, 0);
}

fn entry(mode: i64, c: i64) -> i64 {
    witness(mode);

    if mode == 0 {
        return create_backend(c);
    }

    if mode == 1 {
        release_backend(c);
        return 0;
    }

    if mode == 2 {
        return create_plan(c);
    }

    if mode == 3 {
        release_plan(c);
        return 0;
    }

    if mode == 4 {
        return action(c, 13, 1, 0);
    }

    if mode == 5 {
        return compute_graph(c);
    }

    if mode >= 6 && mode <= 9 {
        assert(meta(c, 0) != 0);

        if mode == 6 {
            put(c, 0, meta(c, 9));
        } else

        if mode == 7 {
            let old = meta(c, 3);
            let next = meta(c, 10);

            if old != 0 && old != next {
                let ignored = action(c, 7, old, 0);
            }
            put(c, 1, next);
        } else

        if mode == 8 {
            put(c, 4, meta(c, 10));
            put(c, 5, meta(c, 11));
        } else {
            put(c, 6, meta(c, 9));
        }
        return 0;
    }

    if mode == 10 {
        return if supports(c) {
            1
        } else {
            0
        }
        ;
    }

    if mode == 11 {
        return if meta(c, 41) != 0 || extra_member(c, meta(c, 42)) {
            1
        } else {
            0
        }
        ;
    }

    if mode == 12 {
        return if extra_member(c, meta(c, 42)) {
            1
        } else {
            0
        }
        ;
    }

    if mode == 13 {
        build_extras(c);
        return 0;
    }

    if mode == 14 {
        copy_extras(c);
        return 0;
    }

    if mode == 15 {
        return proc_address(c);
    }

    if mode == 16 {
        memory_info(c);
        return 0;
    }

    if mode == 17 {
        return 1;
    }
    assert(mode == 18);
    features(c);
    return 0;
}

fn main() {
    let value = entry(17, 0);
    print(value);
}

