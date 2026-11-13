fn entry(
    p: const Plan &,
    x: const u8[:],
    y: const Q8[:],
    routes: const Route[:],
    w: Workspace &,
    gxx: const u8[8][:],
    gxs: const u8[8][:],
    gs: const u8[8][:],
    g3x: const u8[4][:],
    g3s: const u8[4][:],
    g1: const i8[8][:],
    signs: const u8[:],
    palette: const i8[:],
    dst: f32[:]) -> i64 {
    if p.mode == 5 {
        return workspace_offset(p);
    }

    if p.mode == 0 {
        unpack_panel(p, x, w, gxx, gxs, gs, g3x, g3s, g1, signs, palette);
        return 1;
    }

    if p.mode == 1 {
        micro(p, w);
        return 1;
    }

    if p.mode == 2 {
        gather_y(p, y, routes, w);
        return 1;
    }

    if p.mode == 3 {
        if run_moe(p, x, y, routes, w, dst, gxx, gxs, gs, g3x, g3s, g1, signs, palette) {
            return 1;
        }
        return 0;
    }
    assert(p.mode == 4);

    if run_dense(p, x, y, routes, w, dst, gxx, gxs, gs, g3x, g3s, g1, signs, palette) {
        return 1;
    }
    return 0;
}

fn main() {
    var p: Plan = default < Plan > ();
    p.nk = 1;
    p.stride = 144;
    let x: u8[1] = default < u8[1] > ();
    let y: Q8[1] = default < Q8[1] > ();
    let routes: Route[1] = default < Route[1] > ();
    var w: Workspace = default < Workspace > ();
    let g8: u8[8][1] = default < u8[8][1] > ();
    let g4: u8[4][1] = default < u8[4][1] > ();
    let g1: i8[8][1] = default < i8[8][1] > ();
    let signs: u8[128] = default < u8[128] > ();
    let palette: i8[16] = default < i8[16] > ();
    var dst: f32[1] = default < f32[1] > ();
    let status = entry(p, x, y, routes, w, g8, g8, g8, g4, g4, g1, signs, palette, dst);
    print(status);
}

