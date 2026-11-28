#include "platform.h"
#include <psapi.h>

_Static_assert(sizeof(void *) == 8 && sizeof(wchar_t) == 2 &&
    sizeof(SYSTEM_INFO) == 48 && offsetof(SYSTEM_INFO, dwPageSize) == 4 &&
    sizeof(MEMORYSTATUSEX) == 64 && offsetof(MEMORYSTATUSEX, ullTotalPhys) == 8 &&
    offsetof(MEMORYSTATUSEX, ullAvailPhys) == 16 && sizeof(WSADATA) == 408 &&
    sizeof(GROUP_AFFINITY) == 16 && offsetof(GROUP_AFFINITY, Mask) == 0 &&
    sizeof(cpu_set_t) == 8 && sizeof(PSAPI_WORKING_SET_EX_INFORMATION) == 16 &&
    offsetof(PSAPI_WORKING_SET_EX_INFORMATION, VirtualAddress) == 0 &&
    offsetof(PSAPI_WORKING_SET_EX_INFORMATION, VirtualAttributes) == 8 &&
    SOL_SOCKET == 65535 && SO_RCVTIMEO == 0x1006 && SO_SNDTIMEO == 0x1005 &&
    CP_UTF8 == 65001 && MB_ERR_INVALID_CHARS == 8 && WC_ERR_INVALID_CHARS == 128,
    "Goose Windows platform ABI");
