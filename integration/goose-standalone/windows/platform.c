#include "platform.h"

#include <psapi.h>
#include <stdlib.h>

wchar_t *fg_win_to_wide(const char *utf8) {
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, NULL, 0);
    if (!count) return NULL;
    wchar_t *wide = malloc((size_t)count * sizeof(*wide));
    if (wide && !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, wide, count)) {
        free(wide);
        return NULL;
    }
    return wide;
}

char *fg_win_to_utf8(const wchar_t *wide) {
    const int count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide, -1, NULL, 0, NULL, NULL);
    if (!count) return NULL;
    char *text = malloc((size_t)count);
    if (text && !WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide, -1, text, count, NULL, NULL)) {
        free(text);
        return NULL;
    }
    return text;
}

void *fg_win_map_model(const char *path, size_t *bytes) {
    wchar_t *wide = fg_win_to_wide(path);
    if (!wide) return NULL;
    HANDLE file = CreateFileW(wide, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, NULL);
    free(wide);
    if (file == INVALID_HANDLE_VALUE) return NULL;
    LARGE_INTEGER size;
    if (GetFileType(file) != FILE_TYPE_DISK || !GetFileSizeEx(file, &size) ||
        size.QuadPart <= 0 || (uint64_t)size.QuadPart > SIZE_MAX) {
        CloseHandle(file);
        return NULL;
    }
    HANDLE mapping = CreateFileMappingW(file, NULL, PAGE_READONLY, 0, 0, NULL);
    CloseHandle(file);
    if (!mapping) return NULL;
    void *view = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, (size_t)size.QuadPart);
    CloseHandle(mapping);
    if (view) *bytes = (size_t)size.QuadPart;
    return view;
}

size_t fg_win_page_size(void) {
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return info.dwPageSize;
}

int fg_win_resident(const void *address, size_t bytes, unsigned char *pages) {
    const size_t page_size = fg_win_page_size();
    const size_t count = bytes / page_size + (bytes % page_size != 0);
    PSAPI_WORKING_SET_EX_INFORMATION entries[256];
    for (size_t at = 0; at < count;) {
        const size_t n = count - at < 256 ? count - at : 256;
        for (size_t i = 0; i < n; ++i) {
            entries[i].VirtualAddress = (void *)((uintptr_t)address + (at + i) * page_size);
            entries[i].VirtualAttributes.Flags = 0;
        }
        if (!QueryWorkingSetEx(GetCurrentProcess(), entries, (DWORD)(n * sizeof(*entries)))) return -1;
        for (size_t i = 0; i < n; ++i) pages[at + i] = entries[i].VirtualAttributes.Valid != 0;
        at += n;
    }
    return 0;
}

uint64_t fg_win_memory(bool available) {
    MEMORYSTATUSEX info = {.dwLength = sizeof(info)};
    if (!GlobalMemoryStatusEx(&info)) return 0;
    return available ? info.ullAvailPhys : info.ullTotalPhys;
}

bool fg_win_network_ready(void) {
    WSADATA data;
    return WSAStartup(MAKEWORD(2, 2), &data) == 0;
}

int fg_win_socket_timeout(int64_t fd) {
    const DWORD timeout = 30000;
    const SOCKET socket = (SOCKET)fd;
    if (setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof timeout) != 0) return -1;
    return setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout, sizeof timeout);
}

int sched_getaffinity(int pid, size_t bytes, cpu_set_t *set) {
    GROUP_AFFINITY affinity;
    if (pid || bytes < sizeof(*set) || !GetThreadGroupAffinity(GetCurrentThread(), &affinity)) return -1;
    set->mask = affinity.Mask;
    return 0;
}

int sched_setaffinity(int pid, size_t bytes, const cpu_set_t *set) {
    if (pid || bytes < sizeof(*set) || !set->mask) return -1;
    return SetThreadAffinityMask(GetCurrentThread(), set->mask) ? 0 : -1;
}

int sched_getcpu(void) {
    return (int)GetCurrentProcessorNumber();
}
