# EXP323: IQ2_S decode in Goose, byte-exact

Layout pinned with `offsetof`: `block_iq2_s` is 82 bytes with `d` at 0, `qs[64]` at 2, `qh[8]` at 66 and `scales[8]` at 74. The
executed `dequantize_row_iq2_s` is scalar; the grid comes from the host through an extern.

Structure: `qs` splits in half -- codes at `qs[0..31]`, sign bytes at `qs[32..63]` (`signs = qs + QK_K/8`) -- and each group
consumes four code bytes, four sign bytes and one scale byte whose low nibble drives `l < 2` and high nibble `l >= 2`. The
eight-bit code is widened by two bits sliced from `qh[ib]` with a per-`l` shift, which is the part most easily dropped.

## Gate

    EXP323_IQ2S checks=7424 failures=0

Bit-pattern comparison against the real `dequantize_row_iq2_s`, sizes 256/512/2560/4096, last block fully zeroed.

Mutation, and a note on it: the first attempt at the mutation did not apply (the edit pattern did not match), and the gate
correctly stayed green -- so it proved nothing. The applied mutation, narrowing the `qh` widening mask from `0x300` to `0x100`,
produces 2626 divergences out of 7424. The lesson is the one already in the notes: prove the mutation changed the generated code
before quoting the result.
