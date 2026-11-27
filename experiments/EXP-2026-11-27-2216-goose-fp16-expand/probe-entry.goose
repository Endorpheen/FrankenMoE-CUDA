export fn expand_row(input: const u16[:], output: f32[:]) {
    for i in input.len { output[i] = expand_half(input[i]); }
}

extern "fg143_half" fn ggml_half(x: u16) -> f32;

export fn reference_row(input: const u16[:], output: f32[:]) {
    for i in input.len { output[i] = ggml_half(input[i]); }
}
