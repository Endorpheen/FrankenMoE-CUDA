// P1-10: admission and grouping decisions of the MoE device cache, in Goose.
//
// Native reference: llama-moe-cache.cpp, impl::impl. A layer is admitted when it has experts, all of them are host
// weights, and the layer's device is one of the cache's GPU devices. Admitted layers with the same device and the same
// expert tensor layout share one group (one set of banks and one SLRU). Host bytes are summed per group and per device.
//
// Everything here is a decision over flat arrays. Tensor construction, buffer allocation and the upload copies are
// primitives; the SLRU policy is already Goose and gated (EXP-150).

extern "fg262a_fail" fn fail(c:i64);
extern "fg262a_ld_i64" fn ld_i64(base:u64, byte:i64)->i64;
extern "fg262a_st_i64" fn st_i64(base:u64, byte:i64, v:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}
fn at(a:u64, i:i64)->i64 {return ld_i64(a, i*8 as i64);}

// inputs : n_experts[layer] (0 = not host-resident or no experts), device[layer] (-1 = unknown),
//          layout[layer] (0 = no layout), nbytes[layer]
// outputs: group[layer] (-1 = not admitted), group_dev[g], group_layout[g], group_bytes[g], group_count[g],
//          n_groups[1]
fn plan(n_layers:i64, n_experts:u64, device:u64, layout:u64, nbytes:u64,
        group:u64, group_dev:u64, group_layout:u64, group_bytes:u64, group_count:u64,
        n_groups:u64, max_groups:i64) -> bool {
    ensure(n_layers >= 0 as i64 && max_groups >= 0 as i64, 10);
    var g:i64 = 0 as i64;
    var il:i64 = 0 as i64;
    while il < n_layers {
        let ne:i64  = at(n_experts, il);
        let dev:i64 = at(device, il);
        let lay:i64 = at(layout, il);
        var found:i64 = -1 as i64;
        if ne > 0 as i64 && dev >= 0 as i64 && lay != 0 as i64 {
            var j:i64 = 0 as i64;
            while j < g {
                if at(group_dev, j) == dev && at(group_layout, j) == lay {
                    found = j;
                }
                j = j + 1 as i64;
            }
            if found < 0 as i64 {
                if g >= max_groups {return false;}
                found = g;
                st_i64(group_dev,    g*8 as i64, dev);
                st_i64(group_layout, g*8 as i64, lay);
                st_i64(group_bytes,  g*8 as i64, 0 as i64);
                st_i64(group_count,  g*8 as i64, 0 as i64);
                g = g + 1 as i64;
            }
            st_i64(group_bytes, found*8 as i64, at(group_bytes, found) + at(nbytes, il));
            st_i64(group_count, found*8 as i64, at(group_count, found) + 1 as i64);
        }
        st_i64(group, il*8 as i64, found);
        il = il + 1 as i64;
    }
    st_i64(n_groups, 0 as i64, g);
    true
}

fn main(){ plan(0 as i64,0 as u64,0 as u64,0 as u64,0 as u64,0 as u64,0 as u64,0 as u64,0 as u64,0 as u64,0 as u64,0 as i64); }
