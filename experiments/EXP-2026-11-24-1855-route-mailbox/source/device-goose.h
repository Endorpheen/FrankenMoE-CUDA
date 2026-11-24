typedef struct { float e[8]; } a8_f32;

static void goose_compact_route_g(uint64_t p0, uint64_t p1, uint64_t p2, float p3, int32_t p4);

/* ---- code ---- */
static void goose_compact_route_g(uint64_t x_g, uint64_t ids_g, uint64_t weights_g, float scale_g, int32_t lane_g) {
    a8_f32 t0;
    a8_f32 scores_g;
    float t1 = 0.0f;
    for (int64_t t2 = 0; t2 < 8; t2++) {
        t0.e[t2] = t1;
    }
    scores_g = t0;
    int64_t t3 = 8;
    for (int64_t i_g = 0; i_g < t3; i_g++) {
        int64_t t6 = i_g;
        uint64_t t7 = x_g;
        int32_t t8 = gs_add_i32((int32_t)(gs_mul_i32((int32_t)(32), (int32_t)GS_RANGE((int64_t)(i_g), (-2147483647 - 1), 2147483647, "i32", gs_file0, 13), gs_file0, 13)), lane_g, gs_file0, 13);
        float t9 = route_score(t7, t8);
        scores_g.e[t6] = t9;
    }
    float picked_g = 0.0f;
    int64_t t10 = 8;
    for (int64_t slot_g = 0; slot_g < t10; slot_g++) {
        float key_g = scores_g.e[0];
        int32_t expert_g = lane_g;
        int64_t t13 = 1;
        int64_t t14 = 8;
        for (int64_t i_g_2 = t13; i_g_2 < t14; i_g_2++) {
            int64_t t17 = i_g_2;
            if ((uint8_t)((float)((float)(scores_g.e[t17])) > (float)(key_g))) {
                int64_t t18 = i_g_2;
                key_g = scores_g.e[t18];
                expert_g = gs_add_i32((int32_t)(lane_g), gs_mul_i32((int32_t)(32), (int32_t)GS_RANGE((int64_t)(i_g_2), (-2147483647 - 1), 2147483647, "i32", gs_file0, 21), gs_file0, 21), gs_file0, 21);
            }
        }
        int64_t t19 = 5;
        for (int64_t step_g = 0; step_g < t19; step_g++) {
            int32_t offset_g = (int32_t)GS_RANGE((int64_t)((int64_t)((uint64_t)((int64_t)((int64_t)(16))) >> ((int64_t)(step_g) & 63))), (-2147483647 - 1), 2147483647, "i32", gs_file0, 25);
            float alt_g;
            float t22 = key_g;
            int32_t t23 = offset_g;
            float t24 = route_swap_f32(t22, t23);
            alt_g = t24;
            int32_t other_g;
            int32_t t25 = expert_g;
            int32_t t26 = offset_g;
            int32_t t27 = route_swap_i32(t25, t26);
            other_g = t27;
            if ((uint8_t)((float)((float)(alt_g)) > (float)(key_g))) goto L29;
            if (!((uint8_t)((float)((float)(alt_g)) == (float)(key_g)))) goto L28;
            if (!((uint8_t)((int32_t)((int32_t)(other_g)) < (int32_t)(expert_g)))) goto L28;
            L29:;
            {
                key_g = alt_g;
                expert_g = other_g;
            }
            L28:;
        }
        if ((uint8_t)((int32_t)((int32_t)((int32_t)((int32_t)((int32_t)(expert_g)) & (int32_t)(31)))) == (int32_t)(lane_g))) {
            int64_t t30 = 8;
            for (int64_t i_g_3 = 0; i_g_3 < t30; i_g_3++) {
                if ((uint8_t)((int32_t)((int32_t)(gs_shr_i32((int32_t)(expert_g), (int64_t)(5)))) == (int32_t)((int32_t)GS_RANGE((int64_t)(i_g_3), (-2147483647 - 1), 2147483647, "i32", gs_file0, 35)))) {
                    int64_t t33 = i_g_3;
                    float t34 = route_neg_inf();
                    scores_g.e[t33] = t34;
                }
            }
        }
        if ((uint8_t)((int32_t)((int32_t)(lane_g)) == (int32_t)((int32_t)GS_RANGE((int64_t)(slot_g), (-2147483647 - 1), 2147483647, "i32", gs_file0, 38)))) {
            picked_g = key_g;
            uint64_t t35 = ids_g;
            int32_t t36 = lane_g;
            int32_t t37 = expert_g;
            route_write_id(t35, t36, t37);
        }
    }
    double sum_g = 0.0;
    if ((uint8_t)((int32_t)((int32_t)(lane_g)) < (int32_t)(8))) {
        sum_g = sum_g + ((double)(picked_g));
    }
    int64_t t38 = 5;
    for (int64_t step_g_2 = 0; step_g_2 < t38; step_g_2++) {
        double t41 = sum_g;
        double t42 = sum_g;
        int32_t t43 = (int32_t)GS_RANGE((int64_t)((int64_t)((uint64_t)((int64_t)((int64_t)(16))) >> ((int64_t)(step_g_2) & 63))), (-2147483647 - 1), 2147483647, "i32", gs_file0, 45);
        double t44 = route_swap_f64(t42, t43);
        sum_g = t41 + (t44);
    }
    if ((uint8_t)((int32_t)((int32_t)(lane_g)) < (int32_t)(8))) {
        float t45 = picked_g;
        float t46 = (float)(sum_g);
        float t47 = route_clamp(t46);
        float weight_g = ((float)(t45) / (float)(t47));
        uint64_t t48 = weights_g;
        int32_t t49 = lane_g;
        float t50;
        if ((uint8_t)((float)((float)(scale_g)) == (float)(0.0f))) goto L52;
        if (!((uint8_t)((float)((float)(scale_g)) == (float)(1.0f)))) goto L51;
        L52:;
        {
            t50 = weight_g;
        }
        goto L53;
        L51:;
        {
            float t54 = weight_g;
            float t55 = scale_g;
            float t56 = route_scaled(t54, t55);
            t50 = t56;
        }
        L53:;
        float t57 = t50;
        route_write_score(t48, t49, t57);
    }
    return;
}
