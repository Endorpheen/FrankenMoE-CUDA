fn device_range_valid(value: i64, lower: i64, upper: i64) -> bool {
    return value >= lower && value <= upper;
}

fn device_unsigned_range_valid(value: u64, upper: u64) -> bool {
    return value <= upper;
}

fn device_index_valid(index: i64, length: i64) -> bool {
    return (index as u64) < (length as u64);
}

fn device_unsigned_divisor_valid(divisor: u64) -> bool {
    return divisor != (0 as u64);
}

fn device_nonnegative_division_valid(numerator: i64, divisor: i64) -> bool {
    return numerator >= (0 as i64) && divisor > (0 as i64);
}

fn device_signed_division_valid(numerator: i64, divisor: i64) -> bool {
    let minimum = (-9223372036854775807 as i64) - (1 as i64);
    return divisor != (0 as i64) && !(numerator == minimum && divisor == (-1 as i64));
}

fn device_euclidean_mod(numerator: i64, divisor: i64) -> i64 {
    if divisor == (-1 as i64) { return 0 as i64; }
    let remainder = numerator % divisor;
    if remainder >= (0 as i64) { return remainder; }
    var modulus = divisor as u64;
    if divisor < (0 as i64) { modulus = (0 as u64) - modulus; }
    return ((remainder as u64) + modulus) as i64;
}
