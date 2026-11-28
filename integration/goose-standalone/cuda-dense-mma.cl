typedef struct { float a_g, b_g, c_g, d_g; } DenseDot_g;
typedef struct { uint32_t hi_g, lo_g; } DenseParts_g;


#include "cuda-dense-mma-ptx-0.h"
#include "cuda-dense-mma-matrix-ptx-0.h"
#include "dense-mma-device.h"

#include "cuda-dense-mma-entries.h"
