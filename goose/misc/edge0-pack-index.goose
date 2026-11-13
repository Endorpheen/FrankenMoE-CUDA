// Edge0 pack reader in Goose: strata pack index v3 and native experts v6.
//
// The loader will consume these tables, so the parsing and every validation
// decision are Goose. The C++ side is only the differential fixture that reads
// the files and compares the numbers.
//
// index_stats(text, out):
//   0 rows, 1 rows in the pool, 2 pool size, 3 alignment, 4 declared tensors,
//   5 max end offset, 6 misaligned rows, 7 rows outside the pool,
//   8 malformed rows, 9 total in-pool bytes.
// native_stats(text, out):
//   0 layers, 1 expert record size, 2 layer stride, 3 bad type ids,
//   4 bad offsets, 5 malformed rows, 6 first layer, 7 last layer.

fn is_sep(c: u8) -> bool {
    return c == 32 || c == 9 || c == 13;
}

fn field_i64(line: const u8[:], k: i64) -> i64 {
    var idx: i64 = 0;
    var i: i64 = 0;
    var start: i64 = 0;
    var len: i64 = 0;

    while i <= line.len {
        var c: u8 = 32;

        if i < line.len {
            c = line[i];
        }

        if is_sep(c) {
            if len > 0 {
                if idx == k {
                    var v: i64 = 0;
                    var j: i64 = start;
                    var neg: i64 = 0;

                    if line[j] == 45 {
                        neg = 1;
                        j = j + 1;
                    }

                    while j < start + len {
                        v = v * 10 + (line[j] as i64 - 48);
                        j = j + 1;
                    }

                    if neg != 0 {
                        return 0 - v;
                    }
                    return v;
                }
                idx = idx + 1;
                len = 0;
            }
        } else {
            if len == 0 {
                start = i;
            }
            len = len + 1;
        }
        i = i + 1;
    }
    return -1;
}

fn line_at(text: const u8[:], from: i64, next: i64[:]) -> const u8[:] {
    var i: i64 = from;

    while i < text.len && text[i] != 10 {
        i = i + 1;
    }
    var stop: i64 = i;

    if stop > from && text[stop - 1] == 13 {
        stop = stop - 1;
    }

    if i < text.len {
        next[0] = i + 1;
    } else {
        next[0] = -1;
    }
    return text[from .. stop];
}

export fn index_stats(text: u8[:], out: i64[:]) -> i64 {
    var z: i64 = 0;

    while z < 10 {
        out[z] = 0;
        z = z + 1;
    }
    var pos: i64 = 0;

    while pos >= 0 && pos <= text.len {
        var next: i64[1] = [0];
        let line = line_at(text, pos, next);

        pos = next[0];

        if line.len > 0 {
            if line[0] == 35 {
                var has_align: bool = false;
                var k: i64 = 0;

                while k + 5 <= line.len {
                    if line[k] == 97 && line[k + 1] == 108 && line[k + 2] == 105 && line[k + 3] == 103 && line[k + 4] == 110 {
                        has_align = true;
                    }
                    k = k + 1;
                }

                if has_align {
                    out[3] = field_i64(line, 2);
                    out[2] = field_i64(line, 4);
                    out[4] = field_i64(line, 6);
                }
            } else {
                out[0] = out[0] + 1;
                let off = field_i64(line, 3);
                let nb = field_i64(line, 4);

                if off < 0 || nb < 0 {
                    out[8] = out[8] + 1;
                } else if nb > 0 {
                    out[1] = out[1] + 1;
                    out[9] = out[9] + nb;
                    let end = off + nb;

                    if end > out[5] {
                        out[5] = end;
                    }

                    if out[3] > 0 && off % out[3] != 0 {
                        out[6] = out[6] + 1;
                    }

                    if end > out[2] {
                        out[7] = out[7] + 1;
                    }
                }
            }
        }
    }
    return 0;
}

export fn native_stats(text: u8[:], out: i64[:]) -> i64 {
    var z: i64 = 0;

    while z < 8 {
        out[z] = 0;
        z = z + 1;
    }
    var pos: i64 = 0;

    while pos >= 0 && pos <= text.len {
        var next: i64[1] = [0];
        let line = line_at(text, pos, next);

        pos = next[0];

        if line.len > 0 && line[0] != 35 {
            let layer = field_i64(line, 0);
            let gate = field_i64(line, 1);
            let down = field_i64(line, 2);
            let off = field_i64(line, 3);
            let size = field_i64(line, 4);

            if layer < 0 || gate < 0 || down < 0 || off < 0 || size < 0 {
                out[5] = out[5] + 1;
            } else {
                if out[0] == 0 {
                    out[6] = layer;
                    out[1] = size;
                    out[2] = 512 * size;
                }
                out[0] = out[0] + 1;
                out[7] = layer;

                if gate != 11 || down != 20 {
                    out[3] = out[3] + 1;
                }

                if off != layer * out[2] {
                    out[4] = out[4] + 1;
                }
            }
        }
    }
    return 0;
}
