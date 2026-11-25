// Prism v1 stores weight rows in a signed, block-normalized Hadamard basis.
export fn bonsai_signed_fwht(source: const f32[:], signs: const i32[:], output: f32[:],
                            inverse: bool, head_width: i64, key_heads: i64, repeat: i64) -> i64 {
    let width = source.len;
    let tile_width: i64 = 1024;
    if width == 0 || width % tile_width != 0 || signs.len != width || output.len != width { return -1; }
    let grouped = repeat > 1;
    if grouped && (inverse || head_width <= 0 || key_heads <= 0 || width % head_width != 0 ||
       width / head_width % key_heads != 0 || width / head_width / key_heads != repeat) { return -1; }
    for i in width {
        if signs[i] != 1 && signs[i] != -1 { return -1; }
    }
    for i in width {
        var at = i;
        if grouped {
            let head = i / head_width;
            at = (head % repeat * key_heads + head / repeat) * head_width + i % head_width;
        }
        output[i] = if inverse { source[at] } else { source[at] * (signs[i] as f32) };
    }
    for tile in width / tile_width {
        let base = tile * tile_width;
        var stride: i64 = 1;
        while stride < tile_width {
            var first = base;
            while first < base + tile_width {
                for j in stride {
                    let at = first + j;
                    let a = output[at];
                    let b = output[at + stride];
                    output[at] = a + b;
                    output[at + stride] = a - b;
                }
                first += 2 * stride;
            }
            stride *= 2;
        }
    }
    for i in width {
        let value = output[i] * 0.03125;
        output[i] = if inverse { value * (signs[i] as f32) } else { value };
    }
    return width;
}
