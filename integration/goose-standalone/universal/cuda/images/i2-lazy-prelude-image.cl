#include "i2-lazy-prelude-image-ptx-0.h"
static uint64_t depth_bits(double value) { return as_ulong(value); }
static double depth_value(uint64_t bits) { return as_double(bits); }

#include "i2-lazy-prelude-image-ptx-1.h"
static uint lazy_float_bits(float value) { return as_uint(value); }

#include "i2-lazy-prelude-device.h"

#include "i2-lazy-prelude-image-entries.h"
