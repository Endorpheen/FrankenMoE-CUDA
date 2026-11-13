// Goose host route + once-init policy for the CUDA GDN MMA path (EXP168).
// Mirrors select_gdn_mma_path(), init<C,V,BLOCKS>() and ggml_cuda_gdn_mma_launch()
// control flow from the native gated-delta-net-mma.cu snapshot. Device math and
// raw SDK/launch effects stay with the parent bridge via the fg174m_* externs;
// this module is pure routing/validation policy. Apex CUDA only, no HIP/MUSA
// branches, and the original numerical host algorithm is never called here.
extern "fg174m_query" fn query(packet: i64, k: i64) -> i64;
extern "fg174m_cc" fn cc_val(k: i64) -> i64;
extern "fg174m_memo_get" fn memo_get(device: i64, path: i64) -> i64;
extern "fg174m_memo_set" fn memo_set(device: i64, path: i64, flag: i64);
extern "fg174m_lock" fn lock(device: i64, path: i64);
extern "fg174m_unlock" fn unlock(device: i64, path: i64);
extern "fg174m_init" fn sdk_init(packet: i64, path: i64, smem: u64) -> i64;
extern "fg174m_clear_error" fn clear_error();
extern "fg174m_launch_raw" fn launch_raw(packet: i64, path: i64, smem: u64) -> i64;
extern "fg174m_fail" fn fail(code: i64);
// Paths: 0=AR (no MMA), 1=V32 blocks1, 2=V32 blocks2, 3=V128.
// Exact dynamic shared bytes of alignas(128) shared<C,V> with the native matrix
// bf16 hi/lo layout (parent static-asserts): 38016 for V32, 93312 for V128.

fn shared_bytes(path: i64) -> u64 {
    return if path == 3 {
        93312 as u64
    } else {
        38016 as u64
    }
    ;
}
// init<C,V,BLOCKS> equivalent: once per device+path with double-checked memo
// flags 0=uninitialized 1=unavailable 2=available.

fn ensure_ready(packet: i64, device: i64, path: i64) -> bool {
    let smem = shared_bytes(path);
    let fast = memo_get(device, path);

    if fast == 2 {
        return true;
    }

    if fast == 1 {
        return false;
    }
    lock(device, path);
    var ok: bool = false;
    let again = memo_get(device, path);

    if again == 2 {
        ok = true;
    } else

    if again == 1 {
        ok = false;
    } else {
        // Native skips cudaFuncSetAttribute when smem exceeds the per-block
        // opt-in limit; the slot stays unavailable without touching the SDK.

        if smem > (query(packet, 10) as u64) {
            ok = false;
        } else {
            let status = sdk_init(packet, path, smem);

            if status == 0 {
                ok = true;
            } else {
                clear_error();
                ok = false;
            }
        }
        memo_set(device, path, if ok {
            2 as i64
        } else {
            1 as i64
        }
        );
    }
    unlock(device, path);
    return ok;
}
// select_gdn_mma_path equivalent, including its leading GGML_ASSERT.

fn select_path(packet: i64, device: i64) -> i64 {
    if device < 0 || device >= query(packet, 11) {
        fail(1);
    }
    let H = query(packet, 1);
    let Hk = query(packet, 2);
    let nt = query(packet, 3);
    let ns = query(packet, 4);
    let rq3 = query(packet, 5);

    if query(packet, 6) == 0 {
        return 0;
    }

    if Hk != 16 {
        return 0;
    }

    if H != 16 && H != 32 && H != 48 && H != 64 {
        return 0;
    }

    if nt < 64 {
        return 0;
    }

    if nt > 9223372036854775775 {
        return 0;
    }

    if rq3 <= 0 {
        return 0;
    }

    if ns <= 0 {
        return 0;
    }

    if ns > 2147483647 / (H * 4) {
        return 0;
    }
    // Enable only architectures benchmarked for this schedule; MMA support
    // alone does not imply a speedup.

    if query(packet, 12) == 0 {
        return 0;
    }
    let cc = query(packet, 8);

    if cc != cc_val(0) && cc != cc_val(1) && cc != cc_val(2) && cc != cc_val(3) {
        return 0;
    }
    let nsm = query(packet, 9);
    // Value splitting pays off when whole heads leave at least half the SMs idle.

    if H * ns * 2 <= nsm {
        // A grid that already fits in one wave benefits from a larger register budget.

        if H * ns * 4 <= nsm {
            return if ensure_ready(packet, device, 1) {
                1
            } else {
                0
            }
            ;
        }
        return if ensure_ready(packet, device, 2) {
            2
        } else {
            0
        }
        ;
    }
    return if ensure_ready(packet, device, 3) {
        3
    } else {
        0
    }
    ;
}

fn entry(packet: i64) -> bool {
    // Native checks state_out before any device validation.

    if query(packet, 7) == 0 {
        return false;
    }
    let device = query(packet, 0);
    let path = select_path(packet, device);

    if path == 0 {
        return false;
    }
    // CUDA_CHECK(cudaGetLastError()) after the raw launch: abort on any error status.

    if launch_raw(packet, path, shared_bytes(path)) != 0 {
        fail(2);
    }
    return true;
}

fn main() {
    entry(0);
}

