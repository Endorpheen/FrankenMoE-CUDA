


static float i2m_load_half(uint64_t address) {
    float result;
    __asm__("cvt.f32.f16 %0, %1;" : "=f"(result) : "h"((uint16_t)i2m_load_u16(address)));
    return result;
}



#include "matrix-image-ptx-0.h"
#include "inkling-matrix-device.h"

#include "matrix-image-entries.h"
