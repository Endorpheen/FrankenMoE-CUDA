extern "giga_crt_setenv" fn giga_putenv(key: c_ptr<const i8>, value: c_ptr<const i8>) -> i32;

fn giga_enable_mmq() -> i32 {
    if giga_env_value("GOOSE_MMQ") != 0 { return 0; }
    var key: i8[10] = [71, 79, 79, 83, 69, 95, 77, 77, 81, 0];
    var value: i8[2] = [49, 0];
    return giga_putenv(unsafe_c_address<c_ptr<const i8>>(key[0]), unsafe_c_address<c_ptr<const i8>>(value[0]));
}
